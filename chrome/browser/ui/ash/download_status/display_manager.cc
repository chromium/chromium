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
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"
#include "chrome/browser/ui/ash/download_status/notification_display_client.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"

namespace ash::download_status {

namespace {

// Constants -------------------------------------------------------------------

// Indicates an unknown total bytes count of `crosapi::mojom::DownloadStatus`.
constexpr int64_t kUnknownTotalBytes = -1;

// Helpers ---------------------------------------------------------------------

// Returns true if `download_status` provides sufficient data to display the
// associated download update.
bool CanDisplay(const crosapi::mojom::DownloadStatus& download_status) {
  const std::optional<base::FilePath>& file_path = download_status.full_path;
  return file_path.has_value() && !file_path->empty();
}

std::string GetPrintString(const std::optional<int64_t>& data) {
  return data.has_value() ? base::NumberToString(data.value()) : "null";
}

// Returns the progress indicated by `download_status`.
Progress GetProgress(const crosapi::mojom::DownloadStatus& download_status) {
  const std::optional<int64_t>& received_bytes = download_status.received_bytes;
  const std::optional<int64_t>& total_bytes = download_status.total_bytes;

  // `received_bytes` and `total_bytes` could be invalid. Correct these numbers
  // if necessary. NOTE: `total_bytes` could be negative but `Progress` expects
  // a non-negative if `updated_total_bytes` has a value.
  std::optional<int64_t> updated_received_bytes = received_bytes;
  std::optional<int64_t> updated_total_bytes = total_bytes;

  if (received_bytes && received_bytes < 0) {
    LOG(ERROR) << "The received bytes count is invalid: expected a non "
                  "negative value; the actual value is "
               << GetPrintString(received_bytes);
    updated_received_bytes = std::nullopt;
  }

  if (total_bytes && total_bytes < kUnknownTotalBytes) {
    LOG(ERROR) << "The total bytes count is invalid: expected to be a non "
                  "negative value or -1 that indicates an unknown total bytes "
                  "count; the actual value is "
               << GetPrintString(total_bytes);
  }

  // `Progress` does not accept a negative total bytes count.
  if (updated_total_bytes < 0) {
    updated_total_bytes = std::nullopt;
  }

  const bool is_determinate =
      received_bytes && total_bytes && total_bytes != kUnknownTotalBytes;

  if (is_determinate && received_bytes > total_bytes) {
    LOG(ERROR) << "For a download that is determinate, its received bytes "
                  "count should not be greater than the total bytes count; the "
                  "actual received bytes count is "
               << GetPrintString(received_bytes)
               << " and the actual total bytes count is "
               << GetPrintString(total_bytes);
  }

  const bool complete =
      download_status.state == crosapi::mojom::DownloadState::kComplete;

  if (complete) {
    updated_received_bytes = updated_total_bytes =
        base::ranges::max({updated_received_bytes, updated_total_bytes,
                           std::optional<int64_t>(0)});
  } else if (updated_total_bytes >= 0 &&
             updated_received_bytes > updated_total_bytes) {
    updated_total_bytes = updated_received_bytes;
  }

  return Progress(updated_received_bytes, updated_total_bytes, complete);
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

// Opens the download file specified by `file_path` under the file system
// associated with `profile`.
void OpenFile(Profile* profile, const base::FilePath& file_path) {
  if (file_path.empty()) {
    LOG(ERROR) << "Tried to open a file with an empty path.";
    return;
  }

  // TODO(http://b/316368295): Track successful file openings as a metric.
  platform_util::OpenItem(profile, file_path,
                          platform_util::OpenItemType::OPEN_FILE,
                          /*callback=*/base::DoNothing());
}

// Shows the download file specified by `file_path` in the folder under the file
// system associated with `profile`.
void ShowInFolder(Profile* profile, const base::FilePath& file_path) {
  if (file_path.empty()) {
    LOG(ERROR) << "Tried to show a file in folder with an empty path.";
    return;
  }

  file_manager::util::ShowItemInFolder(profile, file_path,
                                       /*callback=*/base::DoNothing());
}

}  // namespace

DisplayManager::DisplayManager(
    Profile* profile,
    crosapi::DownloadStatusUpdaterAsh* download_status_updater)
    : profile_(profile), download_status_updater_(download_status_updater) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
  CHECK(download_status_updater_);

