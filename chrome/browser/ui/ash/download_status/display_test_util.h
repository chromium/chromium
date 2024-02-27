// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_TEST_UTIL_H_

#include <optional>
#include <string_view>

#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"

class Profile;

namespace ash::download_status {

// Creates a download status associated with a file with the specified
// `extension` under the downloads directory of `profile`.
crosapi::mojom::DownloadStatusPtr CreateDownloadStatus(
    Profile* profile,
    std::string_view extension,
    crosapi::mojom::DownloadState state,
    crosapi::mojom::DownloadProgressPtr progress);

// Creates a download status that indicates an in-progress download associated
// with a file under the downloads directory of `profile`.
crosapi::mojom::DownloadStatusPtr CreateInProgressDownloadStatus(
    Profile* profile,
    std::string_view extension,
    int64_t received_bytes,
    const std::optional<int64_t>& total_bytes = std::nullopt);

// Updates `status` to indicate a completed download.
// NOTE: It should be called only when `status` has a defined progress.
void MarkDownloadStatusCompleted(crosapi::mojom::DownloadStatus& status);

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_TEST_UTIL_H_
