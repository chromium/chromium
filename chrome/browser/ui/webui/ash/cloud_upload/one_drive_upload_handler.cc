// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"

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
namespace {

// Runs the callback provided to `OneDriveUploadHandler::Upload`.
void OnUploadDone(scoped_refptr<OneDriveUploadHandler> one_drive_upload_handler,
                  OneDriveUploadHandler::UploadCallback callback,
                  absl::optional<FileSystemURL> uploaded_file_url,
                  int64_t upload_size) {
  std::move(callback).Run(std::move(uploaded_file_url), upload_size);
}

}  // namespace

// static.
void OneDriveUploadHandler::Upload(Profile* profile,
                                   const FileSystemURL& source_url,
                                   UploadCallback callback) {
  scoped_refptr<OneDriveUploadHandler> one_drive_upload_handler =
      new OneDriveUploadHandler(profile, source_url);
  // Keep `one_drive_upload_handler` alive until `UploadToCloudDone` executes.
  one_drive_upload_handler->Run(base::BindOnce(
      &OnUploadDone, one_drive_upload_handler, std::move(callback)));
}

OneDriveUploadHandler::OneDriveUploadHandler(Profile* profile,
                                             const FileSystemURL source_url)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      notification_manager_(
          base::MakeRefCounted<CloudUploadNotificationManager>(
              profile,
              source_url.path().BaseName().value(),
              l10n_util::GetStringUTF8(IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE),
              l10n_util::GetStringUTF8(IDS_OFFICE_FILE_HANDLER_APP_MICROSOFT),
              // TODO(b/242685536) Update when support for multi-files is added.
              /*num_files=*/1,
              GetUploadType(profile, source_url))),
      source_url_(source_url) {
  observed_task_id_ = -1;
}

OneDriveUploadHandler::~OneDriveUploadHandler() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

void OneDriveUploadHandler::Run(UploadCallback callback) {
  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = std::move(callback);

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kOtherError);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kOtherError);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kOtherError);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  // Destination url.
  auto odfs_info = GetODFSInfo(profile_);
  if (!odfs_info) {
    // TODO(b/293363474): Remove when the underlying cause is diagnosed.
    base::debug::DumpWithoutCrashing(FROM_HERE);
    OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kFileSystemNotFound);
    return;
  }
  destination_folder_path_ = odfs_info->mount_path();
  FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path_);
  // TODO (b/243095484) Define error behavior.
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder ODFS URL";
    // TODO(b/293363474): Remove when the underlying cause is diagnosed.
    base::debug::DumpWithoutCrashing(FROM_HERE);
    OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kDestinationUrlError);
    return;
  }

  const file_manager::io_task::OperationType operation_type =
      GetUploadType(profile_, source_url_) == UploadType::kCopy
          ? file_manager::io_task::OperationType::kCopy
          : file_manager::io_task::OperationType::kMove;
  std::vector<FileSystemURL> source_urls{source_url_};
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          operation_type, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void OneDriveUploadHandler::OnEndUpload(
    base::expected<storage::FileSystemURL, std::string> url,
    OfficeFilesUploadResult result_metric) {
  UMA_HISTOGRAM_ENUMERATION(kOneDriveUploadResultMetricName, result_metric);
  if (url.has_value()) {
    // Resolve notifications.
    if (notification_manager_) {
      notification_manager_->MarkUploadComplete();
    }
    if (callback_) {
      std::move(callback_).Run(url.value(), upload_size_);
    }
  } else {
    if (const std::string& error_message = url.error();
        notification_manager_ && !error_message.empty()) {
      LOG(ERROR) << "Upload to OneDrive: " << error_message;
      notification_manager_->ShowUploadError(error_message);
    }
    if (callback_) {
      std::move(callback_).Run(absl::nullopt, 0);
    }
  }
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
      OnEndUpload(status.outputs[0].url, OfficeFilesUploadResult::kSuccess);
      return;
    case file_manager::io_task::State::kCancelled:
      if (status.type == file_manager::io_task::OperationType::kCopy) {
        OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                    OfficeFilesUploadResult::kCopyOperationCancelled);
      } else {
        OnEndUpload(base::unexpected(GetGenericErrorMessage()),
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

void OneDriveUploadHandler::OnGetReauthenticationRequired(
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  std::string error_message = GetGenericErrorMessage();
  OfficeFilesUploadResult upload_result =
      OfficeFilesUploadResult::kCloudAccessDenied;
  if (!metadata_or_error.has_value()) {
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata_or_error.error();
  } else if (metadata_or_error->reauthentication_required) {
    // Show the reauthentication required error notification.
    error_message = GetReauthenticationRequiredMessage();
    upload_result = OfficeFilesUploadResult::kCloudReauthRequired;
  }
  OnEndUpload(base::unexpected(error_message), upload_result);
}

void OneDriveUploadHandler::ShowAccessDeniedError() {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    OnEndUpload(base::unexpected(GetGenericErrorMessage()),
                OfficeFilesUploadResult::kCloudAccessDenied);
    return;
  }
  GetODFSMetadata(
      file_system,
      base::BindOnce(&OneDriveUploadHandler::OnGetReauthenticationRequired,
                     this));
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

  base::UmaHistogramExactLinear(
      copy ? kOneDriveCopyErrorMetricName : kOneDriveMoveErrorMetricName,
      -file_error, -base::File::FILE_ERROR_MAX);

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
    default:
      if (copy) {
        upload_result = OfficeFilesUploadResult::kCopyOperationError;
      } else {
        upload_result = OfficeFilesUploadResult::kMoveOperationError;
      }
      error_message = GetGenericErrorMessage();
  }
  OnEndUpload(base::unexpected(error_message), upload_result);
}

}  // namespace ash::cloud_upload
