// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "ui/base/l10n/l10n_util.h"

using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::Service;
using storage::FileSystemURL;

namespace ash::cloud_upload {

OneDriveUploadHandler::OneDriveUploadHandler(
    Profile* profile,
    const storage::FileSystemURL& source_url,
    UploadCallback callback,
    base::SafeRef<CloudOpenMetrics> cloud_open_metrics)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      notification_manager_(
          base::MakeRefCounted<CloudUploadNotificationManager>(
              profile,
              l10n_util::GetStringUTF8(IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE),
              l10n_util::GetStringUTF8(IDS_OFFICE_FILE_HANDLER_APP_MICROSOFT),
              // TODO(b/242685536) Update when support for multi-files is added.
              /*num_files=*/1,
              GetUploadType(profile, source_url))),
      source_url_(source_url),
      callback_(std::move(callback)),
      cloud_open_metrics_(cloud_open_metrics) {
  observed_task_id_ = -1;
}

OneDriveUploadHandler::~OneDriveUploadHandler() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

void OneDriveUploadHandler::Run() {
  DCHECK(callback_);

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnFailedUpload(OfficeFilesUploadResult::kOtherError);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnFailedUpload(OfficeFilesUploadResult::kOtherError);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnFailedUpload(OfficeFilesUploadResult::kOtherError);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  GetODFSMetadataAndStartIOTask();
}

void OneDriveUploadHandler::GetODFSMetadataAndStartIOTask() {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    LOG(ERROR) << "ODFS not found";
    // TODO(b/293363474): Remove when the underlying cause is diagnosed.
    base::debug::DumpWithoutCrashing(FROM_HERE);
    OnFailedUpload(OfficeFilesUploadResult::kFileSystemNotFound);
    return;
  }

  FileSystemURL destination_folder_url =
      GetDestinationFolderUrl(file_system->GetFileSystemInfo());
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder ODFS URL";
    // TODO(b/293363474): Remove when the underlying cause is diagnosed.
    base::debug::DumpWithoutCrashing(FROM_HERE);
    OnFailedUpload(OfficeFilesUploadResult::kDestinationUrlError);
    return;
  }

  // First check that ODFS is not in the "ReauthenticationRequired" state.
  GetODFSMetadata(
      file_system,
      base::BindOnce(
          &OneDriveUploadHandler::CheckReauthenticationAndStartIOTask,
          weak_ptr_factory_.GetWeakPtr(), destination_folder_url));
}

