// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/message_formatter.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/delete_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
                  absl::optional<GURL> hosted_url,
                  int64_t upload_size) {
  std::move(callback).Run(std::move(hosted_url), upload_size);
}

std::string GetTargetAppName(base::FilePath file_path) {
  const std::string extension = base::ToLowerASCII(file_path.FinalExtension());
  if (base::Contains(file_manager::file_tasks::WordGroupExtensions(),
                     extension)) {
    return l10n_util::GetStringUTF8(IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_DOCS);
  }
  if (base::Contains(file_manager::file_tasks::ExcelGroupExtensions(),
                     extension)) {
    return l10n_util::GetStringUTF8(IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_SHEETS);
  }
  if (base::Contains(file_manager::file_tasks::PowerPointGroupExtensions(),
                     extension)) {
    return l10n_util::GetStringUTF8(IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_SLIDES);
  }
  return l10n_util::GetStringUTF8(IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_DOCS);
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
      upload_type_(GetUploadType(profile, source_url)),
      notification_manager_(
          base::MakeRefCounted<CloudUploadNotificationManager>(
              profile,
              source_url.path().BaseName().value(),
              l10n_util::GetStringUTF8(IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE),
              GetTargetAppName(source_url.path()),
              // TODO(b/242685536) Update when support for multi-files is added.
              /*num_files=*/1,
              upload_type_)),
      source_url_(source_url) {
  observed_copy_task_id_ = -1;
  observed_delete_task_id_ = -1;
}

DriveUploadHandler::~DriveUploadHandler() = default;

void DriveUploadHandler::Run(UploadCallback callback) {
  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = std::move(callback);

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kOtherError);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile_);
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kOtherError);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kOtherError);
    return;
  }

  if (!drive_integration_service_) {
    LOG(ERROR) << "No Drive integration service";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kOtherError);
    return;
  }

  if (drive::util::GetDriveConnectionStatus(profile_) !=
      drive::util::ConnectionStatus::kConnected) {
    LOG(ERROR) << "No connection to Drive";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kNoConnection);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_observer_.Observe(io_task_controller_);

  // Observe Drive updates.
  drive_observer1_.Observe(drive_integration_service_);
  drive_observer2_.Observe(drive_integration_service_->GetDriveFsHost());

  if (!drive_integration_service_->IsMounted()) {
    LOG(ERROR) << "Google Drive is not mounted";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kFileSystemNotFound);
    return;
  }

  // Destination url.
  base::FilePath destination_folder_path =
      drive_integration_service_->GetMountPointPath().Append("root");
  FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path);
  // TODO (b/243095484) Define error behavior.
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder Drive URL";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kFileSystemNotFound);
    return;
  }

  std::vector<FileSystemURL> source_urls{source_url_};
  // Always use a copy task. Will convert to a move upon success.
  std::unique_ptr<file_manager::io_task::IOTask> copy_task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kCopy, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_copy_task_id_ = io_task_controller_->Add(std::move(copy_task));
}

void DriveUploadHandler::UpdateProgressNotification() {
  // The move progress and the syncing progress arbitrarily respectively account
  // for 20% and 80% of the upload workflow.
  int progress = move_progress_ * 0.2 + sync_progress_ * 0.8;
  notification_manager_->ShowUploadProgress(progress);
}

