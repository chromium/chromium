// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_sanitizer.h"

#include <string_view>

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
      file->type != "video" && file->type != "file") {
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
  // storage domains and only for photo and video types.
  if ((file->type == "photo" || file->type == "video") && file->thumbnail_url) {
    if (IsThumbnailUrlAcceptable(file->thumbnail_url.value())) {
      sanitized.thumbnail_url = file->thumbnail_url;
    } else {
      DLOG(WARNING) << "Untrusted thumbnail URL blocked by C++ sanitizer: "
                    << file->thumbnail_url->spec();
      sanitized.thumbnail_url = std::nullopt;
    }
  } else {
    // Non-photo/video types or invalid/missing URLs are nulled out for
    // security.
    sanitized.thumbnail_url = std::nullopt;
  }

  if (file->icon_url) {
    sanitized.icon_url = SanitizeDriveIconUrl(file->icon_url->spec());
    if (sanitized.icon_url->is_empty()) {
      sanitized.icon_url = std::nullopt;
    }
  }

  return sanitized;
}

// static
GURL DrivePickerSanitizer::SanitizeDriveIconUrl(const std::string& url_string) {
  GURL url(url_string);
  if (!url.is_valid()) {
    return GURL();
  }
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return GURL();
  }

  static constexpr auto kTrustedHosts =
      base::MakeFixedFlatSet<std::string_view>({
          "lh3.googleusercontent.com",
          "lh4.googleusercontent.com",
          "lh5.googleusercontent.com",
          "lh6.googleusercontent.com",
          "drive-thirdparty.googleusercontent.com",
      });

  if (!kTrustedHosts.contains(url.host())) {
    return GURL();
  }

  // Path must start with /[size]/hype/ or /[size]/type/
  std::string_view path = url.path();
  if (path.empty() || path[0] != '/') {
    return GURL();
  }

  size_t second_slash = path.find('/', 1);
  if (second_slash == std::string::npos) {
    return GURL();
  }
  std::string_view remaining_path = path.substr(second_slash + 1);

  if (!base::StartsWith(remaining_path, "hype/") &&
      !base::StartsWith(remaining_path, "type/")) {
    return GURL();
  }

  return url;
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
          "drive.google.com",
      });

  if (!kTrustedHosts.contains(url.host())) {
    return false;
  }

  // For drive.google.com, path must be exactly /thumbnail
  if (url.host() == "drive.google.com") {
    return url.path() == "/thumbnail";
  }

  // Path must start with a valid Google Drive content/thumbnail prefix:
  // - "/drive-storage/": Direct Drive content/thumbnail endpoint.
  // - "/d/": Guessable FIFE URL path used to fetch/scale Google Drive files
  //   (go/fife-urls).
  //   (e.g., /d/<drive_file_id> or /d/<drive_file_id>=wXXX-hXXX).
  // - "/rd-d/": Ephemeral redirect authentication path returned as a 302
  //   when a client attempts to access a private /d/ asset without session
  //   credentials.
  if (!url.path().starts_with("/drive-storage/") &&
      !url.path().starts_with("/d/") && !url.path().starts_with("/rd-d/")) {
    return false;
  }

  return true;
}