void OneDriveUploadHandler::CheckReauthenticationAndStartIOTask(
    const FileSystemURL& destination_folder_url,
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  if (!metadata_or_error.has_value()) {
    // Try the copy/move anyway.
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata_or_error.error();
  } else if (metadata_or_error->reauthentication_required ||
             (metadata_or_error->account_state.has_value() &&
              metadata_or_error->account_state.value() ==
                  OdfsAccountState::kReauthenticationRequired)) {
    // TODO(b/330786891): Only query account_state once
    // reauthentication_required is no longer needed for backwards compatibility
    // with ODFS.
    if (tried_reauth_ || !base::FeatureList::IsEnabled(
                             ash::features::kOneDriveUploadImmediateReauth)) {
      // We do not expect this failure because it would mean we became de-auth'd
      // right after auth. Except when the immediate-reauth feature is on, then
      // it just means reauthentication is required and we have to ask the user.
      OnFailedUpload(
          OfficeFilesUploadResult::kUploadNotStartedReauthenticationRequired,
          GetReauthenticationRequiredMessage());
    } else {
      // Try to reauth immediately and then try the upload again.
      RequestODFSMount(profile_,
                       base::BindOnce(&OneDriveUploadHandler::OnMountResponse,
                                      weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  operation_type_ = GetUploadType(profile_, source_url_) == UploadType::kCopy
                        ? file_manager::io_task::OperationType::kCopy
                        : file_manager::io_task::OperationType::kMove;
  std::vector<FileSystemURL> source_urls{source_url_};
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          operation_type_, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void OneDriveUploadHandler::OnMountResponse(base::File::Error result) {
  if (result != base::File::FILE_OK) {
    OnFailedUpload(
        OfficeFilesUploadResult::kUploadNotStartedReauthenticationRequired,
        GetReauthenticationRequiredMessage());
    return;
  }
  tried_reauth_ = true;
  GetODFSMetadataAndStartIOTask();
}

FileSystemURL OneDriveUploadHandler::GetDestinationFolderUrl(
    file_system_provider::ProvidedFileSystemInfo odfs_info) {
  destination_folder_path_ = odfs_info.mount_path();
  return FilePathToFileSystemURL(profile_, file_system_context_,
                                 destination_folder_path_);
}

void OneDriveUploadHandler::OnSuccessfulUpload(
    OfficeFilesUploadResult result_metric,
    storage::FileSystemURL url) {
  cloud_open_metrics_->LogUploadResult(result_metric);
  // Show complete notification.
  if (notification_manager_) {
    notification_manager_->MarkUploadComplete();
  }
  const OfficeTaskResult task_result =
      operation_type_ == file_manager::io_task::OperationType::kCopy
          ? OfficeTaskResult::kCopied
          : OfficeTaskResult::kMoved;
  std::move(callback_).Run(task_result, url, upload_size_);
}

void OneDriveUploadHandler::OnFailedUpload(
    OfficeFilesUploadResult result_metric,
    std::string error_message) {
  cloud_open_metrics_->LogUploadResult(result_metric);
  // Show error notification.
  if (notification_manager_) {
    LOG(ERROR) << "Upload to OneDrive: " << error_message;
    notification_manager_->ShowUploadError(error_message);
  }
    std::move(callback_).Run(OfficeTaskResult::kFailedToUpload, std::nullopt,
                             0);
}

void OneDriveUploadHandler::OnIOTaskStatus(
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
        upload_size_ = status.total_bytes;
        notification_manager_->ShowUploadProgress(
            100 * status.bytes_transferred / status.total_bytes);
      }
      return;
    case file_manager::io_task::State::kPaused:
      return;
    case file_manager::io_task::State::kSuccess:
      notification_manager_->SetDestinationPath(status.outputs[0].url.path());
      notification_manager_->ShowUploadProgress(100);
      DCHECK_EQ(status.outputs.size(), 1u);
      if (tried_reauth_) {
        OnSuccessfulUpload(OfficeFilesUploadResult::kSuccessAfterReauth,
                           status.outputs[0].url);
      } else {
        OnSuccessfulUpload(OfficeFilesUploadResult::kSuccess,
                           status.outputs[0].url);
      }
      return;
    case file_manager::io_task::State::kCancelled:
      if (status.type == file_manager::io_task::OperationType::kCopy) {
        OnFailedUpload(OfficeFilesUploadResult::kCopyOperationCancelled);
      } else {
        OnFailedUpload(OfficeFilesUploadResult::kMoveOperationCancelled);
      }
      return;
    case file_manager::io_task::State::kError:
      ShowIOTaskError(status);
      return;
    case file_manager::io_task::State::kNeedPassword:
      NOTREACHED_IN_MIGRATION()
          << "Encrypted file should not need password to be copied or "
             "moved. Case should not be reached.";
      return;
  }
}

void OneDriveUploadHandler::OnGetReauthenticationRequired(
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  std::string error_message = GetGenericErrorMessage();
  OfficeFilesUploadResult upload_result =
      OfficeFilesUploadResult::kCloudAccessDenied;
  if (!metadata_or_error.has_value()) {
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata_or_error.error();
  } else if (metadata_or_error->reauthentication_required ||
             (metadata_or_error->account_state.has_value() &&
              metadata_or_error->account_state.value() ==
                  OdfsAccountState::kReauthenticationRequired)) {
    // TODO(b/330786891): Only query account_state once
    // reauthentication_required is no longer needed for backwards compatibility
    // with ODFS. Show the reauthentication required error notification.
    error_message = GetReauthenticationRequiredMessage();
    upload_result = OfficeFilesUploadResult::kCloudReauthRequired;
  }
  OnFailedUpload(upload_result, error_message);
}

void OneDriveUploadHandler::ShowAccessDeniedError() {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    LOG(ERROR) << "ODFS not found";
    OnFailedUpload(OfficeFilesUploadResult::kCloudAccessDenied);
    return;
  }
  GetODFSMetadata(
      file_system,
      base::BindOnce(&OneDriveUploadHandler::OnGetReauthenticationRequired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OneDriveUploadHandler::ShowIOTaskError(
    const file_manager::io_task::ProgressStatus& status) {
  OfficeFilesUploadResult upload_result;
  std::string error_message;
  bool copy = status.type == file_manager::io_task::OperationType::kCopy;

  // TODO(b/242685536) Find most relevant error in a multi-file upload when
  // support for multi-files is added.
  base::File::Error file_error =
      GetFirstTaskError(status).value_or(base::File::FILE_ERROR_FAILED);

  if (copy) {
    cloud_open_metrics_->LogCopyError(file_error);
  } else {
    cloud_open_metrics_->LogMoveError(file_error);
  }

  switch (file_error) {
    case base::File::FILE_ERROR_ACCESS_DENIED:
      ShowAccessDeniedError();
      return;
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
                  IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE_SHORT)));
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
    case base::File::FILE_ERROR_INVALID_URL:
      if (copy) {
        upload_result = OfficeFilesUploadResult::kCopyOperationError;
      } else {
        upload_result = OfficeFilesUploadResult::kMoveOperationError;
      }
      error_message =
          l10n_util::GetStringUTF8(IDS_OFFICE_UPLOAD_ERROR_REJECTED);
      break;
    default:
      if (copy) {
        upload_result = OfficeFilesUploadResult::kCopyOperationError;
      } else {
        upload_result = OfficeFilesUploadResult::kMoveOperationError;
      }
      error_message = GetGenericErrorMessage();
  }
  OnFailedUpload(upload_result, error_message);
}

}  // namespace ash::cloud_upload
