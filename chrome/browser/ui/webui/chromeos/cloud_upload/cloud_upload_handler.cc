// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_handler.h"

#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

namespace chromeos::cloud_upload {
namespace {

// The default folder where the file should be uploaded.
const char kDestinationFolder[] = "from Chromebook";

// The maximum amount of time allowed, in seconds, between the syncing
// completion of a file and the update of its metadata with the expected (Google
// editor) alternate URL.
const int kAlternateUrlTimeout = 15;

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath file_path) {
  GURL url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    LOG(ERROR) << "Unable to ConvertAbsoluteFilePathToFileSystemUrl";
    return storage::FileSystemURL();
  }

  return file_system_context->CrackURLInFirstPartyContext(url);
}

// Runs the callback provided to `CloudUploadHandler::UploadToCloud`.
void UploadToCloudDone(scoped_refptr<CloudUploadHandler> cloud_upload_handler,
                       CloudUploadHandler::UploadCallback callback,
                       const GURL& hosted_url) {
  std::move(callback).Run(hosted_url);
}

void CreateDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemURL destination_folder_url,
    base::OnceCallback<void(base::File::Error)> complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_context->operation_runner()->CreateDirectory(
      destination_folder_url, /*exclusive=*/false, /*recursive=*/false,
      std::move(complete_callback));
}

}  // namespace

// static.
base::FilePath CloudUploadHandler::GenerateUploadFolderPath(Profile* profile) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return integration_service
             ? integration_service->GetMountPointPath().Append("root").Append(
                   kDestinationFolder)
             : base::FilePath();
}

// static.
void CloudUploadHandler::UploadToCloud(Profile* profile,
                                       const storage::FileSystemURL& source_url,
                                       UploadCallback callback) {
  scoped_refptr<CloudUploadHandler> cloud_upload_handler =
      new CloudUploadHandler(profile, source_url);
  // Keep `cloud_upload_handler` alive until `UploadToCloudDone` executes.
  cloud_upload_handler->Run(base::BindOnce(
      &UploadToCloudDone, cloud_upload_handler, std::move(callback)));
}

CloudUploadHandler::CloudUploadHandler(Profile* profile,
                                       const storage::FileSystemURL source_url)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      drive_integration_service_(
          drive::DriveIntegrationServiceFactory::FindForProfile(profile)),
      notification_manager_(
          base::MakeRefCounted<CloudUploadNotificationManager>(profile)),
      source_url_(source_url) {
  observed_task_id_ = -1;
}

CloudUploadHandler::~CloudUploadHandler() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }

  // Stop observing Drive updates.
  if (drive_integration_service_) {
    drive_integration_service_->GetDriveFsHost()->RemoveObserver(this);
  }
}

void CloudUploadHandler::Run(UploadCallback callback) {
  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = std::move(callback);

  if (!profile_) {
    OnEndUpload(GURL(), "No profile");
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
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
  base::FilePath destination_folder_path = GenerateUploadFolderPath(profile_);
  if (destination_folder_path.empty()) {
    OnEndUpload(GURL(),
                "Unable to generate destination folder path, the drive "
                "integration service might not be available");
    return;
  }
  storage::FileSystemURL destination_folder_url = FilePathToFileSystemURL(
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
                         &CloudUploadHandler::OnDestinationDirectoryCreated,
                         this, destination_folder_url))));
}

void CloudUploadHandler::UpdateProgressNotification() {
  // The move progress and the syncing progress arbitrarily respectively account
  // for 20% and 80% of the upload workflow.
  int progress = move_progress_ * 0.2 + sync_progress_ * 0.8;
  notification_manager_->ShowProgress(progress);
}

void CloudUploadHandler::OnEndUpload(GURL hosted_url,
                                     std::string error_message) {
  // TODO (b/243095484) Define error behavior on invalid hosted URL.
  observed_relative_drive_path_.clear();
  // Stop suppressing Drive events for the observed file.
  scoped_suppress_drive_notifications_for_path_.reset();
  // Resolve notifications.
  if (notification_manager_) {
    if (hosted_url.is_valid()) {
      notification_manager_->Completed();
    } else if (!error_message.empty()) {
      LOG(ERROR) << "Cloud upload: " << error_message;
      notification_manager_->ShowError(error_message);
    }
  }
  if (callback_) {
    std::move(callback_).Run(hosted_url);
  }
}

