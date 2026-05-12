// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_SANITIZER_H_
#define CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_SANITIZER_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "url/gurl.h"

/**
 * Data structure representing a sanitized Drive file.
 */
struct SanitizedDriveFileData {
  SanitizedDriveFileData();
  SanitizedDriveFileData(const SanitizedDriveFileData&);
  ~SanitizedDriveFileData();

  std::string drive_id;
  std::string mime_type;
  std::string file_name;
  uint64_t size_bytes;
  std::optional<std::string> resource_key;
  std::optional<GURL> thumbnail_url;
};

/**
 * Helper class to sanitize selected files from the 3P Google Picker.
 * This class runs in the trusted browser process and performs strict
 * validation to ensure data from the renderer is safe.
 */
class DrivePickerSanitizer {
 public:
  /**
   * Sanitizes a single DriveFile from Mojo. Returns a SanitizedDriveFileData if
   * all fields are valid, otherwise returns std::nullopt.
   */
  static std::optional<SanitizedDriveFileData> Sanitize(
      const drive_picker_host::mojom::DriveFilePtr& file);

 private:
  /**
   * Validates that the given ID or resource key contains only safe characters.
   * Security rationale: Prevents injection or malformed data in backend flows.
   * Matches the regex: [a-zA-Z0-9\-_]+
   */
  static bool IsIdValid(const std::string& id);

  /**
   * Validates that the given thumbnail URL is a valid HTTPS URL and points
   * to a trusted Google Drive storage domain.
   * Security rationale: Prevents the browser from loading images from
   * arbitrary or malicious domains.
   */
  static bool IsThumbnailUrlAcceptable(const GURL& url);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DRIVE_PICKER_HOST_DRIVE_PICKER_SANITIZER_H_
