// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_manager.h"

#include <functional>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"
#include "chrome/browser/ui/ash/download_status/notification_display_client.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash::download_status {

namespace {

// Constants -------------------------------------------------------------------

// Indicates an unknown total bytes count of `crosapi::mojom::DownloadStatus`.
constexpr int64_t kUnknownTotalBytes = 0;

// Helpers ---------------------------------------------------------------------

// Returns true if `download_status` provides sufficient data to display the
// associated download update.
bool CanDisplay(const crosapi::mojom::DownloadStatus& download_status) {
  const std::optional<base::FilePath>& file_path = download_status.full_path;
  return file_path.has_value() && !file_path->empty();
}

// Returns valid icons from `download_status` if any.
// NOTE: Returns a non-null only if both dark and light mode icons are valid.
crosapi::mojom::DownloadStatusIconsPtr GetIcons(
    const crosapi::mojom::DownloadStatus& download_status) {
  auto is_image_valid = [](const gfx::ImageSkia& image) {
    return !image.size().IsEmpty();
  };

  const crosapi::mojom::DownloadStatusIconsPtr& icons = download_status.icons;
  return icons && is_image_valid(icons->dark_mode) &&
                 is_image_valid(icons->light_mode)
             ? icons.Clone()
             : nullptr;
}

std::string GetPrintString(const std::optional<int64_t>& data) {
  return data.has_value() ? base::NumberToString(data.value()) : "null";
}

// Returns the progress indicated by `download_status`.
Progress GetProgress(const crosapi::mojom::DownloadStatus& download_status) {
  std::optional<int64_t> received_bytes;
  std::optional<int64_t> total_bytes;
  bool visible = false;

  if (const crosapi::mojom::DownloadProgressPtr& progress_ptr =
          download_status.progress) {
    received_bytes = progress_ptr->received_bytes;
    total_bytes = progress_ptr->total_bytes;
    visible = progress_ptr->visible;
  }

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
                  "negative value or 0 that indicates an unknown total bytes "
                  "count; the actual value is "
               << GetPrintString(total_bytes);
  }

  // Use `std::nullopt` to indicate an indeterminate total bytes count.
  if (updated_total_bytes <= kUnknownTotalBytes) {
    updated_total_bytes = std::nullopt;
  }

