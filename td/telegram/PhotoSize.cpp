//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2022
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/PhotoSize.h"

#include "td/telegram/files/FileLocation.h"
#include "td/telegram/files/FileManager.h"

#include "td/utils/base64.h"
#include "td/utils/HttpUrl.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/Random.h"
#include "td/utils/SliceBuilder.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace td {

static uint16 get_dimension(int32 size, const char *source) {
  if (size < 0 || size > 65535) {
    LOG(ERROR) << "Wrong image dimension = " << size << " from " << source;
    return 0;
  }
  return narrow_cast<uint16>(size);
}

Dimensions get_dimensions(int32 width, int32 height, const char *source) {
  Dimensions result;
  result.width = get_dimension(width, source);
  result.height = get_dimension(height, source);
  if (result.width == 0 || result.height == 0) {
    result.width = 0;
    result.height = 0;
  }
  return result;
}

static uint32 get_pixel_count(const Dimensions &dimensions) {
  return static_cast<uint32>(dimensions.width) * static_cast<uint32>(dimensions.height);
}

bool operator==(const Dimensions &lhs, const Dimensions &rhs) {
  return lhs.width == rhs.width && lhs.height == rhs.height;
}

bool operator!=(const Dimensions &lhs, const Dimensions &rhs) {
  return !(lhs == rhs);
}

StringBuilder &operator<<(StringBuilder &string_builder, const Dimensions &dimensions) {
  return string_builder << "(" << dimensions.width << ", " << dimensions.height << ")";
}

td_api::object_ptr<td_api::minithumbnail> get_minithumbnail_object(const string &packed) {
  if (packed.size() < 3) {
    return nullptr;
  }
  if (packed[0] == '\x01') {
    static const string header =
        base64_decode(
            "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDACgcHiMeGSgjISMtKygwPGRBPDc3PHtYXUlkkYCZlo+AjIqgtObDoKrarYqMyP/L2u71////"
            "m8H///"
            "/6/+b9//j/2wBDASstLTw1PHZBQXb4pYyl+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj4+Pj/"
            "wAARCAAAAAADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/"
            "8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0R"
            "FRkd"
            "ISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2"
            "uHi4"
            "+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/"
            "8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkN"
            "ERUZ"
            "HSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2"
            "Nna4"
            "uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwA=")
            .move_as_ok();
    static const string footer = base64_decode("/9k=").move_as_ok();
    auto result = td_api::make_object<td_api::minithumbnail>();
    result->height_ = static_cast<unsigned char>(packed[1]);
    result->width_ = static_cast<unsigned char>(packed[2]);
    result->data_ = PSTRING() << header.substr(0, 164) << packed[1] << header[165] << packed[2] << header.substr(167)
                              << packed.substr(3) << footer;
    return result;
  }
  return nullptr;
}

static td_api::object_ptr<td_api::ThumbnailFormat> get_thumbnail_format_object(PhotoFormat format) {
  switch (format) {
    case PhotoFormat::Jpeg:
      return td_api::make_object<td_api::thumbnailFormatJpeg>();
    case PhotoFormat::Png:
      return td_api::make_object<td_api::thumbnailFormatPng>();
    case PhotoFormat::Webp:
      return td_api::make_object<td_api::thumbnailFormatWebp>();
    case PhotoFormat::Gif:
      return td_api::make_object<td_api::thumbnailFormatGif>();
    case PhotoFormat::Tgs:
      return td_api::make_object<td_api::thumbnailFormatTgs>();
    case PhotoFormat::Mpeg4:
      return td_api::make_object<td_api::thumbnailFormatMpeg4>();
    case PhotoFormat::Webm:
      return td_api::make_object<td_api::thumbnailFormatWebm>();
    default:
      UNREACHABLE();
      return nullptr;
  }
}

static StringBuilder &operator<<(StringBuilder &string_builder, PhotoFormat format) {
  switch (format) {
    case PhotoFormat::Jpeg:
      return string_builder << "jpg";
    case PhotoFormat::Png:
      return string_builder << "png";
    case PhotoFormat::Webp:
      return string_builder << "webp";
    case PhotoFormat::Gif:
      return string_builder << "gif";
    case PhotoFormat::Tgs:
      return string_builder << "tgs";
    case PhotoFormat::Mpeg4:
      return string_builder << "mp4";
    case PhotoFormat::Webm:
      return string_builder << "webm";
    default:
      UNREACHABLE();
      return string_builder;
  }
}

