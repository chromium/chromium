// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_DRIVE_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_DRIVE_UPLOAD_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/scoped_suppress_drive_notifications_for_path.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

class Profile;

namespace ash::cloud_upload {
FORWARD_DECLARE_TEST(DriveUploadHandlerTest, OnGetDriveMetadata_WhenNoMetadata);
FORWARD_DECLARE_TEST(DriveUploadHandlerTest,
                     OnGetDriveMetadata_WhenInvalidAlternateUrl);
FORWARD_DECLARE_TEST(DriveUploadHandlerTest,
                     OnGetDriveMetadata_WhenHostIsUnexpected);
FORWARD_DECLARE_TEST(DriveUploadHandlerTest,
                     OnGetDriveMetadata_WhenFileNotAnOfficeFile);

// Manages the "upload to Drive" workflow after user confirmation on the upload
// dialog. Instantiated by the static `Upload` method. Starts with moving the
// file to the cloud. Gets upload status by observing move and Drive events.
// Calls the UploadCallback with the uploaded file's hosted URL once the upload
// is completed, which is when `DriveUploadHandler` goes out of scope.
class DriveUploadHandler
    : public ::file_manager::io_task::IOTaskController::Observer,
      drivefs::DriveFsHost::Observer,
      drive::DriveIntegrationService::Observer {
 public:
  using UploadCallback =
      base::OnceCallback<void(OfficeTaskResult, std::optional<GURL>, int64_t)>;

  DriveUploadHandler(Profile* profile,
                     const storage::FileSystemURL& source_url,
                     UploadCallback callback,
                     base::SafeRef<CloudOpenMetrics> cloud_open_metrics);
  ~DriveUploadHandler() override;

  // Starts the upload workflow:
  // - Copy the file via an IO task.
  // - Sync to Drive.
  // - Remove the source file in case of a move operation. Move mode of the
  //   `CopyOrMoveIOTask` is not used because the source file should only be
  //   deleted at the end of the sync operation.
  // Initiated by the `Upload` static method.
  void Run();

  DriveUploadHandler(const DriveUploadHandler&) = delete;
  DriveUploadHandler& operator=(const DriveUploadHandler&) = delete;

  FRIEND_TEST_ALL_PREFIXES(DriveUploadHandlerTest,
                           OnGetDriveMetadata_WhenNoMetadata);
  FRIEND_TEST_ALL_PREFIXES(DriveUploadHandlerTest,
                           OnGetDriveMetadata_WhenInvalidAlternateUrl);
  FRIEND_TEST_ALL_PREFIXES(DriveUploadHandlerTest,
                           OnGetDriveMetadata_WhenHostIsUnexpected);
  FRIEND_TEST_ALL_PREFIXES(DriveUploadHandlerTest,
                           OnGetDriveMetadata_WhenFileNotAnOfficeFile);

 private:
  // Updates the progress notification for the upload workflow (copy + syncing).
  void UpdateProgressNotification();

  // Called upon a copy to Drive success or failure. If required, complete or
  // undo the operation. Then call |OnSuccessfulUpload| or |OnFailedUpload| to
  // end the successful or failed upload respectively.
  void OnEndCopy(OfficeFilesUploadResult result_metric,
                 base::expected<GURL, std::string> hosted_url =
                     base::unexpected(GetGenericErrorMessage()));

  // Ends upload in a successful state, shows a complete notification and runs
  // the upload callback.
  void OnSuccessfulUpload(OfficeFilesUploadResult result_metric,
                          GURL hosted_url);

  // Ends upload in a failed state, shows an error notification and runs the
  // upload callback.
  void OnFailedUpload(OfficeFilesUploadResult result_metric,
                      std::string error_message);

  // Callback for when ImmediatelyUpload() is called on DriveFS.
  void ImmediatelyUploadDone(drive::FileError error);

  // Directs IO task status updates to |OnCopyStatus| or |OnDeleteStatus| based
  // on task id.
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  // Observes copy to Drive IO task status updates. Calls |OnEndCopy| upon any
  // error.
  void OnCopyStatus(const ::file_manager::io_task::ProgressStatus& status);

  // Observes delete IO task status updates from the delete task for cleaning up
  // the source file. Calls `OnEndUpload` once the delete is finished.
  void OnDeleteStatus(const ::file_manager::io_task::ProgressStatus& status);

  // Find the base::File::Error error returned by the IO Task and convert it to
  // an appropriate error notification.
  void ShowIOTaskError(const ::file_manager::io_task::ProgressStatus& status);

  // DriveFsHost::Observer implementation.
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // DriveIntegrationService::Observer implementation.
  void OnDriveConnectionStatusChanged(
      drive::util::ConnectionStatus status) override;

  // Checks the alternate URL from the request file's metadata.
  void OnGetDriveMetadata(bool timed_out,
                          drive::FileError error,
                          drivefs::mojom::FileMetadataPtr metadata);

  // Get the uploaded file's alternate URL. `timed_out` indicates whether or not
  // the timeout for getting the alternate URL is hit.
  void CheckAlternateUrl(bool timed_out);

  const raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<::file_manager::io_task::IOTaskController> io_task_controller_ =
      nullptr;
  const raw_ptr<drive::DriveIntegrationService> drive_integration_service_;
  const UploadType upload_type_;
  scoped_refptr<CloudUploadNotificationManager> notification_manager_;
  const storage::FileSystemURL source_url_;
  ::file_manager::io_task::IOTaskId observed_copy_task_id_;
  ::file_manager::io_task::IOTaskId observed_delete_task_id_;
  base::FilePath observed_absolute_dest_path_;
  base::FilePath observed_relative_drive_path_;
  bool copy_ended_ = false;
  int move_progress_ = 0;
  int sync_progress_ = 0;
  base::OneShotTimer alternate_url_timeout_;
  base::OneShotTimer alternate_url_poll_timer_;
  base::OnceClosure end_upload_callback_;
  UploadCallback callback_;
  // Total size (in bytes) required to upload.
  int64_t upload_size_ = 0;
  std::unique_ptr<::file_manager::ScopedSuppressDriveNotificationsForPath>
      scoped_suppress_drive_notifications_for_path_ = nullptr;
  base::ScopedObservation<::file_manager::io_task::IOTaskController,
                          ::file_manager::io_task::IOTaskController::Observer>
      io_task_controller_observer_{this};
  base::SafeRef<CloudOpenMetrics> cloud_open_metrics_;
  base::WeakPtrFactory<DriveUploadHandler> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_DRIVE_UPLOAD_HANDLER_H_