  const bool is_determinate = updated_received_bytes && updated_total_bytes;

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
  } else if (is_determinate && updated_received_bytes > updated_total_bytes) {
    updated_total_bytes = updated_received_bytes;
  }

  return Progress(updated_received_bytes, updated_total_bytes, complete,
                  !visible);
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
  const base::FilePath& full_path = *download_status.full_path;
  switch (download_status.state) {
    case crosapi::mojom::DownloadState::kComplete: {
      const base::FilePath::StringType ext = full_path.Extension();
      std::string mime_type;
      const bool has_mime_type = ext.empty()
                                     ? false
                                     : net::GetWellKnownMimeTypeFromExtension(
                                           ext.substr(1), &mime_type);

      // NOTE: `kOpenFile` is not shown so it doesn't require an icon/text_id.
      command_infos.emplace_back(
          base::BindRepeating(&DisplayManager::PerformCommand,
                              weak_ptr_factory_.GetWeakPtr(),
                              CommandType::kOpenFile, full_path),
          /*icon=*/nullptr, /*text_id=*/-1, CommandType::kOpenFile);

      std::optional<std::pair<CommandType, /*text_id=*/int>>
          media_app_command_metadata;

      if (mime_type == "application/pdf") {
        media_app_command_metadata =
            std::make_pair(CommandType::kEditWithMediaApp,
                           IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN_AND_EDIT);
      } else if (base::StartsWith(mime_type, "audio/",
                                  base::CompareCase::SENSITIVE) ||
                 base::StartsWith(mime_type, "video/",
                                  base::CompareCase::SENSITIVE)) {
        media_app_command_metadata =
            std::make_pair(CommandType::kOpenWithMediaApp,
                           IDS_DOWNLOAD_NOTIFICATION_LABEL_OPEN);
      }

      if (media_app_command_metadata) {
        command_infos.emplace_back(
            base::BindRepeating(&DisplayManager::PerformCommand,
                                weak_ptr_factory_.GetWeakPtr(),
                                media_app_command_metadata->first, full_path),
            /*icon=*/nullptr, media_app_command_metadata->second,
            media_app_command_metadata->first);
      }

      // NOTE: The `kShowInFolder` button does not have an icon.
      command_infos.emplace_back(
          base::BindRepeating(&DisplayManager::PerformCommand,
                              weak_ptr_factory_.GetWeakPtr(),
                              CommandType::kShowInFolder, full_path),
          /*icon=*/nullptr, IDS_ASH_DOWNLOAD_COMMAND_TEXT_SHOW_IN_FOLDER,
          CommandType::kShowInFolder);

      // Add a command to copy the download file to clipboard if:
      // 1. `download_status` has a valid image; AND
      // 2. The download file is an image.
      // NOTE: The `kCopyToClipboard` button does not require an icon.
      if (const gfx::ImageSkia& image = download_status.image;
          !image.isNull() && !image.size().IsEmpty() && has_mime_type &&
          blink::IsSupportedImageMimeType(mime_type)) {
        command_infos.emplace_back(
            base::BindRepeating(&DisplayManager::PerformCommand,
                                weak_ptr_factory_.GetWeakPtr(),
                                CommandType::kCopyToClipboard, full_path),
            /*icon=*/nullptr, IDS_ASH_DOWNLOAD_COMMAND_TEXT_COPY_TO_CLIPBOARD,
            CommandType::kCopyToClipboard);
      }
      break;
    }
    case crosapi::mojom::DownloadState::kInProgress:
      // NOTE: `kShowInBrowser` is not shown so doesn't require an icon/text_id.
      command_infos.emplace_back(
          base::BindRepeating(
              &DisplayManager::PerformCommand, weak_ptr_factory_.GetWeakPtr(),
              CommandType::kShowInBrowser, download_status.guid),
          /*icon=*/nullptr, /*text_id=*/-1, CommandType::kShowInBrowser);

      if (!download_status.cancellable.value_or(false) &&
          !download_status.pausable.value_or(false) &&
          !download_status.resumable.value_or(false)) {
        command_infos.emplace_back(
            base::BindRepeating(
                &DisplayManager::PerformCommand, weak_ptr_factory_.GetWeakPtr(),
                CommandType::kViewDetailsInBrowser, download_status.guid),
            &kOpenInBrowserIcon,
            IDS_ASH_DOWNLOAD_COMMAND_TEXT_VIEW_DETAILS_IN_BROWSER,
            CommandType::kViewDetailsInBrowser);
      }
      break;
    case crosapi::mojom::DownloadState::kCancelled:
    case crosapi::mojom::DownloadState::kInterrupted:
    case crosapi::mojom::DownloadState::kUnknown:
      break;
  }
  display_metadata.command_infos = std::move(command_infos);

  display_metadata.file_path = full_path;
  display_metadata.icons = GetIcons(download_status);
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
    case CommandType::kCopyToClipboard: {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteFilenames(ui::FileInfosToURIList(
          /*filenames=*/{ui::FileInfo(std::get<base::FilePath>(param),
                                      /*display_name=*/base::FilePath())}));
      break;
    }
    case CommandType::kEditWithMediaApp:
    case CommandType::kOpenWithMediaApp: {
      SystemAppLaunchParams app_launch_params;
      app_launch_params.launch_paths.push_back(std::get<base::FilePath>(param));
      LaunchSystemWebAppAsync(profile_, SystemWebAppType::MEDIA,
                              app_launch_params);
      break;
    }
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
    case CommandType::kViewDetailsInBrowser:
      download_status_updater_->ShowInBrowser(
          /*guid=*/std::get<std::string>(param),
          /*callback=*/base::DoNothing());
      break;
  }
}

void DisplayManager::Remove(const std::string& guid) {
  for (auto& client : clients_) {
    client->Remove(guid);
  }
}

}  // namespace ash::download_status