void CloudUploadHandler::OnDestinationDirectoryCreated(
    storage::FileSystemURL destination_folder_url,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    OnEndUpload(GURL(), "Unable to create destination folder");
    return;
  }
  if (!destination_folder_url.is_valid()) {
    OnEndUpload(GURL(), "Received destination URL is invalid");
    return;
  }

  // Source URLs.
  std::vector<storage::FileSystemURL> source_urls{source_url_};

  // TODO (b/242685159) Change copy to move.
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kCopy, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void CloudUploadHandler::OnFileShownInFolder(
    platform_util::OpenOperationResult result) {
  if (result == platform_util::OPEN_SUCCEEDED) {
    return;
  }
  std::string error_string = "";
  switch (result) {
    case platform_util::OpenOperationResult::OPEN_SUCCEEDED:
      error_string = "OPEN_SUCCEEDED";
      break;
    case platform_util::OpenOperationResult::OPEN_FAILED_PATH_NOT_FOUND:
      error_string = "OPEN_FAILED_PATH_NOT_FOUND";
      break;
    case platform_util::OpenOperationResult::OPEN_FAILED_INVALID_TYPE:
      error_string = "OPEN_FAILED_INVALID_TYPE";
      break;
    case platform_util::OpenOperationResult::
        OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE:
      error_string = "OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE";
      break;
    case platform_util::OpenOperationResult::OPEN_FAILED_FILE_ERROR:
      error_string = "OPEN_FAILED_FILE_ERROR";
      break;
  }
  LOG(ERROR) << "Failed to show destination file in Files app : "
             << error_string;
}

void CloudUploadHandler::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id != observed_task_id_) {
    return;
  }
  switch (status.state) {
    case file_manager::io_task::State::kQueued:
      return;
    case file_manager::io_task::State::kInProgress:
      if (status.total_bytes > 0) {
        move_progress_ = 100 * status.bytes_transferred / status.total_bytes;
      }
      UpdateProgressNotification();
      if (observed_relative_drive_path_.empty()) {
        // TODO (b/242685536) Define multiple-file handling.
        DCHECK_EQ(status.sources.size(), 1);
        DCHECK_EQ(status.outputs.size(), 1);

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
    case file_manager::io_task::State::kSuccess:
      move_progress_ = 100;
      UpdateProgressNotification();
      DCHECK_EQ(status.outputs.size(), 1);
      file_manager::util::ShowItemInFolder(
          profile_, status.outputs[0].url.path(),
          base::BindOnce(&CloudUploadHandler::OnFileShownInFolder,
                         weak_ptr_factory_.GetWeakPtr()));
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

void CloudUploadHandler::OnUnmounted() {}

void CloudUploadHandler::OnSyncingStatusUpdate(
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
        alternate_url_timer_.Start(
            FROM_HERE, base::Seconds(kAlternateUrlTimeout),
            base::BindOnce(&CloudUploadHandler::OnAlternateUrlTimeout,
                           weak_ptr_factory_.GetWeakPtr()));
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

// If a `kModify` event has been dispatched for the uploaded file, check the
// file's metadata to see if its alternate URL is available, in which case the
// upload is complete.
void CloudUploadHandler::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  for (const auto& change : changes) {
    if (base::FilePath(change.path) != observed_relative_drive_path_ ||
        change.type != drivefs::mojom::FileChange::Type::kModify) {
      continue;
    }

    if (!drive_integration_service_) {
      OnEndUpload(GURL(), "No drive integration service");
      return;
    }
    drive_integration_service_->GetDriveFsInterface()->GetMetadata(
        observed_relative_drive_path_,
        base::BindOnce(&CloudUploadHandler::OnGetDriveMetadata,
                       weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/false));
  }
}

void CloudUploadHandler::OnError(const drivefs::mojom::DriveError& error) {
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

void CloudUploadHandler::OnGetDriveMetadata(
    bool timed_out,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  if (!hosted_url.is_valid()) {
    if (timed_out) {
      OnEndUpload(GURL(), "Invalid alternate URL - Drive editing unavailable");
    }
    return;
  }

  // URLs for editing Office files in Web Drive all have a "docs.google.com"
  // host.
  if (hosted_url.host() != "docs.google.com") {
    if (timed_out) {
      OnEndUpload(GURL(),
                  "Unexpected alternate URL - Drive editing unavailable");
    }
    return;
  }

  // Success.
  alternate_url_timer_.Stop();
  OnEndUpload(hosted_url);
}

void CloudUploadHandler::OnAlternateUrlTimeout() {
  if (!drive_integration_service_) {
    OnEndUpload(GURL(), "No drive integration service");
    return;
  }

  drive_integration_service_->GetDriveFsInterface()->GetMetadata(
      observed_relative_drive_path_,
      base::BindOnce(&CloudUploadHandler::OnGetDriveMetadata,
                     weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/true));
}

}  // namespace chromeos::cloud_upload