void DriveUploadHandler::OnEndCopy(base::expected<GURL, std::string> hosted_url,
                                   OfficeFilesUploadResult result_metric) {
  if (copy_ended_) {
    // Prevent loops in case Copy IO task and Drive sync fail separately.
    return;
  }
  copy_ended_ = true;

  // If copy to Drive was successful and intended operation is a copy, no delete
  // is required.
  if (hosted_url.has_value() && upload_type_ == UploadType::kCopy) {
    OnEndUpload(hosted_url, result_metric);
    return;
  }

  // If destination file doesn't exist, no delete is required.
  base::FilePath rel_path;
  bool destination_file_exists =
      !observed_absolute_dest_path_.empty() &&
      drive_integration_service_->GetRelativeDrivePath(
          observed_absolute_dest_path_, &rel_path);
  if (!destination_file_exists) {
    OnEndUpload(hosted_url, result_metric);
    return;
  }

  end_upload_callback_ =
      base::BindOnce(&DriveUploadHandler::OnEndUpload,
                     weak_ptr_factory_.GetWeakPtr(), hosted_url, result_metric);

  std::vector<FileSystemURL> file_urls;
  if (hosted_url.has_value()) {
    // If copy to Drive was successful, delete source file to convert the upload
    // to a move to Drive.
    file_urls.push_back(source_url_);
  } else {
    // If copy to Drive was unsuccessful, delete destination file to undo the
    // copy to Drive.
    FileSystemURL dest_url = FilePathToFileSystemURL(
        profile_, file_system_context_, observed_absolute_dest_path_);
    file_urls.push_back(dest_url);
  }

  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::DeleteIOTask>(
          std::move(file_urls), file_system_context_,
          /*show_notification=*/false);
  observed_delete_task_id_ = io_task_controller_->Add(std::move(task));
}

void DriveUploadHandler::OnEndUpload(
    base::expected<GURL, std::string> hosted_url,
    OfficeFilesUploadResult result_metric) {
  UMA_HISTOGRAM_ENUMERATION(kGoogleDriveUploadResultMetricName, result_metric);
  // TODO (b/243095484) Define error behavior on invalid hosted URL.
  observed_relative_drive_path_.clear();
  // Stop suppressing Drive events for the observed file.
  scoped_suppress_drive_notifications_for_path_.reset();
  if (hosted_url.has_value()) {
    // Resolve notifications.
    if (notification_manager_) {
      notification_manager_->MarkUploadComplete();
    }
    if (callback_) {
      std::move(callback_).Run(hosted_url.value(), upload_size_);
    }
  } else {
    if (const std::string& error_message = hosted_url.error();
        notification_manager_ && !error_message.empty()) {
      LOG(ERROR) << "Upload to Google Drive: " << error_message;
      notification_manager_->ShowUploadError(error_message);
    }
    if (callback_) {
      std::move(callback_).Run(absl::nullopt, 0);
    }
  }
}

void DriveUploadHandler::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id == observed_copy_task_id_) {
    OnCopyStatus(status);
    return;
  }
  if (status.task_id == observed_delete_task_id_) {
    OnDeleteStatus(status);
    return;
  }
}

void DriveUploadHandler::OnCopyStatus(
    const ::file_manager::io_task::ProgressStatus& status) {
  switch (status.state) {
    case file_manager::io_task::State::kScanning:
      // TODO(crbug.com/1361915): Potentially adapt to show scanning.
    case file_manager::io_task::State::kQueued:
      return;
    case file_manager::io_task::State::kInProgress:
      if (status.total_bytes > 0) {
        upload_size_ = status.total_bytes;
        move_progress_ = 100 * status.bytes_transferred / status.total_bytes;
      }
      UpdateProgressNotification();
      if (observed_relative_drive_path_.empty()) {
        // TODO (b/242685536) Define multiple-file handling.
        DCHECK_EQ(status.sources.size(), 1u);
        DCHECK_EQ(status.outputs.size(), 1u);

        if (!drive_integration_service_) {
          LOG(ERROR) << "No Drive integration service";
          OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                    OfficeFilesUploadResult::kOtherError);
          return;
        }

        // Get the output path from the IOTaskController's ProgressStatus. The
        // destination file name is not known in advance, given that it's
        // generated from the IOTaskController which resolves potential name
        // clashes.
        observed_absolute_dest_path_ = status.outputs[0].url.path();
        drive_integration_service_->GetRelativeDrivePath(
            observed_absolute_dest_path_, &observed_relative_drive_path_);
        scoped_suppress_drive_notifications_for_path_ = std::make_unique<
            file_manager::ScopedSuppressDriveNotificationsForPath>(
            profile_, observed_relative_drive_path_);
      }
      return;
    case file_manager::io_task::State::kPaused:
      return;
    case file_manager::io_task::State::kSuccess:
      move_progress_ = 100;
      notification_manager_->SetDestinationPath(status.outputs[0].url.path());
      UpdateProgressNotification();
      DCHECK_EQ(status.outputs.size(), 1u);
      return;
    case file_manager::io_task::State::kCancelled:
      LOG(ERROR) << "Upload to Google Drive cancelled";
      if (upload_type_ == UploadType::kCopy) {
        OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                  OfficeFilesUploadResult::kCopyOperationCancelled);
      } else {
        OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                  OfficeFilesUploadResult::kMoveOperationCancelled);
      }
      return;
    case file_manager::io_task::State::kError:
      ShowIOTaskError(status);
      return;
    case file_manager::io_task::State::kNeedPassword:
      NOTREACHED() << "Encrypted file should not need password to be copied or "
                      "moved. Case should not be reached.";
      return;
  }
}

