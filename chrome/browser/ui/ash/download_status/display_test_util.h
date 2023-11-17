// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_TEST_UTIL_H_

#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash::download_status {

// Creates a download status associated with a file under the downloads
// directory of `profile`.
crosapi::mojom::DownloadStatusPtr CreateDownloadStatus(
    Profile* profile,
    crosapi::mojom::DownloadState state,
    const absl::optional<int64_t>& received_bytes,
    const absl::optional<int64_t>& target_bytes);

// Creates a download status that indicates an in progress download associated
// with a file under the downloads directory of `profile`.
crosapi::mojom::DownloadStatusPtr CreateInProgressDownloadStatus(
    Profile* profile,
    const absl::optional<int64_t>& received_bytes,
    const absl::optional<int64_t>& target_bytes);

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_TEST_UTIL_H_
