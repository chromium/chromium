// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_handler.h"

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"

namespace chromeos::cloud_upload {
namespace {

// The default folder where the file should be uploaded.
const char kDestinationFolder[] = "from Chromebook";

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    storage::FileSystemContext* file_system_context,
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
                       GURL hosted_url) {
  std::move(callback).Run(hosted_url);
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
                                       const storage::FileSystemURL& source_url)
    : profile_(profile),
      drive_integration_service_(
          drive::DriveIntegrationServiceFactory::FindForProfile(profile)),
      source_url_(source_url) {
  observed_task_id_ = -1;
  upload_done_ = false;
  error_found_ = false;

  if (!profile_) {
    LOG(ERROR) << "No profile";
    error_found_ = true;
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    error_found_ = true;
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No volume_manager or task_controller";
    error_found_ = true;
    return;
  }

  if (!drive_integration_service_) {
    LOG(ERROR) << "No drive integration service";
    error_found_ = true;
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  // Observe Drive updates.
  drive_integration_service_->GetDriveFsHost()->AddObserver(this);
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

  if (error_found_) {
    // TODO (b/243095484) Define error behavior.
    std::move(callback_).Run(GURL());
    return;
  }

  // Filesystem context.
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileSystemContextForSourceURL(
          profile_, file_manager::util::GetFileManagerURL());

  // Source and destination urls.
  std::vector<storage::FileSystemURL> source_urls{source_url_};
  base::FilePath destination_folder_path = GenerateUploadFolderPath(profile_);
  // TODO (b/243095484) Define error behavior.
  if (destination_folder_path.empty()) {
    LOG(ERROR) << "Unable to generate destination folder, the drive "
                  "integration service might not be available.";
    std::move(callback_).Run(GURL());
    return;
  }
  storage::FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context, destination_folder_path);

  // TODO (b/242685159) Change copy to move.
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kCopy, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void CloudUploadHandler::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id != observed_task_id_) {
    return;
  }
  // TODO (b/242685213) Add notification.
  LOG(ERROR) << "total bytes: " << status.total_bytes
             << " bytes transferred: " << status.bytes_transferred;
  if (observed_relative_drive_path_.empty()) {
    // TODO (b/242685536) Define multiple-file handling.
    DCHECK(status.sources.size() == 1);
    DCHECK(status.outputs.size() == 1);

    if (!drive_integration_service_) {
      LOG(ERROR) << "No drive integration service";
      if (callback_) {
        std::move(callback_).Run(GURL());
      }
      return;
    }

    // Get the output path from the IOTaskController's ProgressStatus. The
    // destination file name is not known in advance, given that it's generated
    // from the IOTaskController which resolves potential name clashes.
    drive_integration_service_->GetRelativeDrivePath(
        status.outputs[0].url.path(), &observed_relative_drive_path_);
  }
}

void CloudUploadHandler::OnUnmounted() {}

void CloudUploadHandler::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  for (const auto& item : syncing_status.item_events) {
    if (base::FilePath(item->path) != observed_relative_drive_path_) {
      continue;
    }
    // TODO (b/242685213) Add notification.
    switch (item->state) {
      case drivefs::mojom::ItemEvent::State::kQueued:
        LOG(ERROR) << "Drive -- QUEUED";
        return;
      case drivefs::mojom::ItemEvent::State::kInProgress:
        LOG(ERROR) << "Drive -- INPROGRESS: " << item->bytes_transferred
                   << " - " << item->bytes_to_transfer;
        return;
      case drivefs::mojom::ItemEvent::State::kCompleted:
        LOG(ERROR) << "Drive -- COMPLETED: " << item->bytes_transferred << " - "
                   << item->bytes_to_transfer;
        return;
      case drivefs::mojom::ItemEvent::State::kFailed:
        // TODO (b/243095484) Define error behavior.
        LOG(ERROR) << "Drive -- FAILED";
        if (callback_) {
          std::move(callback_).Run(GURL());
        }
        return;
      default:
        LOG(ERROR) << "Drive -- Invalid state";
        if (callback_) {
          std::move(callback_).Run(GURL());
        }
        return;
    }
  }
}

// If a `kModify` event has been dispatched for the uploaded file, check the
// file's metadata to see if its alternate URL is available, in which case the
// upload is complete. TODO (b/243638305) Add a timeout for how long we want to
// wait for the alternate URL.
void CloudUploadHandler::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  for (const auto& change : changes) {
    if (base::FilePath(change.path) != observed_relative_drive_path_ ||
        change.type != drivefs::mojom::FileChange::Type::kModify) {
      continue;
    }

    if (upload_done_) {
      return;
    }

    if (!drive_integration_service_) {
      LOG(ERROR) << "No drive integration service";
      if (callback_) {
        std::move(callback_).Run(GURL());
      }
      return;
    }
    drive_integration_service_->GetDriveFsInterface()->GetMetadata(
        observed_relative_drive_path_,
        base::BindOnce(&CloudUploadHandler::OnGetDriveMetadata,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CloudUploadHandler::OnError(const drivefs::mojom::DriveError& error) {
  if (base::FilePath(error.path) != observed_relative_drive_path_) {
    return;
  }
  // TODO (b/243095484) Define error behavior.
  LOG(ERROR) << "Drive -- FAILED";
  switch (error.type) {
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFull:
      LOG(ERROR) << "type: kCantUploadStorageFull";
      break;
    case drivefs::mojom::DriveError::Type::kPinningFailedDiskFull:
      LOG(ERROR) << "type: kPinningFailedDiskFull";
      break;
    default:
      LOG(ERROR) << "Invalid type";
  }
  observed_relative_drive_path_ = base::FilePath();

  if (callback_) {
    std::move(callback_).Run(GURL());
  }
}

void CloudUploadHandler::OnGetDriveMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  if (!hosted_url.is_valid()) {
    return;
  }
  upload_done_ = true;
  observed_relative_drive_path_ = base::FilePath();
  if (callback_) {
    std::move(callback_).Run(hosted_url);
  }
}

}  // namespace chromeos::cloud_upload
