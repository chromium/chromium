// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_test_util.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"

namespace ash::download_status {

namespace {

// Indicates an unknown total bytes count of `crosapi::mojom::DownloadProgress`.
constexpr int64_t kUnknownTotalBytes = -1;

}  // namespace

crosapi::mojom::DownloadStatusPtr CreateDownloadStatus(
    Profile* profile,
    std::string_view extension,
    crosapi::mojom::DownloadState state,
    crosapi::mojom::DownloadProgressPtr progress) {
  crosapi::mojom::DownloadStatusPtr download_status =
      crosapi::mojom::DownloadStatus::New();
  download_status->full_path = test::CreateFile(profile, extension);
  download_status->guid = base::UnguessableToken::Create().ToString();
  download_status->progress = std::move(progress);
  download_status->state = state;

  return download_status;
}

crosapi::mojom::DownloadStatusPtr CreateInProgressDownloadStatus(
    Profile* profile,
    std::string_view extension,
    int64_t received_bytes,
    const std::optional<int64_t>& total_bytes) {
  return CreateDownloadStatus(
      profile, extension, crosapi::mojom::DownloadState::kInProgress,
      crosapi::mojom::DownloadProgress::New(
          /*loop=*/false, received_bytes,
          total_bytes.value_or(kUnknownTotalBytes), /*visible=*/true));
}

void MarkDownloadStatusCompleted(crosapi::mojom::DownloadStatus& status) {
  crosapi::mojom::DownloadProgressPtr& progress_ptr = status.progress;
  CHECK(progress_ptr);

  progress_ptr->received_bytes = progress_ptr->total_bytes = base::ranges::max(
      {progress_ptr->received_bytes, progress_ptr->total_bytes, int64_t{0}});
  progress_ptr->visible = false;

  status.state = crosapi::mojom::DownloadState::kComplete;
}

}  // namespace ash::download_status
