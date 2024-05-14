// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace search_provider_logos {

namespace {

// The cached logo metadata is persisted as JSON using these keys.
const char kSourceUrlKey[] = "url";
const char kExpirationTimeKey[] = "expiration_time";
const char kCanShowAfterExpirationKey[] = "can_show_after_expiration";
const char kFingerprintKey[] = "fingerprint";
const char kTypeKey[] = "type";
const char kOnClickURLKey[] = "on_click_url";
const char kFullPageURLKey[] = "full_page_url";
const char kAltTextKey[] = "alt_text";
const char kMimeTypeKey[] = "mime_type";
const char kDarkMimeTypeKey[] = "dark_mime_type";
const char kNumBytesKey[] = "num_bytes";
const char kDarkNumBytesKey[] = "dark_num_bytes";
const char kAnimatedUrlKey[] = "animated_url";
const char kDarkAnimatedUrlKey[] = "dark_animated_url";
const char kLogUrlKey[] = "log_url";
const char kDarkLogUrlKey[] = "dark_log_url";
const char kCtaLogUrlKey[] = "cta_log_url";
const char kDarkCtaLogUrlKey[] = "dark_cta_log_url";
const char kShortLinkKey[] = "short_link";
const char kWidthPx[] = "width_px";
const char kHeightPx[] = "height_px";
const char kDarkWidthPx[] = "dark_width_px";
const char kDarkHeightPx[] = "dark_height_px";
const char kIframeWidthPx[] = "iframe_width_px";
const char kIframeHeightPx[] = "iframe_height_px";
const char kDarkBackgroundColorKey[] = "dark_background_color";

const char kShareButtonX[] = "share_button_x";
const char kShareButtonY[] = "share_button_y";
const char kShareButtonOpacity[] = "share_button_opacity";
const char kShareButtonIcon[] = "share_button_icon";
const char kShareButtonBg[] = "share_button_bg";
const char kDarkShareButtonX[] = "dark_share_button_x";
const char kDarkShareButtonY[] = "dark_share_button_y";
const char kDarkShareButtonOpacity[] = "dark_share_button_opacity";
const char kDarkShareButtonIcon[] = "dark_share_button_icon";
const char kDarkShareButtonBg[] = "dark_share_button_bg";

const char kSimpleType[] = "SIMPLE";
const char kAnimatedType[] = "ANIMATED";
const char kInteractiveType[] = "INTERACTIVE";

bool GetTimeValue(const base::Value::Dict& dict,
                  const std::string& key,
                  base::Time* time) {
  const std::string* str = dict.FindString(key);
  int64_t internal_time_value;
  if (str && base::StringToInt64(*str, &internal_time_value)) {
    *time = base::Time::FromInternalValue(internal_time_value);
    return true;
  }
  return false;
}

void SetTimeValue(base::Value::Dict& dict,
                  const std::string& key,
                  const base::Time& time) {
  int64_t internal_time_value = time.ToInternalValue();
  dict.Set(key, base::NumberToString(internal_time_value));
}

LogoType LogoTypeFromString(std::string_view type) {
  if (type == kSimpleType) {
    return LogoType::SIMPLE;
  }
  if (type == kAnimatedType) {
    return LogoType::ANIMATED;
  }
  if (type == kInteractiveType) {
    return LogoType::INTERACTIVE;
  }
  LOG(WARNING) << "invalid type " << type;
  return LogoType::SIMPLE;
}

std::string LogoTypeToString(LogoType type) {
  switch (type) {
    case LogoType::SIMPLE:
      return kSimpleType;
    case LogoType::ANIMATED:
      return kAnimatedType;
    case LogoType::INTERACTIVE:
      return kInteractiveType;
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace

LogoCache::LogoCache(const base::FilePath& cache_directory)
    : cache_directory_(cache_directory),
      metadata_is_valid_(false) {
  // The LogoCache can be constructed on any thread, as long as it's used
  // on a single sequence after construction.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

LogoCache::~LogoCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LogoCache::UpdateCachedLogoMetadata(const LogoMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_);
  DCHECK_EQ(metadata_->fingerprint, metadata.fingerprint);

  UpdateMetadata(std::make_unique<LogoMetadata>(metadata));
  WriteMetadata();
}

const LogoMetadata* LogoCache::GetCachedLogoMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReadMetadataIfNeeded();
  return metadata_.get();
}

void LogoCache::SetCachedLogo(const EncodedLogo* logo) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!logo) {
    UpdateMetadata(nullptr);
    DeleteLogoAndMetadata();
    return;
  }