FileId register_photo_size(FileManager *file_manager, const PhotoSizeSource &source, int64 id, int64 access_hash,
                           string file_reference, DialogId owner_dialog_id, int32 file_size, DcId dc_id,
                           PhotoFormat format) {
  LOG(DEBUG) << "Receive " << format << " photo " << id << " of type " << source.get_file_type("register_photo_size")
             << " from " << dc_id;
  auto suggested_name = PSTRING() << source.get_unique_name(id) << '.' << format;
  auto file_location_source = owner_dialog_id.get_type() == DialogType::SecretChat ? FileLocationSource::FromUser
                                                                                   : FileLocationSource::FromServer;
  return file_manager->register_remote(
      FullRemoteFileLocation(source, id, access_hash, dc_id, std::move(file_reference)), file_location_source,
      owner_dialog_id, file_size, 0, std::move(suggested_name));
}

PhotoSize get_secret_thumbnail_photo_size(FileManager *file_manager, BufferSlice bytes, DialogId owner_dialog_id,
                                          int32 width, int32 height) {
  if (bytes.empty()) {
    return PhotoSize();
  }
  PhotoSize res;
  res.type = 't';
  res.dimensions = get_dimensions(width, height, "get_secret_thumbnail_photo_size");
  res.size = narrow_cast<int32>(bytes.size());

  // generate some random remote location to save
  auto dc_id = DcId::invalid();
  auto photo_id = -(Random::secure_int64() & std::numeric_limits<int64>::max());

  res.file_id = file_manager->register_remote(
      FullRemoteFileLocation(PhotoSizeSource::thumbnail(FileType::EncryptedThumbnail, 't'), photo_id, 0, dc_id,
                             string()),
      FileLocationSource::FromServer, owner_dialog_id, res.size, 0,
      PSTRING() << static_cast<uint64>(photo_id) << ".jpg");
  file_manager->set_content(res.file_id, std::move(bytes));

  return res;
}

Variant<PhotoSize, string> get_photo_size(FileManager *file_manager, PhotoSizeSource source, int64 id,
                                          int64 access_hash, std::string file_reference, DcId dc_id,
                                          DialogId owner_dialog_id, tl_object_ptr<telegram_api::PhotoSize> &&size_ptr,
                                          PhotoFormat format) {
  CHECK(size_ptr != nullptr);

  string type;
  PhotoSize res;
  BufferSlice content;
  switch (size_ptr->get_id()) {
    case telegram_api::photoSizeEmpty::ID:
      return std::move(res);
    case telegram_api::photoSize::ID: {
      auto size = move_tl_object_as<telegram_api::photoSize>(size_ptr);

      type = std::move(size->type_);
      res.dimensions = get_dimensions(size->w_, size->h_, "photoSize");
      res.size = size->size_;

      break;
    }
    case telegram_api::photoCachedSize::ID: {
      auto size = move_tl_object_as<telegram_api::photoCachedSize>(size_ptr);

      type = std::move(size->type_);
      CHECK(size->bytes_.size() <= static_cast<size_t>(std::numeric_limits<int32>::max()));
      res.dimensions = get_dimensions(size->w_, size->h_, "photoCachedSize");
      res.size = static_cast<int32>(size->bytes_.size());

      content = std::move(size->bytes_);

      break;
    }
    case telegram_api::photoStrippedSize::ID: {
      auto size = move_tl_object_as<telegram_api::photoStrippedSize>(size_ptr);
      if (format != PhotoFormat::Jpeg) {
        LOG(ERROR) << "Receive unexpected JPEG minithumbnail in photo " << id << " from " << source << " of format "
                   << format;
        return std::move(res);
      }
      return size->bytes_.as_slice().str();
    }
    case telegram_api::photoSizeProgressive::ID: {
      auto size = move_tl_object_as<telegram_api::photoSizeProgressive>(size_ptr);

      if (size->sizes_.empty()) {
        LOG(ERROR) << "Receive photo " << id << " from " << source << " with empty size " << to_string(size);
        return std::move(res);
      }
      std::sort(size->sizes_.begin(), size->sizes_.end());

      type = std::move(size->type_);
      res.dimensions = get_dimensions(size->w_, size->h_, "photoSizeProgressive");
      res.size = size->sizes_.back();
      size->sizes_.pop_back();
      res.progressive_sizes = std::move(size->sizes_);

      break;
    }
    case telegram_api::photoPathSize::ID: {
      auto size = move_tl_object_as<telegram_api::photoPathSize>(size_ptr);
      if (format != PhotoFormat::Tgs && format != PhotoFormat::Webp && format != PhotoFormat::Webm) {
        LOG(ERROR) << "Receive unexpected SVG minithumbnail in photo " << id << " from " << source << " of format "
                   << format;
        return std::move(res);
      }
      return size->bytes_.as_slice().str();
    }
    default:
      UNREACHABLE();
      break;
  }

  if (type.size() != 1) {
    LOG(ERROR) << "Wrong photoSize \"" << type << "\" " << res;
    res.type = 0;
  } else {
    res.type = static_cast<uint8>(type[0]);
    if (res.type >= 128) {
      LOG(ERROR) << "Wrong photoSize \"" << type << "\" " << res;
      res.type = 0;
    }
  }
  if (source.get_type("get_photo_size") == PhotoSizeSource::Type::Thumbnail) {
    source.thumbnail().thumbnail_type = res.type;
  }

  res.file_id = register_photo_size(file_manager, source, id, access_hash, std::move(file_reference), owner_dialog_id,
                                    res.size, dc_id, format);

  if (!content.empty()) {
    file_manager->set_content(res.file_id, std::move(content));
  }

  return std::move(res);
}

