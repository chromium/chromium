// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_ONE_DRIVE_UPLOAD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_ONE_DRIVE_UPLOAD_HANDLER_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_notification_manager.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

class Profile;

namespace ash::cloud_upload {

// Manages the "upload to OneDrive" workflow after user confirmation on the
// upload dialog. Instantiated by the static `Upload` method. Starts with moving
// the file to OneDrive. Calls the UploadCallback uploaded file's URL once the
// upload is completed, which is when the `OneDriveUploadHandler` goes out of
// scope.
class OneDriveUploadHandler
    : public ::file_manager::io_task::IOTaskController::Observer,
      public base::RefCounted<OneDriveUploadHandler> {
 public:
  using UploadCallback =
      base::OnceCallback<void(const storage::FileSystemURL&)>;

  // Starts the upload workflow for the file specified at construct time.
  static void Upload(Profile* profile,
                     const storage::FileSystemURL& source_url,
                     UploadCallback callback);

  OneDriveUploadHandler(const OneDriveUploadHandler&) = delete;
  OneDriveUploadHandler& operator=(const OneDriveUploadHandler&) = delete;

 private:
  friend base::RefCounted<OneDriveUploadHandler>;
  OneDriveUploadHandler(Profile* profile,
                        const storage::FileSystemURL source_url);
  ~OneDriveUploadHandler() override;

  // Starts the upload workflow. Initiated by the `UploadToCloud` static method.
  void Run(UploadCallback callback);

  // Ends upload and runs Upload callback.
  void OnEndUpload(const storage::FileSystemURL& uploaded_file_url,
                   std::string error_message = "");

  void OnDestinationDirectoryCreated(
      storage::FileSystemURL destination_folder_url,
      base::File::Error error);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  Profile* const profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  ::file_manager::io_task::IOTaskController* io_task_controller_;
  scoped_refptr<CloudUploadNotificationManager> notification_manager_;
  const storage::FileSystemURL source_url_;
  ::file_manager::io_task::IOTaskId observed_task_id_;
  UploadCallback callback_;
  base::WeakPtrFactory<OneDriveUploadHandler> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_ONE_DRIVE_UPLOAD_HANDLER_H_