  logo_num_bytes_ =
      logo->encoded_image ? static_cast<int>(logo->encoded_image->size()) : 0;
  dark_logo_num_bytes_ =
      logo->dark_encoded_image
          ? static_cast<int>(logo->dark_encoded_image->size())
          : 0;
  UpdateMetadata(std::make_unique<LogoMetadata>(logo->metadata));
  WriteLogo(logo->encoded_image, logo->dark_encoded_image);
}

std::unique_ptr<EncodedLogo> LogoCache::GetCachedLogo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReadMetadataIfNeeded();
  if (!metadata_)
    return nullptr;

  base::FilePath logo_path = GetLogoPath();
  base::FilePath dark_logo_path = GetDarkLogoPath();
  scoped_refptr<base::RefCountedString> encoded_image;
  if (logo_num_bytes_ != 0) {
    encoded_image = new base::RefCountedString();

    if (!base::ReadFileToString(logo_path, &encoded_image->as_string())) {
      UpdateMetadata(nullptr);
      return nullptr;
    }

    if (encoded_image->size() != static_cast<size_t>(logo_num_bytes_)) {
      // Delete corrupt metadata and logo.
      DeleteLogoAndMetadata();
      UpdateMetadata(nullptr);
      return nullptr;
    }
  }

  scoped_refptr<base::RefCountedString> dark_encoded_image;
  if (dark_logo_num_bytes_ != 0) {
    dark_encoded_image = new base::RefCountedString();

    if (!base::ReadFileToString(dark_logo_path,
                                &dark_encoded_image->as_string())) {
      UpdateMetadata(nullptr);
      return nullptr;
    }

    if (dark_encoded_image->size() !=
        static_cast<size_t>(dark_logo_num_bytes_)) {
      // Delete corrupt metadata and logo.
      DeleteLogoAndMetadata();
      UpdateMetadata(nullptr);
      return nullptr;
    }
  }

  std::unique_ptr<EncodedLogo> logo(new EncodedLogo());
  logo->encoded_image = encoded_image;
  logo->dark_encoded_image = dark_encoded_image;
  logo->metadata = *metadata_;
  return logo;
}

