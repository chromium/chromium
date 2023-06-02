// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::Service;
using storage::FileSystemURL;

namespace ash::cloud_upload {
namespace {

constexpr char kUploadResultMetricName[] =
    "FileBrowser.OfficeFiles.Open.UploadResult.OneDrive";

// Runs the callback provided to `OneDriveUploadHandler::Upload`.
void OnUploadDone(scoped_refptr<OneDriveUploadHandler> one_drive_upload_handler,
                  OneDriveUploadHandler::UploadCallback callback,
                  const FileSystemURL& uploaded_file_url) {
  std::move(callback).Run(uploaded_file_url);
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
              "Microsoft OneDrive",
              "Microsoft 365",
              // TODO(b/242685536) Update when support for multi-files is added.
              /*num_files=*/1,
              GetOperationTypeForUpload(profile, source_url))),
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
    OnEndUpload(FileSystemURL(), OfficeFilesUploadResult::kOtherError,
                "No profile");
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    OnEndUpload(FileSystemURL(), OfficeFilesUploadResult::kOtherError,
                "No volume manager");
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    OnEndUpload(FileSystemURL(), OfficeFilesUploadResult::kOtherError,
                "No task_controller");
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  // Destination url.
  ProviderId provider_id = ProviderId::CreateFromExtensionId(
      file_manager::file_tasks::GetODFSExtensionId(profile_));
  Service* service = Service::Get(profile_);
  std::vector<ProvidedFileSystemInfo> file_systems =
      service->GetProvidedFileSystemInfoList(provider_id);
  // One and only one filesystem should be mounted for the ODFS extension.
  if (file_systems.size() != 1u) {
    std::string error_message =
        file_systems.empty()
            ? "No file systems found for the ODFS Extension"
            : "Multiple file systems found for the ODFS Extension";
    OnEndUpload(FileSystemURL(), OfficeFilesUploadResult::kFileSystemNotFound,
                error_message);
    return;
  }
  destination_folder_path_ = file_systems[0].mount_path();
  FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path_);
  // TODO (b/243095484) Define error behavior.
  if (!destination_folder_url.is_valid()) {
    OnEndUpload(FileSystemURL(), OfficeFilesUploadResult::kFileSystemNotFound,
                "Unable to generate destination folder URL");
    return;
  }

  const file_manager::io_task::OperationType operation_type =
      GetOperationTypeForUpload(profile_, source_url_);
  std::vector<FileSystemURL> source_urls{source_url_};
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          operation_type, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void OneDriveUploadHandler::ShowReauthenticationOrMoveUploadError(
    OfficeFilesUploadResult generic_upload_result,
    std::string generic_move_error_message) {
  ash::file_system_provider::util::LocalPathParser parser(
      profile_, destination_folder_path_);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    OnEndUpload(FileSystemURL(), generic_upload_result,
                generic_move_error_message);
    return;
  }
  // GetActions will fail with ACCESS_DENIED if the user is unauthenticated.
  parser.file_system()->GetActions(
      {parser.file_path()},
      base::BindOnce(&OneDriveUploadHandler::OnGetActionsResult,
                     weak_ptr_factory_.GetWeakPtr(), generic_upload_result,
                     generic_move_error_message));
}

void OneDriveUploadHandler::OnGetActionsResult(
    OfficeFilesUploadResult generic_upload_result,
    std::string generic_move_error_message,
    const file_system_provider::Actions& actions,
    base::File::Error result) {
  if (result == base::File::Error::FILE_ERROR_ACCESS_DENIED) {
    OnEndUpload(FileSystemURL(), OfficeFilesUploadResult::kCloudAuthError,
                kReauthenticationRequiredMessage);
    return;
  }
  OnEndUpload(FileSystemURL(), generic_upload_result,
              generic_move_error_message);
}

void OneDriveUploadHandler::OnEndUpload(const FileSystemURL& uploaded_file_url,
                                        OfficeFilesUploadResult result,
                                        std::string error_message) {
  UMA_HISTOGRAM_ENUMERATION(kUploadResultMetricName, result);
  // Resolve notifications.
  if (notification_manager_) {
    if (uploaded_file_url.is_valid()) {
      notification_manager_->MarkUploadComplete();
    } else if (!error_message.empty()) {
      LOG(ERROR) << "Upload to OneDrive: " << error_message;
      notification_manager_->ShowUploadError(error_message);
    }
  }
  if (callback_) {
    std::move(callback_).Run(uploaded_file_url);
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
        OnEndUpload(FileSystemURL(),
                    OfficeFilesUploadResult::kCopyOperationCancelled,
                    "Copy error: kCancelled");
      } else {
        OnEndUpload(FileSystemURL(),
                    OfficeFilesUploadResult::kMoveOperationCancelled,
                    "Move error: kCancelled");
      }
      return;
    case file_manager::io_task::State::kError:
      if (status.type == file_manager::io_task::OperationType::kCopy) {
        ShowReauthenticationOrMoveUploadError(
            OfficeFilesUploadResult::kCopyOperationError, "Copy error: kError");
      } else {
        ShowReauthenticationOrMoveUploadError(
            OfficeFilesUploadResult::kMoveOperationError, "Move error: kError");
      }
      return;
    case file_manager::io_task::State::kNeedPassword:
      if (status.type == file_manager::io_task::OperationType::kCopy) {
        OnEndUpload(FileSystemURL(),
                    OfficeFilesUploadResult::kCopyOperationNeedPassword,
                    "Copy error: kNeedPassword");
      } else {
        OnEndUpload(FileSystemURL(),
                    OfficeFilesUploadResult::kMoveOperationNeedPassword,
                    "Move error: kNeedPassword");
      }
      return;
  }
}

}  // namespace ash::cloud_upload
