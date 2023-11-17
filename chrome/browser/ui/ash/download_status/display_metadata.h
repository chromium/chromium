// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_

#include <string>

#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::download_status {

// The metadata used to display downloads.
// TODO(http://b/307347158): Fill `DisplayMetadata`.
struct DisplayMetadata {
  DisplayMetadata();
  DisplayMetadata(DisplayMetadata&&);
  DisplayMetadata& operator=(DisplayMetadata&&);
  ~DisplayMetadata();

  // The path to the file that bytes are actually written to during download.
  // NOTE: This path is different from the download target path.
  base::FilePath file_path;

  // The received bytes of download.
  absl::optional<int64_t> received_bytes;

  // The text that provides additional details about the download.
  absl::optional<std::u16string> secondary_text;

  // The primary text of the displayed download.
  absl::optional<std::u16string> text;

  // The total bytes of download.
  absl::optional<int64_t> total_bytes;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_METADATA_H_