// static
std::unique_ptr<LogoMetadata> LogoCache::LogoMetadataFromString(
    const std::string& str,
    int* logo_num_bytes,
    int* dark_logo_num_bytes) {
  std::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value) {
    return nullptr;
  }
  const base::Value::Dict* dict = value->GetIfDict();
  if (!dict) {
    return nullptr;
  }

  // These helpers replace the deprecated analogous methods on
  // base::DictionaryValue, so as to maintain the early exit behavior in the if
  // predicate below.
  auto get_string = [dict](const char* key, std::string* ret) -> bool {
    const std::string* v = dict->FindString(key);
    if (v)
      *ret = *v;
    return v != nullptr;
  };
  auto get_boolean = [dict](const char* key, bool* ret) -> bool {
    std::optional<bool> v = dict->FindBool(key);
    if (v.has_value())
      *ret = v.value();
    return v.has_value();
  };
  auto get_integer = [dict](const char* key, int* ret) -> bool {
    std::optional<int> v = dict->FindInt(key);
    if (v.has_value())
      *ret = v.value();
    return v.has_value();
  };
  auto get_double = [dict](const char* key, double* ret) -> bool {
    std::optional<double> v = dict->FindDouble(key);
    if (v.has_value())
      *ret = v.value();
    return v.has_value();
  };

  std::unique_ptr<LogoMetadata> metadata(new LogoMetadata());
  std::string source_url;
  std::string type;
  std::string on_click_url;
  std::string full_page_url;
  std::string animated_url;
  std::string dark_animated_url;
  std::string log_url;
  std::string dark_log_url;
  std::string cta_log_url;
  std::string dark_cta_log_url;
  std::string short_link;
  if (!get_string(kSourceUrlKey, &source_url) ||
      !get_string(kFingerprintKey, &metadata->fingerprint) ||
      !get_string(kTypeKey, &type) ||
      !get_string(kOnClickURLKey, &on_click_url) ||
      !get_string(kFullPageURLKey, &full_page_url) ||
      !get_string(kAltTextKey, &metadata->alt_text) ||
      !get_string(kAnimatedUrlKey, &animated_url) ||
      !get_string(kDarkAnimatedUrlKey, &dark_animated_url) ||
      !get_string(kLogUrlKey, &log_url) ||
      !get_string(kDarkLogUrlKey, &dark_log_url) ||
      !get_string(kCtaLogUrlKey, &cta_log_url) ||
      !get_string(kDarkCtaLogUrlKey, &dark_cta_log_url) ||
      !get_string(kShortLinkKey, &short_link) ||
      !get_string(kMimeTypeKey, &metadata->mime_type) ||
      !get_string(kDarkMimeTypeKey, &metadata->dark_mime_type) ||
      !get_boolean(kCanShowAfterExpirationKey,
                   &metadata->can_show_after_expiration) ||
      !get_integer(kNumBytesKey, logo_num_bytes) ||
      !get_integer(kDarkNumBytesKey, dark_logo_num_bytes) ||
      !get_integer(kShareButtonX, &metadata->share_button_x) ||
      !get_integer(kShareButtonY, &metadata->share_button_y) ||
      !get_double(kShareButtonOpacity, &metadata->share_button_opacity) ||
      !get_string(kShareButtonIcon, &metadata->share_button_icon) ||
      !get_string(kShareButtonBg, &metadata->share_button_bg) ||
      !get_integer(kDarkShareButtonX, &metadata->dark_share_button_x) ||
      !get_integer(kDarkShareButtonY, &metadata->dark_share_button_y) ||
      !get_double(kDarkShareButtonOpacity,
                  &metadata->dark_share_button_opacity) ||
      !get_string(kDarkShareButtonIcon, &metadata->dark_share_button_icon) ||
      !get_string(kDarkShareButtonBg, &metadata->dark_share_button_bg) ||
      !get_integer(kWidthPx, &metadata->width_px) ||
      !get_integer(kHeightPx, &metadata->height_px) ||
      !get_integer(kDarkWidthPx, &metadata->dark_width_px) ||
      !get_integer(kDarkHeightPx, &metadata->dark_height_px) ||
      !get_integer(kIframeWidthPx, &metadata->iframe_width_px) ||
      !get_integer(kIframeHeightPx, &metadata->iframe_height_px) ||
      !get_string(kDarkBackgroundColorKey, &metadata->dark_background_color) ||
      !GetTimeValue(*dict, kExpirationTimeKey, &metadata->expiration_time)) {
    return nullptr;
  }
  metadata->type = LogoTypeFromString(type);
  metadata->source_url = GURL(source_url);
  metadata->on_click_url = GURL(on_click_url);
  metadata->full_page_url = GURL(full_page_url);
  metadata->animated_url = GURL(animated_url);
  metadata->dark_animated_url = GURL(dark_animated_url);
  metadata->log_url = GURL(log_url);
  metadata->dark_log_url = GURL(dark_log_url);
  metadata->cta_log_url = GURL(cta_log_url);
  metadata->dark_cta_log_url = GURL(dark_cta_log_url);
  metadata->short_link = GURL(short_link);

  return metadata;
}

// static
void LogoCache::LogoMetadataToString(const LogoMetadata& metadata,
                                     int num_bytes,
                                     int dark_num_bytes,
                                     std::string* str) {
  base::Value::Dict dict;
  dict.Set(kSourceUrlKey, metadata.source_url.spec());
  dict.Set(kFingerprintKey, metadata.fingerprint);
  dict.Set(kTypeKey, LogoTypeToString(metadata.type));
  dict.Set(kOnClickURLKey, metadata.on_click_url.spec());
  dict.Set(kFullPageURLKey, metadata.full_page_url.spec());
  dict.Set(kAltTextKey, metadata.alt_text);
  dict.Set(kAnimatedUrlKey, metadata.animated_url.spec());
  dict.Set(kDarkAnimatedUrlKey, metadata.dark_animated_url.spec());
  dict.Set(kLogUrlKey, metadata.log_url.spec());
  dict.Set(kDarkLogUrlKey, metadata.dark_log_url.spec());
  dict.Set(kCtaLogUrlKey, metadata.cta_log_url.spec());
  dict.Set(kDarkCtaLogUrlKey, metadata.dark_cta_log_url.spec());
  dict.Set(kShortLinkKey, metadata.short_link.spec());
  dict.Set(kMimeTypeKey, metadata.mime_type);
  dict.Set(kDarkMimeTypeKey, metadata.dark_mime_type);
  dict.Set(kCanShowAfterExpirationKey, metadata.can_show_after_expiration);
  dict.Set(kNumBytesKey, num_bytes);
  dict.Set(kDarkNumBytesKey, dark_num_bytes);
  dict.Set(kShareButtonX, metadata.share_button_x);
  dict.Set(kShareButtonY, metadata.share_button_y);
  dict.Set(kShareButtonOpacity, metadata.share_button_opacity);
  dict.Set(kShareButtonIcon, metadata.share_button_icon);
  dict.Set(kShareButtonBg, metadata.share_button_bg);
  dict.Set(kDarkShareButtonX, metadata.dark_share_button_x);
  dict.Set(kDarkShareButtonY, metadata.dark_share_button_y);
  dict.Set(kDarkShareButtonOpacity, metadata.dark_share_button_opacity);
  dict.Set(kDarkShareButtonIcon, metadata.dark_share_button_icon);
  dict.Set(kDarkShareButtonBg, metadata.dark_share_button_bg);
  dict.Set(kWidthPx, metadata.width_px);
  dict.Set(kHeightPx, metadata.height_px);
  dict.Set(kDarkWidthPx, metadata.dark_width_px);
  dict.Set(kDarkHeightPx, metadata.dark_height_px);
  dict.Set(kIframeWidthPx, metadata.iframe_width_px);
  dict.Set(kIframeHeightPx, metadata.iframe_height_px);
  dict.Set(kDarkBackgroundColorKey, metadata.dark_background_color);
  SetTimeValue(dict, kExpirationTimeKey, metadata.expiration_time);
  base::JSONWriter::Write(dict, str);
}

