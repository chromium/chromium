// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

using storage::FileSystemURL;

namespace ash::cloud_upload {
namespace {

// The maximum amount of time allowed, in seconds, between the syncing
// completion of a file and the update of its metadata with the expected (Google
// editor) alternate URL.
const int kAlternateUrlTimeout = 15;

// The polling interval, in milliseconds, for querying the uploaded file's
// alternate URL.
const int kAlternateUrlPollInterval = 200;

// Runs the callback provided to `DriveUploadHandler::Upload`.
void OnUploadDone(scoped_refptr<DriveUploadHandler> drive_upload_handler,
                  DriveUploadHandler::UploadCallback callback,
                  const GURL& hosted_url) {
  std::move(callback).Run(hosted_url);
}

std::string GetTargetAppName(base::FilePath file_path) {
  const std::string extension = file_path.FinalExtension();
  if (extension == "doc" || extension == "docx") {
    return "Google Docs";
  }
  if (extension == "xls" || extension == "xlsx") {
    return "Google Sheets";
  }
  if (extension == "ppt" || extension == "pptx") {
    return "Google Slides";
  }
  return "Google Docs";
}

}  // namespace

// static.
void DriveUploadHandler::Upload(Profile* profile,
                                const FileSystemURL& source_url,
                                UploadCallback callback) {
  scoped_refptr<DriveUploadHandler> drive_upload_handler =
      new DriveUploadHandler(profile, source_url);
  // Keep `drive_upload_handler` alive until `UploadDone` executes.
  drive_upload_handler->Run(
      base::BindOnce(&OnUploadDone, drive_upload_handler, std::move(callback)));
}

DriveUploadHandler::DriveUploadHandler(Profile* profile,
                                       const FileSystemURL source_url)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      drive_integration_service_(
          drive::DriveIntegrationServiceFactory::FindForProfile(profile)),
      notification_manager_(
          base::MakeRefCounted<CloudUploadNotificationManager>(
              profile,
              source_url.path().BaseName().value(),
              "Drive",
              GetTargetAppName(source_url.path()))),
      source_url_(source_url) {
  observed_task_id_ = -1;
}

DriveUploadHandler::~DriveUploadHandler() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }

  // Stop observing Drive updates.
  if (drive_integration_service_) {
    drive_integration_service_->GetDriveFsHost()->RemoveObserver(this);
  }
}

void DriveUploadHandler::Run(UploadCallback callback) {
  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = std::move(callback);

  if (!profile_) {
    OnEndUpload(GURL(), "No profile");
    return;
  }

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile_);
  if (!volume_manager) {
    OnEndUpload(GURL(), "No volume manager");
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    OnEndUpload(GURL(), "No task_controller");
    return;
  }

  if (!drive_integration_service_) {
    OnEndUpload(GURL(), "No drive integration service");
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  // Observe Drive updates.
  drive_integration_service_->GetDriveFsHost()->AddObserver(this);

  // Destination url.
  base::FilePath destination_folder_path =
      drive_integration_service_->GetMountPointPath().Append("root").Append(
          kDestinationFolder);
  FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path);
  // TODO (b/243095484) Define error behavior.
  if (!destination_folder_url.is_valid()) {
    OnEndUpload(GURL(), "Unable to generate destination folder URL");
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateDirectoryOnIOThread, file_system_context_,
                     destination_folder_url,
                     google_apis::CreateRelayCallback(base::BindOnce(
                         &DriveUploadHandler::OnDestinationDirectoryCreated,
                         this, destination_folder_url))));
}

void DriveUploadHandler::UpdateProgressNotification() {
  // The move progress and the syncing progress arbitrarily respectively account
  // for 20% and 80% of the upload workflow.
  int progress = move_progress_ * 0.2 + sync_progress_ * 0.8;
  notification_manager_->ShowUploadProgress(progress);
}

void DriveUploadHandler::OnEndUpload(GURL hosted_url,
                                     std::string error_message) {
  // TODO (b/243095484) Define error behavior on invalid hosted URL.
  observed_relative_drive_path_.clear();
  // Stop suppressing Drive events for the observed file.
  scoped_suppress_drive_notifications_for_path_.reset();
  // Resolve notifications.
  if (notification_manager_) {
    if (hosted_url.is_valid()) {
      notification_manager_->MarkUploadComplete();
    } else if (!error_message.empty()) {
      LOG(ERROR) << "Cloud upload: " << error_message;
      notification_manager_->ShowUploadError(error_message);
    }
  }
  if (callback_) {
    std::move(callback_).Run(hosted_url);
  }
}

