// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_DRIVE_UPLOADER_H_
#define COMPONENTS_DRIVE_DRIVE_UPLOADER_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

class GURL;

namespace base {
class FilePath;
class TaskRunner;
}

namespace google_apis {
struct UploadRangeResponse;
}

namespace drive {
class DriveServiceInterface;

// Callback to be invoked once the upload has completed.
// |upload_location| will be returned when the uploading process is started but
// terminated before the completion due to some errors. It can be used to
// resume it.
typedef base::Callback<void(
    google_apis::DriveApiErrorCode error,
    const GURL& upload_location,
    std::unique_ptr<google_apis::FileResource> resource_entry)>
    UploadCompletionCallback;

class DriveUploaderInterface {
 public:
  virtual ~DriveUploaderInterface() = default;

  // Starts batch processing for upload requests. All requests which upload
  // small files (less than kMaxMultipartUploadSize) between
  // |StartBatchProcessing| and |StopBatchProcessing| are sent as a single batch
  // request.
  virtual void StartBatchProcessing() = 0;

  // Stops batch processing. Must be called after calling |StartBatchProcessing|
  // to commit requests.
  virtual void StopBatchProcessing() = 0;

  // Uploads a new file to a directory specified by |upload_location|.
  // Returns a callback for cancelling the uploading job.
  //
  // parent_resource_id:
  //   resource id of the destination directory.
  //
  // local_file_path:
  //   The path to the local file to be uploaded.
  //
  // title:
  //   The title (file name) of the file to be uploaded.
  //
  // content_type:
  //   The content type of the file to be uploaded.
  //
  // callback:
  //   Called when an upload is done regardless of it was successful or not.
  //   Must not be null.
  //
  // progress_callback:
  //   Periodically called back with the total number of bytes sent so far.
  //   May be null if the information is not needed.
  virtual google_apis::CancelCallback UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const UploadNewFileOptions& options,
      const UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) = 0;

  // Uploads an existing file (a file that already exists on Drive).
  //
  // See comments at UploadNewFile about common parameters and the return value.
  //
  // resource_id:
  //   resource id of the existing file to be overwritten.
  //
  // etag:
  //   Expected ETag for the destination file. If it does not match, the upload
  //   fails with UPLOAD_ERROR_CONFLICT.
  //   If |etag| is empty, the test is skipped.
  virtual google_apis::CancelCallback UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const UploadExistingFileOptions& options,
      const UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) = 0;

  // Resumes the uploading process terminated before the completion.
  // |upload_location| should be the one returned via UploadCompletionCallback
  // for previous invocation. |drive_file_path|, |local_file_path| and
  // |content_type| must be set to the same ones for previous invocation.
  //
  // See comments at UploadNewFile about common parameters and the return value.
  virtual google_apis::CancelCallback ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) = 0;
};

class DriveUploader : public DriveUploaderInterface {
 public:
  // In unittest, the |wake_lock_provider| is set as nullptr.
  DriveUploader(
      DriveServiceInterface* drive_service,
      const scoped_refptr<base::TaskRunner>& blocking_task_runner,
      mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider);

  ~DriveUploader() override;

  // DriveUploaderInterface overrides.
  void StartBatchProcessing() override;
  void StopBatchProcessing() override;
  google_apis::CancelCallback UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const UploadNewFileOptions& options,
      const UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const UploadExistingFileOptions& options,
      const UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;

 private:
  class RefCountedBatchRequest;
  struct UploadFileInfo;
  typedef base::Callback<void(std::unique_ptr<UploadFileInfo> upload_file_info)>
      StartInitiateUploadCallback;

  // Starts uploading a file with |upload_file_info|.
  google_apis::CancelCallback StartUploadFile(
      std::unique_ptr<UploadFileInfo> upload_file_info,
      const StartInitiateUploadCallback& start_initiate_upload_callback);
  void StartUploadFileAfterGetFileSize(
      std::unique_ptr<UploadFileInfo> upload_file_info,
      const StartInitiateUploadCallback& start_initiate_upload_callback,
      bool get_file_size_result);

  // Checks file size and call InitiateUploadNewFile or MultipartUploadNewFile
  // API.  Upon completion, OnUploadLocationReceived (for InitiateUploadNewFile)
  // or OnMultipartUploadComplete (for MultipartUploadNewFile) should be called.
  // If |batch_request| is non-null, it calls the API function on the batch
  // request.
  void CallUploadServiceAPINewFile(
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      const scoped_refptr<RefCountedBatchRequest>& batch_request,
      std::unique_ptr<UploadFileInfo> upload_file_info);

  // Checks file size and call InitiateUploadExistingFile or
  // MultipartUploadExistingFile API.  Upon completion, OnUploadLocationReceived
  // (for InitiateUploadExistingFile) or OnMultipartUploadComplete (for
  // MultipartUploadExistingFile) should be called.
  // If |batch_request| is non-null, it calls the API function on the batch
  // request.
  void CallUploadServiceAPIExistingFile(
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      const scoped_refptr<RefCountedBatchRequest>& batch_request,
      std::unique_ptr<UploadFileInfo> upload_file_info);

  // DriveService callback for InitiateUpload.
  void OnUploadLocationReceived(
      std::unique_ptr<UploadFileInfo> upload_file_info,
      google_apis::DriveApiErrorCode code,
      const GURL& upload_location);

  // Starts to get the current upload status for the file uploading.
  // Upon completion, OnUploadRangeResponseReceived should be called.
  void StartGetUploadStatus(std::unique_ptr<UploadFileInfo> upload_file_info);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(std::unique_ptr<UploadFileInfo> upload_file_info);

  // DriveService callback for ResumeUpload.
  void OnUploadRangeResponseReceived(
      std::unique_ptr<UploadFileInfo> upload_file_info,
      const google_apis::UploadRangeResponse& response,
      std::unique_ptr<google_apis::FileResource> entry);
  void OnUploadProgress(const google_apis::ProgressCallback& callback,
                        int64_t start_position,
                        int64_t total_size,
                        int64_t progress_of_chunk,
                        int64_t total_of_chunk);

  // Handles failed uploads.
  void UploadFailed(std::unique_ptr<UploadFileInfo> upload_file_info,
                    google_apis::DriveApiErrorCode error);

  // Handles completion/error of multipart uploading.
  void OnMultipartUploadComplete(
      std::unique_ptr<UploadFileInfo> upload_file_info,
      google_apis::DriveApiErrorCode error,
      std::unique_ptr<google_apis::FileResource> entry);

  device::mojom::WakeLockProvider* GetWakeLockProvider();

  // The class is expected to run on UI thread.
  base::ThreadChecker thread_checker_;

  // The lifetime of this object should be guaranteed to exceed that of the
  // DriveUploader instance.
  DriveServiceInterface* drive_service_;  // Not owned by this class.

  scoped_refptr<base::TaskRunner> blocking_task_runner_;
  scoped_refptr<RefCountedBatchRequest> current_batch_request_;

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveUploader> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DriveUploader);
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_DRIVE_UPLOADER_H_