AnimationSize get_animation_size(FileManager *file_manager, PhotoSizeSource source, int64 id, int64 access_hash,
                                 std::string file_reference, DcId dc_id, DialogId owner_dialog_id,
                                 tl_object_ptr<telegram_api::videoSize> &&size) {
  CHECK(size != nullptr);
  AnimationSize res;
  if (size->type_ != "v" && size->type_ != "u") {
    LOG(ERROR) << "Wrong videoSize \"" << size->type_ << "\" in " << to_string(size);
  }
  res.type = static_cast<uint8>(size->type_[0]);
  if (res.type >= 128) {
    LOG(ERROR) << "Wrong videoSize \"" << res.type << "\" " << res;
    res.type = 0;
  }
  res.dimensions = get_dimensions(size->w_, size->h_, "get_animation_size");
  res.size = size->size_;
  if ((size->flags_ & telegram_api::videoSize::VIDEO_START_TS_MASK) != 0) {
    res.main_frame_timestamp = size->video_start_ts_;
  }

  if (source.get_type("get_animation_size") == PhotoSizeSource::Type::Thumbnail) {
    source.thumbnail().thumbnail_type = res.type;
  }

  res.file_id = register_photo_size(file_manager, source, id, access_hash, std::move(file_reference), owner_dialog_id,
                                    res.size, dc_id, PhotoFormat::Mpeg4);
  return res;
}

PhotoSize get_web_document_photo_size(FileManager *file_manager, FileType file_type, DialogId owner_dialog_id,
                                      tl_object_ptr<telegram_api::WebDocument> web_document_ptr) {
  if (web_document_ptr == nullptr) {
    return {};
  }

  FileId file_id;
  vector<tl_object_ptr<telegram_api::DocumentAttribute>> attributes;
  int32 size = 0;
  string mime_type;
  switch (web_document_ptr->get_id()) {
    case telegram_api::webDocument::ID: {
      auto web_document = move_tl_object_as<telegram_api::webDocument>(web_document_ptr);
      auto r_http_url = parse_url(web_document->url_);
      if (r_http_url.is_error()) {
        LOG(ERROR) << "Can't parse URL " << web_document->url_;
        return {};
      }
      auto http_url = r_http_url.move_as_ok();
      auto url = http_url.get_url();
      file_id = file_manager->register_remote(FullRemoteFileLocation(file_type, url, web_document->access_hash_),
                                              FileLocationSource::FromServer, owner_dialog_id, 0, web_document->size_,
                                              get_url_query_file_name(http_url.query_));
      size = web_document->size_;
      mime_type = std::move(web_document->mime_type_);
      attributes = std::move(web_document->attributes_);
      break;
    }
    case telegram_api::webDocumentNoProxy::ID: {
      auto web_document = move_tl_object_as<telegram_api::webDocumentNoProxy>(web_document_ptr);
      if (web_document->url_.find('.') == string::npos) {
        LOG(ERROR) << "Receive invalid URL " << web_document->url_;
        return {};
      }

      auto r_file_id = file_manager->from_persistent_id(web_document->url_, file_type);
      if (r_file_id.is_error()) {
        LOG(ERROR) << "Can't register URL: " << r_file_id.error();
        return {};
      }
      file_id = r_file_id.move_as_ok();

      size = web_document->size_;
      mime_type = std::move(web_document->mime_type_);
      attributes = std::move(web_document->attributes_);
      break;
    }
    default:
      UNREACHABLE();
  }
  CHECK(file_id.is_valid());
  bool is_animation = mime_type == "video/mp4";
  bool is_gif = mime_type == "image/gif";

  Dimensions dimensions;
  for (auto &attribute : attributes) {
    switch (attribute->get_id()) {
      case telegram_api::documentAttributeImageSize::ID: {
        auto image_size = move_tl_object_as<telegram_api::documentAttributeImageSize>(attribute);
        dimensions = get_dimensions(image_size->w_, image_size->h_, "web documentAttributeImageSize");
        break;
      }
      case telegram_api::documentAttributeAnimated::ID:
      case telegram_api::documentAttributeHasStickers::ID:
      case telegram_api::documentAttributeSticker::ID:
      case telegram_api::documentAttributeVideo::ID:
      case telegram_api::documentAttributeAudio::ID:
        LOG(ERROR) << "Unexpected web document attribute " << to_string(attribute);
        break;
      case telegram_api::documentAttributeFilename::ID:
        break;
      default:
        UNREACHABLE();
    }
  }

  PhotoSize s;
  s.type = is_animation ? 'v' : (is_gif ? 'g' : (file_type == FileType::Thumbnail ? 't' : 'n'));
  s.dimensions = dimensions;
  s.size = size;
  s.file_id = file_id;
  return s;
}

