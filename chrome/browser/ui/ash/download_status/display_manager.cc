// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_manager.h"

#include <functional>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"
#include "chrome/browser/ui/ash/download_status/notification_display_client.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash::download_status {

namespace {

// Returns true if `download_status` provides sufficient data to display the
// associated download update.
bool CanDisplay(const crosapi::mojom::DownloadStatus& download_status) {
  const absl::optional<base::FilePath>& file_path = download_status.full_path;
  return file_path.has_value() && !file_path->empty();
}

// Returns true if the download specified by `download_status` is in progress.
bool IsInProgress(const crosapi::mojom::DownloadStatus& download_status) {
  return download_status.state == crosapi::mojom::DownloadState::kInProgress;
}

// Returns the total number of bytes, or `absl::nullopt` if the
// `download_status` total bytes count is null or less than one.
absl::optional<int64_t> GetTotalBytes(
    const crosapi::mojom::DownloadStatus& download_status) {
  const absl::optional<int64_t>& total_bytes = download_status.total_bytes;
  return total_bytes > 0 ? total_bytes : absl::nullopt;
}

// Returns the number of received bytes, or `absl::nullopt` if the
// `download_status` received bytes count is null or a negative value.
// NOTE: This function ensures that the number of received bytes is less than
// the number of total bytes if the download is not complete.
absl::optional<int64_t> GetReceivedBytes(
    const crosapi::mojom::DownloadStatus& download_status) {
  const absl::optional<int64_t>& received_bytes =
      download_status.received_bytes;
  if (received_bytes < 0) {
    return absl::nullopt;
  }

  if (const absl::optional<int64_t>& total_bytes =
          GetTotalBytes(download_status);
      received_bytes == total_bytes &&
      download_status.state != crosapi::mojom::DownloadState::kComplete) {
    return *received_bytes - 1;
  }

  return received_bytes;
}

// Returns the secondary text for the download specified by `download_status`,
// typically the received bytes count and the total bytes count.
absl::optional<std::u16string> GetSecondaryText(
    const crosapi::mojom::DownloadStatus& download_status) {
  // Only in-progress downloads have secondary text.
  if (!IsInProgress(download_status)) {
    return absl::nullopt;
  }

  const absl::optional<int64_t> received_bytes =
      GetReceivedBytes(download_status);
  const absl::optional<int64_t> total_bytes = GetTotalBytes(download_status);

  // If both `received_bytes` and `total_bytes` are known, the secondary text
  // will be something of the form "10/100 MB", where the first number is the
  // count of received bytes and the second number is the count of total bytes.
  if (received_bytes.has_value() && total_bytes.has_value()) {
    const ui::DataUnits units = ui::GetByteDisplayUnits(total_bytes.value());
    return l10n_util::GetStringFUTF16(
        IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_SIZE_INFO,
        ui::FormatBytesWithUnits(*received_bytes, units, /*show_units=*/false),
        ui::FormatBytesWithUnits(*total_bytes, units,
                                 /*show_units=*/true));
  }

  // If only `received_bytes` is known, the secondary text will be something of
  // the form "10 MB", indicating only the count of received bytes.
  if (received_bytes.has_value()) {
    return ui::FormatBytes(*received_bytes);
  }

  // TODO(http://b/307347158): Update the secondary text if the underlying
  // download is paused.
  return absl::nullopt;
}

// Returns the text to display for the download specified by `download_status`.
absl::optional<std::u16string> GetText(
    const crosapi::mojom::DownloadStatus& download_status) {
  CHECK(CanDisplay(download_status));

  // By default, text is generated from the full path.
  std::reference_wrapper<const base::FilePath> file_path =
      *download_status.full_path;

  // Generate text from the target file path if:
  // 1. The associated download is in progress.
  // 2. The target file path exists.
  if (download_status.state == crosapi::mojom::DownloadState::kInProgress &&
      download_status.target_file_path) {
    file_path = *download_status.target_file_path;
  }

  return file_path.get().BaseName().LossyDisplayName();
}

// Calculates the metadata to display the download update specified by
// `download_status`. This function should be called only when the specified
// download can be displayed.
download_status::DisplayMetadata CalculateDisplayMetadata(
    const crosapi::mojom::DownloadStatus& download_status) {
  // TODO(http://b/307347158): Fill `display_metadata`.

  CHECK(CanDisplay(download_status));

  download_status::DisplayMetadata display_metadata;
  display_metadata.file_path = *download_status.full_path;
  display_metadata.received_bytes = GetReceivedBytes(download_status);
  display_metadata.secondary_text = GetSecondaryText(download_status);
  display_metadata.text = GetText(download_status);
  display_metadata.total_bytes = GetTotalBytes(download_status);

  return display_metadata;
}

}  // namespace

DisplayManager::DisplayManager(Profile* profile) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());

  clients_.push_back(std::make_unique<HoldingSpaceDisplayClient>(profile));
  clients_.push_back(std::make_unique<NotificationDisplayClient>(profile));
}

DisplayManager::~DisplayManager() = default;

void DisplayManager::Update(
    const crosapi::mojom::DownloadStatus& download_status) {
  switch (download_status.state) {
    case crosapi::mojom::DownloadState::kCancelled:
    case crosapi::mojom::DownloadState::kInterrupted:
      Remove(download_status.guid);
      return;
    case crosapi::mojom::DownloadState::kComplete:
    case crosapi::mojom::DownloadState::kInProgress: {
      if (!CanDisplay(download_status)) {
        // TODO(http://b/308192833): Add a metric to record the case where a
        // displayed download is removed because it cannot be displayed.
        Remove(download_status.guid);
        return;
      }
      const download_status::DisplayMetadata display_metadata =
          CalculateDisplayMetadata(download_status);
      for (auto& client : clients_) {
        client->AddOrUpdate(download_status.guid, display_metadata);
      }
      return;
    }
    case crosapi::mojom::DownloadState::kUnknown:
      return;
  }
}

void DisplayManager::Remove(const std::string& guid) {
  for (auto& client : clients_) {
    client->Remove(guid);
  }
}

}  // namespace ash::download_status