  CHECK(profile_);
  profile_observation_.Observe(profile_);

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

void DisplayManager::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_ = nullptr;
}

DisplayMetadata DisplayManager::CalculateDisplayMetadata(
    const crosapi::mojom::DownloadStatus& download_status) {
  CHECK(CanDisplay(download_status));

  DisplayMetadata display_metadata;

  std::vector<CommandInfo> command_infos;
  if (download_status.cancellable.value_or(false)) {
    command_infos.emplace_back(
        base::BindRepeating(&DisplayManager::PerformCommand,
                            weak_ptr_factory_.GetWeakPtr(),
                            CommandType::kCancel, download_status.guid),
        &kCancelIcon, IDS_ASH_DOWNLOAD_COMMAND_TEXT_CANCEL,
        CommandType::kCancel);
  }
  if (download_status.pausable.value_or(false)) {
    command_infos.emplace_back(
        base::BindRepeating(&DisplayManager::PerformCommand,
                            weak_ptr_factory_.GetWeakPtr(), CommandType::kPause,
                            download_status.guid),
        &kPauseIcon, IDS_ASH_DOWNLOAD_COMMAND_TEXT_PAUSE, CommandType::kPause);
  }
  if (download_status.resumable.value_or(false)) {
    command_infos.emplace_back(
        base::BindRepeating(&DisplayManager::PerformCommand,
                            weak_ptr_factory_.GetWeakPtr(),
                            CommandType::kResume, download_status.guid),
        &kResumeIcon, IDS_ASH_DOWNLOAD_COMMAND_TEXT_RESUME,
        CommandType::kResume);
  }
  switch (download_status.state) {
    case crosapi::mojom::DownloadState::kComplete:
      // NOTE: `kOpenFile` is not shown so it doesn't require an icon/text_id.
      command_infos.emplace_back(
          base::BindRepeating(
              &DisplayManager::PerformCommand, weak_ptr_factory_.GetWeakPtr(),
              CommandType::kOpenFile, *download_status.full_path),
          /*icon=*/nullptr, /*text_id=*/-1, CommandType::kOpenFile);

      // NOTE: The `kShowInFolder` button does not have an icon.
      command_infos.emplace_back(
          base::BindRepeating(
              &DisplayManager::PerformCommand, weak_ptr_factory_.GetWeakPtr(),
              CommandType::kShowInFolder, *download_status.full_path),
          /*icon=*/nullptr, IDS_ASH_DOWNLOAD_COMMAND_TEXT_SHOW_IN_FOLDER,
          CommandType::kShowInFolder);
      break;
    case crosapi::mojom::DownloadState::kInProgress:
      // NOTE: `kShowInBrowser` is not shown so doesn't require an icon/text_id.
      command_infos.emplace_back(
          base::BindRepeating(
              &DisplayManager::PerformCommand, weak_ptr_factory_.GetWeakPtr(),
              CommandType::kShowInBrowser, download_status.guid),
          /*icon=*/nullptr, /*text_id=*/-1, CommandType::kShowInBrowser);
      break;
    case crosapi::mojom::DownloadState::kCancelled:
    case crosapi::mojom::DownloadState::kInterrupted:
    case crosapi::mojom::DownloadState::kUnknown:
      break;
  }
  display_metadata.command_infos = std::move(command_infos);

  display_metadata.file_path = *download_status.full_path;
  display_metadata.image = download_status.image;
  display_metadata.progress = GetProgress(download_status);
  display_metadata.secondary_text = download_status.status_text;
  display_metadata.text = GetText(download_status);

  return display_metadata;
}

void DisplayManager::PerformCommand(
    CommandType command,
    const std::variant</*guid=*/std::string, base::FilePath>& param) {
  switch (command) {
    case CommandType::kCancel:
      download_status_updater_->Cancel(/*guid=*/std::get<std::string>(param),
                                       /*callback=*/base::DoNothing());
      break;
    case CommandType::kOpenFile:
      OpenFile(profile_, std::get<base::FilePath>(param));
      break;
    case CommandType::kPause:
      download_status_updater_->Pause(/*guid=*/std::get<std::string>(param),
                                      /*callback=*/base::DoNothing());
      break;
    case CommandType::kResume:
      download_status_updater_->Resume(/*guid=*/std::get<std::string>(param),
                                       /*callback=*/base::DoNothing());
      break;
    case CommandType::kShowInBrowser:
      download_status_updater_->ShowInBrowser(
          /*guid=*/std::get<std::string>(param),
          /*callback=*/base::DoNothing());
      break;
    case CommandType::kShowInFolder:
      ShowInFolder(profile_, std::get<base::FilePath>(param));
      break;
  }
}

void DisplayManager::Remove(const std::string& guid) {
  for (auto& client : clients_) {
    client->Remove(guid);
  }
}

}  // namespace ash::download_status