base::FilePath LogoCache::GetLogoPath() {
  return cache_directory_.Append(FILE_PATH_LITERAL("logo"));
}

base::FilePath LogoCache::GetDarkLogoPath() {
  return cache_directory_.Append(FILE_PATH_LITERAL("dark_logo"));
}

base::FilePath LogoCache::GetMetadataPath() {
  return cache_directory_.Append(FILE_PATH_LITERAL("metadata"));
}

void LogoCache::UpdateMetadata(std::unique_ptr<LogoMetadata> metadata) {
  metadata_ = std::move(metadata);
  metadata_is_valid_ = true;
}

void LogoCache::ReadMetadataIfNeeded() {
  if (metadata_is_valid_)
    return;

  std::unique_ptr<LogoMetadata> metadata;
  base::FilePath metadata_path = GetMetadataPath();
  std::string str;
  if (base::ReadFileToString(metadata_path, &str)) {
    metadata =
        LogoMetadataFromString(str, &logo_num_bytes_, &dark_logo_num_bytes_);
    if (!metadata) {
      // Delete corrupt metadata and logo.
      DeleteLogoAndMetadata();
    }
  }

  UpdateMetadata(std::move(metadata));
}

void LogoCache::WriteMetadata() {
  if (!EnsureCacheDirectoryExists())
    return;

  std::string str;
  LogoMetadataToString(*metadata_, logo_num_bytes_, dark_logo_num_bytes_, &str);
  base::WriteFile(GetMetadataPath(), str);
}

void LogoCache::WriteLogo(
    scoped_refptr<base::RefCountedMemory> encoded_image,
    scoped_refptr<base::RefCountedMemory> dark_encoded_image) {
  if (!EnsureCacheDirectoryExists())
    return;

  if (!metadata_) {
    DeleteLogoAndMetadata();
    return;
  }

  // To minimize the chances of ending up in an undetectably broken state:
  // First, delete the metadata file, then update the logo file, then update the
  // metadata file.
  base::FilePath logo_path = GetLogoPath();
  base::FilePath dark_logo_path = GetDarkLogoPath();
  base::FilePath metadata_path = GetMetadataPath();

  if (!base::DeleteFile(metadata_path))
    return;

  if (encoded_image && !base::WriteFile(logo_path, *encoded_image)) {
    base::DeleteFile(logo_path);
    return;
  }
  if (dark_encoded_image &&
      !base::WriteFile(dark_logo_path, *dark_encoded_image)) {
    base::DeleteFile(logo_path);
    base::DeleteFile(dark_logo_path);
    return;
  }

  WriteMetadata();
}

void LogoCache::DeleteLogoAndMetadata() {
  base::DeleteFile(GetLogoPath());
  base::DeleteFile(GetDarkLogoPath());
  base::DeleteFile(GetMetadataPath());
}

bool LogoCache::EnsureCacheDirectoryExists() {
  if (base::DirectoryExists(cache_directory_))
    return true;
  return base::CreateDirectory(cache_directory_);
}

}  // namespace search_provider_logos
