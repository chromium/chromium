// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_ONE_DRIVE_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_ONE_DRIVE_UPLOAD_HANDLER_H_

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace ash::cloud_upload {

// Manages the "upload to OneDrive" workflow after user confirmation on the
// upload dialog. Instantiated by the static `Upload` method. Starts with moving
// the file to OneDrive. Calls the UploadCallback uploaded file's URL once the
// upload is completed, which is when the `OneDriveUploadHandler` goes out of
// scope.
class OneDriveUploadHandler
    : public ::file_manager::io_task::IOTaskController::Observer {
 public:
  using UploadCallback = base::OnceCallback<
      void(OfficeTaskResult, std::optional<storage::FileSystemURL>, int64_t)>;

  OneDriveUploadHandler(Profile* profile,
                        const storage::FileSystemURL& source_url,
                        UploadCallback callback,
                        base::SafeRef<CloudOpenMetrics> cloud_open_metrics);
  ~OneDriveUploadHandler() override;

  // Starts the upload workflow.
  void Run();

  OneDriveUploadHandler(const OneDriveUploadHandler&) = delete;
  OneDriveUploadHandler& operator=(const OneDriveUploadHandler&) = delete;

 private:
  void GetODFSMetadataAndStartIOTask();

  // If reauth is required, request a new mount without a notification. If that
  // fails, show an error and finish, or else start the IOTask for copy/move.
  void CheckReauthenticationAndStartIOTask(
      const FileSystemURL& destination_folder_url,
      base::expected<ODFSMetadata, base::File::Error> metadata_or_error);

  // Called when we have attempted to remount ODFS due to needing to reauth.
  void OnMountResponse(base::File::Error result);

  FileSystemURL GetDestinationFolderUrl(
      file_system_provider::ProvidedFileSystemInfo odfs_info);

  // Ends upload in a successful state, shows a complete notification and runs
  // the upload callback.
  void OnSuccessfulUpload(OfficeFilesUploadResult result_metric,
                          storage::FileSystemURL url);

  // Ends upload in a failed state, shows an error notification and runs the
  // upload callback.
  void OnFailedUpload(OfficeFilesUploadResult result_metric,
                      std::string error_message = GetGenericErrorMessage());

  // Ends upload in an abandoned state and runs the upload callback.
  void OnAbandonedUpload();

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  // Find the base::File::Error error returned by the IO Task and convert it to
  // an appropriate error notification.
  void ShowIOTaskError(const ::file_manager::io_task::ProgressStatus& status);

  // Show the correct error notification for
  // base::File::FILE_ERROR_ACCESS_DENIED. Request ODFS metadata and show the
  // correct notification in the |OnGetReauthenticationRequired| callback.
  void ShowAccessDeniedError();

  // Check if reauthentication to OneDrive is required from the ODFS metadata
  // and show the reuathentication is required notification if true. Otherwise
  // show the generic access error notification.
  void OnGetReauthenticationRequired(
      base::expected<ODFSMetadata, base::File::Error> metadata_or_error);

  // OnGetActions callback which checks the |result| to see if reauthentication
  // is required. If reauthentication is required, show the reauthentication
  // required error. Otherwise show a generic move upload error.
  void OnGetActionsResult(OfficeFilesUploadResult generic_upload_result,
                          std::string generic_move_error_message,
                          const file_system_provider::Actions& actions,
                          base::File::Error result);

  const raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  base::FilePath destination_folder_path_;
  raw_ptr<::file_manager::io_task::IOTaskController> io_task_controller_;
  scoped_refptr<CloudUploadNotificationManager> notification_manager_;
  const storage::FileSystemURL source_url_;
  ::file_manager::io_task::IOTaskId observed_task_id_;
  ::file_manager::io_task::OperationType operation_type_;
  UploadCallback callback_;
  // Total size (in bytes) required to upload.
  int64_t upload_size_ = 0;
  base::SafeRef<CloudOpenMetrics> cloud_open_metrics_;
  bool tried_reauth_ = false;
  base::WeakPtrFactory<OneDriveUploadHandler> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_ONE_DRIVE_UPLOAD_HANDLER_H_
