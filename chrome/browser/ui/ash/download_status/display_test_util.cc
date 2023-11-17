// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_test_util.h"

#include "base/unguessable_token.h"
#include "chrome/browser/ui/ash/ash_test_util.h"

namespace ash::download_status {

crosapi::mojom::DownloadStatusPtr CreateDownloadStatus(
    Profile* profile,
    crosapi::mojom::DownloadState state,
    const absl::optional<int64_t>& received_bytes,
    const absl::optional<int64_t>& target_bytes) {
  crosapi::mojom::DownloadStatusPtr download_status =
      crosapi::mojom::DownloadStatus::New();
  download_status->full_path = test::CreateFile(profile);
  download_status->guid = base::UnguessableToken::Create().ToString();
  download_status->received_bytes = received_bytes;
  download_status->state = state;
  download_status->total_bytes = target_bytes;

  return download_status;
}

crosapi::mojom::DownloadStatusPtr CreateInProgressDownloadStatus(
    Profile* profile,
    const absl::optional<int64_t>& received_bytes,
    const absl::optional<int64_t>& target_bytes) {
  return CreateDownloadStatus(profile,
                              crosapi::mojom::DownloadState::kInProgress,
                              received_bytes, target_bytes);
}

}  // namespace ash::download_status