void DriveUploadHandler::OnDeleteStatus(
    const ::file_manager::io_task::ProgressStatus& status) {
  switch (status.state) {
    case file_manager::io_task::State::kCancelled:
      NOTREACHED() << "Deletion of source or destination file should not have "
                      "been cancelled.";
      ABSL_FALLTHROUGH_INTENDED;
    case file_manager::io_task::State::kError:
    case file_manager::io_task::State::kSuccess:
      std::move(end_upload_callback_).Run();
      return;
    default:
      return;
  }
}

void DriveUploadHandler::ShowIOTaskError(
    const file_manager::io_task::ProgressStatus& status) {
  OfficeFilesUploadResult upload_result;
  std::string error_message;
  bool copy = upload_type_ == UploadType::kCopy;

  // TODO(b/242685536) Find most relevant error in a multi-file upload when
  // support for multi-files is added.
  base::File::Error file_error =
      GetFirstTaskError(status).value_or(base::File::FILE_ERROR_FAILED);

  base::UmaHistogramExactLinear(
      copy ? kGoogleDriveCopyErrorMetricName : kGoogleDriveMoveErrorMetricName,
      -file_error, -base::File::FILE_ERROR_MAX);

  switch (file_error) {
    case base::File::FILE_ERROR_NO_SPACE:
      upload_result = OfficeFilesUploadResult::kCloudQuotaFull;
      // TODO(b/242685536) Use "these files" for multi-files when support for
      // multi-files is added.
      error_message = base::UTF16ToUTF8(
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(
                  copy ? IDS_OFFICE_UPLOAD_ERROR_FREE_UP_SPACE_TO_COPY
                       : IDS_OFFICE_UPLOAD_ERROR_FREE_UP_SPACE_TO_MOVE),
              // TODO(b/242685536) Update when support for multi-files is added.
              1,
              l10n_util::GetStringUTF16(
                  IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE_SHORT)));
      break;
    case base::File::FILE_ERROR_NOT_FOUND:
      if (copy) {
        upload_result = OfficeFilesUploadResult::kCopyOperationError;
      } else {
        upload_result = OfficeFilesUploadResult::kMoveOperationError;
      }
      error_message = l10n_util::GetStringUTF8(
          copy ? IDS_OFFICE_UPLOAD_ERROR_FILE_NOT_EXIST_TO_COPY
               : IDS_OFFICE_UPLOAD_ERROR_FILE_NOT_EXIST_TO_MOVE);
      break;
    default:
      if (copy) {
        upload_result = OfficeFilesUploadResult::kCopyOperationError;
      } else {
        upload_result = OfficeFilesUploadResult::kMoveOperationError;
      }
      LOG(ERROR) << "IO Task error";
      error_message = GetGenericErrorMessage();
  }

  OnEndCopy(base::unexpected(error_message), upload_result);
}

void DriveUploadHandler::OnUnmounted() {}

void DriveUploadHandler::ImmediatelyUploadDone(drive::FileError error) {
  LOG_IF(ERROR, error != drive::FileError::FILE_ERROR_OK)
      << "ImmediatelyUpload failed with status: " << error;
}