td_api::object_ptr<td_api::thumbnail> get_thumbnail_object(FileManager *file_manager, const PhotoSize &photo_size,
                                                           PhotoFormat format) {
  if (!photo_size.file_id.is_valid()) {
    return nullptr;
  }

  if (format == PhotoFormat::Jpeg && photo_size.type == 'g') {
    format = PhotoFormat::Gif;
  }

  return td_api::make_object<td_api::thumbnail>(get_thumbnail_format_object(format), photo_size.dimensions.width,
                                                photo_size.dimensions.height,
                                                file_manager->get_file_object(photo_size.file_id));
}

bool operator==(const PhotoSize &lhs, const PhotoSize &rhs) {
  return lhs.type == rhs.type && lhs.dimensions == rhs.dimensions && lhs.size == rhs.size &&
         lhs.file_id == rhs.file_id && lhs.progressive_sizes == rhs.progressive_sizes;
}

bool operator!=(const PhotoSize &lhs, const PhotoSize &rhs) {
  return !(lhs == rhs);
}

bool operator<(const PhotoSize &lhs, const PhotoSize &rhs) {
  if (lhs.size != rhs.size) {
    return lhs.size < rhs.size;
  }
  auto lhs_pixels = get_pixel_count(lhs.dimensions);
  auto rhs_pixels = get_pixel_count(rhs.dimensions);
  if (lhs_pixels != rhs_pixels) {
    return lhs_pixels < rhs_pixels;
  }
  int32 lhs_type = lhs.type == 't' ? -1 : lhs.type;
  int32 rhs_type = rhs.type == 't' ? -1 : rhs.type;
  if (lhs_type != rhs_type) {
    return lhs_type < rhs_type;
  }
  if (lhs.file_id != rhs.file_id) {
    return lhs.file_id.get() < rhs.file_id.get();
  }
  return lhs.dimensions.width < rhs.dimensions.width;
}

StringBuilder &operator<<(StringBuilder &string_builder, const PhotoSize &photo_size) {
  return string_builder << "{type = " << photo_size.type << ", dimensions = " << photo_size.dimensions
                        << ", size = " << photo_size.size << ", file_id = " << photo_size.file_id
                        << ", progressive_sizes = " << photo_size.progressive_sizes << "}";
}

bool operator==(const AnimationSize &lhs, const AnimationSize &rhs) {
  return static_cast<const PhotoSize &>(lhs) == static_cast<const PhotoSize &>(rhs) &&
         fabs(lhs.main_frame_timestamp - rhs.main_frame_timestamp) < 1e-3;
}

bool operator!=(const AnimationSize &lhs, const AnimationSize &rhs) {
  return !(lhs == rhs);
}

StringBuilder &operator<<(StringBuilder &string_builder, const AnimationSize &animation_size) {
  return string_builder << static_cast<const PhotoSize &>(animation_size) << " from "
                        << animation_size.main_frame_timestamp;
}

}  // namespace td
