// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_HANDLER_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/extensions/file_manager/scoped_suppress_drive_notifications_for_path.h"
#include "chromeos/ash/components/drivefs/drivefs_host_observer.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

class Profile;

namespace chromeos::cloud_upload {

// Manages the upload workflow after user confirmation. Instantiated by the
// static `UploadToCloud` method. Starts with moving the file to the cloud. Gets
// upload status by observing move and Drive events. Calls the UploadCallback
// with the uploaded file's hosted URL once the upload is completed, which is
// when the lifetime of the instance is expected to end.
class CloudUploadHandler
    : public file_manager::io_task::IOTaskController::Observer,
      public drivefs::DriveFsHostObserver,
      public base::RefCounted<CloudUploadHandler> {
 public:
  using UploadCallback = base::OnceCallback<void(GURL)>;

  // Generates the upload destination path.
  static base::FilePath GenerateUploadFolderPath(Profile* profile);

  // Starts the upload workflow for the file specified at construct time.
  static void UploadToCloud(Profile* profile,
                            const storage::FileSystemURL& source_url,
                            UploadCallback callback);

  CloudUploadHandler(const CloudUploadHandler&) = delete;
  CloudUploadHandler& operator=(const CloudUploadHandler&) = delete;

 private:
  friend base::RefCounted<CloudUploadHandler>;
  CloudUploadHandler(Profile* profile, const storage::FileSystemURL source_url);
  ~CloudUploadHandler() override;

  // Starts the upload workflow. Initiated by the `UploadToCloud` static method.
  void Run(UploadCallback callback);

  // Ends upload and runs Upload callback.
  void OnEndUpload(GURL hosted_url);

  void OnDestinationDirectoryCreated(
      storage::FileSystemURL destination_folder_url,
      base::File::Error error);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override;

  // DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;
  void OnFilesChanged(
      const std::vector<drivefs::mojom::FileChange>& changes) override;
  void OnError(const drivefs::mojom::DriveError& error) override;

  // Checks the alternate URL from the request file's metadata.
  void OnGetDriveMetadata(drive::FileError error,
                          drivefs::mojom::FileMetadataPtr metadata);

  Profile* const profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  file_manager::io_task::IOTaskController* io_task_controller_;
  drive::DriveIntegrationService* const drive_integration_service_;
  const storage::FileSystemURL source_url_;
  file_manager::io_task::IOTaskId observed_task_id_;
  base::FilePath observed_relative_drive_path_;
  bool error_found_;
  bool upload_done_;
  UploadCallback callback_;
  std::unique_ptr<file_manager::ScopedSuppressDriveNotificationsForPath>
      scoped_suppress_drive_notifications_for_path_ = nullptr;
  base::WeakPtrFactory<CloudUploadHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_HANDLER_H_
