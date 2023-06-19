// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_DRIVE_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_DRIVE_UPLOAD_HANDLER_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/scoped_suppress_drive_notifications_for_path.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chromeos/ash/components/drivefs/drivefs_host_observer.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

class Profile;

namespace ash::cloud_upload {

// Manages the "upload to Drive" workflow after user confirmation on the upload
// dialog. Instantiated by the static `Upload` method. Starts with moving the
// file to the cloud. Gets upload status by observing move and Drive events.
// Calls the UploadCallback with the uploaded file's hosted URL once the upload
// is completed, which is when `DriveUploadHandler` goes out of scope.
class DriveUploadHandler
    : public ::file_manager::io_task::IOTaskController::Observer,
      public drivefs::DriveFsHostObserver,
      public base::RefCounted<DriveUploadHandler> {
 public:
  using UploadCallback = base::OnceCallback<void(const GURL&, int64_t)>;

  // Starts the upload workflow for the file specified at construct time.
  static void Upload(Profile* profile,
                     const storage::FileSystemURL& source_url,
                     UploadCallback callback);

  DriveUploadHandler(const DriveUploadHandler&) = delete;
  DriveUploadHandler& operator=(const DriveUploadHandler&) = delete;

 private:
  friend base::RefCounted<DriveUploadHandler>;
  DriveUploadHandler(Profile* profile, const storage::FileSystemURL source_url);
  ~DriveUploadHandler() override;

  // Starts the upload workflow. Initiated by the `UploadToCloud` static method.
  void Run(UploadCallback callback);

  // Updates the progress notification for the upload workflow (move + syncing).
  void UpdateProgressNotification();

  // Ends upload and runs Upload callback.
  void OnEndUpload(GURL hosted_url,
                   OfficeFilesUploadResult result,
                   std::string error_message = "");

  // Callback for when ImmediatelyUpload() is called on DriveFS.
  void ImmediatelyUploadDone(drive::FileError error);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  // Find base::File::Error error returned by the IO Task and convert them to an
  // appropriate error notification.
  void ConvertFileErrorToUploadError(
      const file_manager::io_task::ProgressStatus& status);

  // DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // Checks the alternate URL from the request file's metadata.
  void OnGetDriveMetadata(bool timed_out,
                          drive::FileError error,
                          drivefs::mojom::FileMetadataPtr metadata);

  // Get the uploaded file's alternate URL. `timed_out` indicates whether or not
  // the timeout for getting the alternate URL is hit.
  void CheckAlternateUrl(bool timed_out);

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<::file_manager::io_task::IOTaskController, ExperimentalAsh>
      io_task_controller_;
  const raw_ptr<drive::DriveIntegrationService, ExperimentalAsh>
      drive_integration_service_;
  scoped_refptr<CloudUploadNotificationManager> notification_manager_;
  const storage::FileSystemURL source_url_;
  ::file_manager::io_task::IOTaskId observed_task_id_;
  base::FilePath observed_relative_drive_path_;
  int move_progress_ = 0;
  int sync_progress_ = 0;
  base::OneShotTimer alternate_url_timeout_;
  base::OneShotTimer alternate_url_poll_timer_;
  UploadCallback callback_;
  // Total size (in bytes) required to upload.
  int64_t upload_size_ = 0;
  std::unique_ptr<::file_manager::ScopedSuppressDriveNotificationsForPath>
      scoped_suppress_drive_notifications_for_path_ = nullptr;
  base::WeakPtrFactory<DriveUploadHandler> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_DRIVE_UPLOAD_HANDLER_H_
