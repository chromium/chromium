// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_sanitizer.h"

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/mime_util.h"

SanitizedDriveFileData::SanitizedDriveFileData() = default;
SanitizedDriveFileData::SanitizedDriveFileData(const SanitizedDriveFileData&) =
    default;
SanitizedDriveFileData::~SanitizedDriveFileData() = default;

// static
std::optional<SanitizedDriveFileData> DrivePickerSanitizer::Sanitize(
    const drive_picker_host::mojom::DriveFilePtr& file) {
  if (!file) {
    return std::nullopt;
  }

  // Validate ID: Must match [a-zA-Z0-9\-_]+
  if (file->id.empty() || !IsIdValid(file->id)) {
    DLOG(WARNING) << "Invalid Drive file ID: " << file->id;
    return std::nullopt;
  }

  // Validate Metadata: Essential fields must be non-empty and well-formed.
  if (file->name.empty() || file->mime_type.empty() ||
      !net::ParseMimeTypeWithoutParameter(file->mime_type, nullptr, nullptr)) {
    DLOG(WARNING) << "Missing or invalid essential metadata for file: "
                  << file->id;
    return std::nullopt;
  }

  // Validate Type: Only allow known Drive document types.
  if (file->type != "document" && file->type != "photo" &&
      file->type != "video") {
    DLOG(WARNING) << "Unsupported file type: " << file->type;
    return std::nullopt;
  }

  SanitizedDriveFileData sanitized;
  sanitized.drive_id = file->id;
  sanitized.mime_type = file->mime_type;
  sanitized.file_name = file->name;
  sanitized.size_bytes = file->size_bytes;

  // Validate Resource Key: Match ID validation if present.
  if (file->resource_key) {
    if (IsIdValid(*file->resource_key)) {
      sanitized.resource_key = file->resource_key;
    } else {
      DLOG(WARNING) << "Invalid resource key for file: " << file->id;
      sanitized.resource_key = std::nullopt;
    }
  }

  // Validate Thumbnail URL: Strictly restrict to trusted Google Drive
  // storage domains and only for photo types.
  if (file->type == "photo" && file->thumbnail_url) {
    if (IsThumbnailUrlAcceptable(file->thumbnail_url.value())) {
      sanitized.thumbnail_url = file->thumbnail_url;
    } else {
      DLOG(WARNING) << "Untrusted thumbnail URL blocked: "
                    << file->thumbnail_url->spec();
      sanitized.thumbnail_url = std::nullopt;
    }
  } else {
    // Non-photo types or invalid/missing URLs are nulled out for security.
    sanitized.thumbnail_url = std::nullopt;
  }

  return sanitized;
}

// static
bool DrivePickerSanitizer::IsIdValid(const std::string& id) {
  for (char c : id) {
    if (!base::IsAsciiAlphaNumeric(c) && c != '-' && c != '_') {
      return false;
    }
  }
  return !id.empty();
}

// static
bool DrivePickerSanitizer::IsThumbnailUrlAcceptable(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  static constexpr auto kTrustedHosts =
      base::MakeFixedFlatSet<std::string_view>({
          "lh3.googleusercontent.com",
          "lh4.googleusercontent.com",
          "lh5.googleusercontent.com",
          "lh6.googleusercontent.com",
      });

  if (!kTrustedHosts.contains(url.host())) {
    return false;
  }

  // Path must start with /drive-storage/
  if (!base::StartsWith(url.path(), "/drive-storage/",
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  return true;
}