void DriveUploadHandler::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  for (const auto& item : syncing_status.item_events) {
    if (base::FilePath(item->path) != observed_relative_drive_path_) {
      continue;
    }
    switch (item->state) {
      case drivefs::mojom::ItemEvent::State::kQueued: {
        // Tell Drive to upload the file now. If successful, we will receive a
        // kInProgress or kCompleted event sooner. If this fails, we ignore it.
        // The file will get uploaded eventually.
        drive_integration_service_->ImmediatelyUpload(
            observed_relative_drive_path_,
            base::BindOnce(&DriveUploadHandler::ImmediatelyUploadDone,
                           weak_ptr_factory_.GetWeakPtr()));
        return;
      }
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
        LOG(ERROR) << "Drive sync error";
        OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                  OfficeFilesUploadResult::kCloudError);
        return;
      default:
        LOG(ERROR) << "Drive sync error + invalid sync state";
        OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                  OfficeFilesUploadResult::kCloudError);
        return;
    }
  }
}

void DriveUploadHandler::OnError(const drivefs::mojom::DriveError& error) {
  if (base::FilePath(error.path) != observed_relative_drive_path_) {
    return;
  }
  bool copy = upload_type_ == UploadType::kCopy;
  switch (error.type) {
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFull:
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFullOrganization:
    case drivefs::mojom::DriveError::Type::kCantUploadSharedDriveStorageFull:
      OnEndCopy(
          base::unexpected(base::UTF16ToUTF8(
              base::i18n::MessageFormatter::FormatWithNumberedArgs(
                  l10n_util::GetStringUTF16(
                      copy ? IDS_OFFICE_UPLOAD_ERROR_FREE_UP_SPACE_TO_COPY
                           : IDS_OFFICE_UPLOAD_ERROR_FREE_UP_SPACE_TO_MOVE),
                  // TODO(b/242685536) Update when support for
                  // multi-files is added.
                  1,
                  l10n_util::GetStringUTF16(
                      IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE_SHORT)))),
          OfficeFilesUploadResult::kCloudQuotaFull);
      break;
    case drivefs::mojom::DriveError::Type::kPinningFailedDiskFull:
      LOG(ERROR) << "Pinning failed, disk full";
      OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kPinningFailedDiskFull);
      break;
    default:
      LOG(ERROR) << "Cloud error";
      OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kCloudError);
  }
}

void DriveUploadHandler::OnDriveIntegrationServiceDestroyed() {
  drive_observer2_.Reset();
  drive_observer1_.Reset();
}

void DriveUploadHandler::OnDriveConnectionStatusChanged(
    drive::util::ConnectionStatus status) {
  if (status != drive::util::ConnectionStatus::kConnected) {
    LOG(ERROR) << "Lost connection to Drive during upload";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kNoConnection);
  }
}

void DriveUploadHandler::OnGetDriveMetadata(
    bool timed_out,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    if (timed_out) {
      LOG(ERROR) << "Drive Metadata error";
      OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kCloudMetadataError);
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
      LOG(ERROR) << "Invalid alternate URL - Drive editing unavailable";
      OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kCloudMetadataError);
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
      LOG(ERROR) << "Unexpected alternate URL - Drive editing unavailable";
      OnEndCopy(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kCloudMetadataError);
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
  OnEndCopy(hosted_url, OfficeFilesUploadResult::kSuccess);
}

void DriveUploadHandler::CheckAlternateUrl(bool timed_out) {
  if (!drive_integration_service_) {
    LOG(ERROR) << "No Drive integration service";
    OnEndCopy(base::unexpected(GetGenericErrorMessage()),
              OfficeFilesUploadResult::kOtherError);
    return;
  }

  drive_integration_service_->GetDriveFsInterface()->GetMetadata(
      observed_relative_drive_path_,
      base::BindOnce(&DriveUploadHandler::OnGetDriveMetadata,
                     weak_ptr_factory_.GetWeakPtr(), /*timed_out=*/timed_out));
}

}  // namespace ash::cloud_upload