void DriveUploadHandler::OnDestinationDirectoryCreated(
    FileSystemURL destination_folder_url,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    OnEndUpload(GURL(), "Unable to create destination folder");
    return;
  }
  if (!destination_folder_url.is_valid()) {
    OnEndUpload(GURL(), "Received destination URL is invalid");
    return;
  }

  std::vector<FileSystemURL> source_urls{source_url_};
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kMove, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void DriveUploadHandler::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id != observed_task_id_) {
    return;
  }
  switch (status.state) {
    case file_manager::io_task::State::kScanning:
      // TODO(crbug.com/1361915): Potentially adapt to show scanning.
    case file_manager::io_task::State::kQueued:
      return;
    case file_manager::io_task::State::kInProgress:
      if (status.total_bytes > 0) {
        move_progress_ = 100 * status.bytes_transferred / status.total_bytes;
      }
      UpdateProgressNotification();
      if (observed_relative_drive_path_.empty()) {
        // TODO (b/242685536) Define multiple-file handling.
        DCHECK_EQ(status.sources.size(), 1u);
        DCHECK_EQ(status.outputs.size(), 1u);

        if (!drive_integration_service_) {
          OnEndUpload(GURL(), "No drive integration service");
          return;
        }

        // Get the output path from the IOTaskController's ProgressStatus. The
        // destination file name is not known in advance, given that it's
        // generated from the IOTaskController which resolves potential name
        // clashes.
        drive_integration_service_->GetRelativeDrivePath(
            status.outputs[0].url.path(), &observed_relative_drive_path_);
        scoped_suppress_drive_notifications_for_path_ = std::make_unique<
            file_manager::ScopedSuppressDriveNotificationsForPath>(
            profile_, observed_relative_drive_path_);
      }
      return;
    case file_manager::io_task::State::kPaused:
      return;
    case file_manager::io_task::State::kSuccess:
      move_progress_ = 100;
      UpdateProgressNotification();
      DCHECK_EQ(status.outputs.size(), 1u);
      file_manager::util::ShowItemInFolder(
          profile_, status.outputs[0].url.path(),
          base::BindOnce(&LogErrorOnShowItemInFolder));
      return;
    case file_manager::io_task::State::kCancelled:
      OnEndUpload(GURL(), "Move error: kCancelled");
      return;
    case file_manager::io_task::State::kError:
      OnEndUpload(GURL(), "Move error: kError");
      return;
    case file_manager::io_task::State::kNeedPassword:
      OnEndUpload(GURL(), "Move error: kNeedPassword");
      return;
  }
}

void DriveUploadHandler::OnUnmounted() {}

void DriveUploadHandler::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  for (const auto& item : syncing_status.item_events) {
    if (base::FilePath(item->path) != observed_relative_drive_path_) {
      continue;
    }
    switch (item->state) {
      case drivefs::mojom::ItemEvent::State::kQueued:
        return;
      case drivefs::mojom::ItemEvent::State::kInProgress:
        if (item->bytes_transferred > 0) {
          sync_progress_ =
              100 * item->bytes_transferred / item->bytes_to_transfer;
        }
        UpdateProgressNotification();
        return;
      case drivefs::mojom::ItemEvent::State::kCompleted:
        sync_progress_ = 100;
        UpdateProgressNotification();
        // The file has fully synced. Start the timer for the maximum amount of
        // time we allow before the file's alternate URL is available.
        alternate_url_timeout_.Start(
            FROM_HERE, base::Seconds(kAlternateUrlTimeout),
            base::BindOnce(&DriveUploadHandler::CheckAlternateUrl,
                           weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/true));
        CheckAlternateUrl(/*timed_out=*/false);
        return;
      case drivefs::mojom::ItemEvent::State::kFailed:
        OnEndUpload(GURL(), "Drive sync error: kFailed");
        return;
      default:
        OnEndUpload(GURL(), "Drive sync error + invalid sync state");
        return;
    }
  }
}

void DriveUploadHandler::OnError(const drivefs::mojom::DriveError& error) {
  if (base::FilePath(error.path) != observed_relative_drive_path_) {
    return;
  }
  switch (error.type) {
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFull:
      OnEndUpload(GURL(), "Drive error: kCantUploadStorageFull");
      break;
    case drivefs::mojom::DriveError::Type::kPinningFailedDiskFull:
      OnEndUpload(GURL(), "Drive error: kPinningFailedDiskFull");
      break;
    default:
      OnEndUpload(GURL(), "Drive error + invalid error type...");
  }
}

void DriveUploadHandler::OnGetDriveMetadata(
    bool timed_out,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    if (timed_out) {
      OnEndUpload(GURL(), "Drive Metadata error");
    } else {
      alternate_url_poll_timer_.Start(
          FROM_HERE, base::Milliseconds(kAlternateUrlPollInterval),
          base::BindOnce(&DriveUploadHandler::CheckAlternateUrl,
                         weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/false));
    }
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  if (!hosted_url.is_valid()) {
    if (timed_out) {
      OnEndUpload(GURL(), "Invalid alternate URL - Drive editing unavailable");
    } else {
      alternate_url_poll_timer_.Start(
          FROM_HERE, base::Milliseconds(kAlternateUrlPollInterval),
          base::BindOnce(&DriveUploadHandler::CheckAlternateUrl,
                         weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/false));
    }
    return;
  }

  // URLs for editing Office files in Web Drive all have a "docs.google.com"
  // host.
  if (hosted_url.host() != "docs.google.com") {
    if (timed_out) {
      OnEndUpload(GURL(),
                  "Unexpected alternate URL - Drive editing unavailable");
    } else {
      alternate_url_poll_timer_.Start(
          FROM_HERE, base::Milliseconds(kAlternateUrlPollInterval),
          base::BindOnce(&DriveUploadHandler::CheckAlternateUrl,
                         weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/false));
    }
    return;
  }

  // Success.
  alternate_url_timeout_.Stop();
  alternate_url_poll_timer_.Stop();
  OnEndUpload(hosted_url);
}

void DriveUploadHandler::CheckAlternateUrl(bool timed_out) {
  if (!drive_integration_service_) {
    OnEndUpload(GURL(), "No drive integration service");
    return;
  }

  drive_integration_service_->GetDriveFsInterface()->GetMetadata(
      observed_relative_drive_path_,
      base::BindOnce(&DriveUploadHandler::OnGetDriveMetadata,
                     weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/timed_out));
}

}  // namespace ash::cloud_upload
