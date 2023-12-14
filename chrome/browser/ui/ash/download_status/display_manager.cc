// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_manager.h"

#include <functional>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"
#include "chrome/browser/ui/ash/download_status/notification_display_client.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"

namespace ash::download_status {

namespace {

// Returns true if `download_status` provides sufficient data to display the
// associated download update.
bool CanDisplay(const crosapi::mojom::DownloadStatus& download_status) {
  const std::optional<base::FilePath>& file_path = download_status.full_path;
  return file_path.has_value() && !file_path->empty();
}

// Returns the total number of bytes, or `std::nullopt` if the
// `download_status` total bytes count is null or less than one.
std::optional<int64_t> GetTotalBytes(
    const crosapi::mojom::DownloadStatus& download_status) {
  const std::optional<int64_t>& total_bytes = download_status.total_bytes;
  return total_bytes > 0 ? total_bytes : std::nullopt;
}

// Returns the number of received bytes, or `std::nullopt` if the
// `download_status` received bytes count is null or a negative value.
// NOTE: This function ensures that the number of received bytes is less than
// the number of total bytes if the download is not complete.
std::optional<int64_t> GetReceivedBytes(
    const crosapi::mojom::DownloadStatus& download_status) {
  const std::optional<int64_t>& received_bytes = download_status.received_bytes;
  if (received_bytes < 0) {
    return std::nullopt;
  }

  if (const std::optional<int64_t>& total_bytes =
          GetTotalBytes(download_status);
      received_bytes == total_bytes &&
      download_status.state != crosapi::mojom::DownloadState::kComplete) {
    return *received_bytes - 1;
  }

  return received_bytes;
}

// Returns the text to display for the download specified by `download_status`.
std::optional<std::u16string> GetText(
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

}  // namespace

DisplayManager::DisplayManager(
    Profile* profile,
    crosapi::DownloadStatusUpdaterAsh* download_status_updater)
    : download_status_updater_(download_status_updater) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
  CHECK(download_status_updater_);

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

// TODO(http://b/307347158): Fill `display_metadata`.
DisplayMetadata DisplayManager::CalculateDisplayMetadata(
    const crosapi::mojom::DownloadStatus& download_status) {
  CHECK(CanDisplay(download_status));

  DisplayMetadata display_metadata;

  // NOTE: Since `download_status_updater_` owns `DisplayManager`, using
  // `base::Unretained()` is safe here.
  std::vector<CommandInfo> command_infos;
  if (download_status.cancellable.value_or(false)) {
    command_infos.emplace_back(
        base::BindRepeating(&crosapi::DownloadStatusUpdaterAsh::Cancel,
                            base::Unretained(download_status_updater_),
                            download_status.guid,
                            /*callback=*/base::DoNothing()),
        &kCancelIcon, IDS_ASH_DOWNLOAD_COMMAND_TEXT_CANCEL,
        CommandType::kCancel);
  }
  if (download_status.pausable.value_or(false)) {
    command_infos.emplace_back(
        base::BindRepeating(&crosapi::DownloadStatusUpdaterAsh::Pause,
                            base::Unretained(download_status_updater_),
                            download_status.guid,
                            /*callback=*/base::DoNothing()),
        &kPauseIcon, IDS_ASH_DOWNLOAD_COMMAND_TEXT_PAUSE, CommandType::kPause);
  }
  if (download_status.resumable.value_or(false)) {
    command_infos.emplace_back(
        base::BindRepeating(&crosapi::DownloadStatusUpdaterAsh::Resume,
                            base::Unretained(download_status_updater_),
                            download_status.guid,
                            /*callback=*/base::DoNothing()),
        &kResumeIcon, IDS_ASH_DOWNLOAD_COMMAND_TEXT_RESUME,
        CommandType::kResume);
  }
  display_metadata.command_infos = std::move(command_infos);

  display_metadata.file_path = *download_status.full_path;
  display_metadata.received_bytes = GetReceivedBytes(download_status);
  display_metadata.secondary_text = download_status.status_text;
  display_metadata.text = GetText(download_status);
  display_metadata.total_bytes = GetTotalBytes(download_status);

  return display_metadata;
}

void DisplayManager::Remove(const std::string& guid) {
  for (auto& client : clients_) {
    client->Remove(guid);
  }
}

}  // namespace ash::download_status
