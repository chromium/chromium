// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_uploader.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/drive_api_parser.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

using google_apis::ApiErrorCode;
using google_apis::CancelCallbackOnce;
using google_apis::CANCELLED;
using google_apis::DRIVE_NO_SPACE;
using google_apis::FileResource;
using google_apis::HTTP_CONFLICT;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_FORBIDDEN;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::ProgressCallback;
using google_apis::UploadRangeResponse;

namespace drive {

namespace {

// Upload data is split to multiple HTTP request each conveying kUploadChunkSize
// bytes (except the request for uploading the last chunk of data).
// The value must be a multiple of 512KB according to the spec of GData WAPI and
// Drive API v2. It is set to a smaller value than 2^31 for working around
// server side error (crbug.com/264089).
const int64_t kUploadChunkSize = (1LL << 30);  // 1GB
// Maximum file size to be uploaded by multipart requests. The file that is
// larger than the size is processed by resumable upload.
const int64_t kMaxMultipartUploadSize = (1LL << 20);  // 1MB

// Drive upload protocol. This is used to back a histogram. Sync this with UMA
// enum "DriveUploadProtocol" and treat this as append-only.
enum DriveUploadProtocol {
  UPLOAD_METHOD_RESUMABLE,
  UPLOAD_METHOD_MULTIPART,
  UPLOAD_METHOD_BATCH,
  UPLOAD_METHOD_MAX_VALUE
};

void RecordDriveUploadProtocol(DriveUploadProtocol protocol) {
  UMA_HISTOGRAM_ENUMERATION("Drive.UploadProtocol", protocol,
                            UPLOAD_METHOD_MAX_VALUE);
}

// Wrapper around base::GetFileSize(), as it is currently overloaded, so
// base::BindOnce() cannot figure out which version to use.
// TODO(crbug.com/371234479): Remove this after removing the deprecated version
// of base::GetFileSize().
std::optional<int64_t> GetFileSizeWrapper(const base::FilePath& file_path) {
  return base::GetFileSize(file_path);
}

}  // namespace

// Refcounted helper class to manage batch request. DriveUploader uses the class
// for keeping the BatchRequestConfigurator instance while it prepares upload
// file information asynchronously. DriveUploader discard the reference after
// getting file information and the instance will be destroyed after all
// preparations complete. At that time, the helper instance commits owned batch
// request at the destrutor.
class DriveUploader::RefCountedBatchRequest
    : public base::RefCounted<RefCountedBatchRequest> {
 public:
  RefCountedBatchRequest(
      std::unique_ptr<BatchRequestConfiguratorInterface> configurator)
      : configurator_(std::move(configurator)) {}

  // Gets pointer of BatchRequestConfiguratorInterface owned by the instance.
  BatchRequestConfiguratorInterface* configurator() const {
    return configurator_.get();
  }

 private:
  friend class base::RefCounted<RefCountedBatchRequest>;
  ~RefCountedBatchRequest() { configurator_->Commit(); }
  std::unique_ptr<BatchRequestConfiguratorInterface> configurator_;
};

// Structure containing current upload information of file, passed between
// DriveServiceInterface methods and callbacks.
struct DriveUploader::UploadFileInfo {
  UploadFileInfo(const base::FilePath& local_path,
                 const std::string& content_type,
                 UploadCompletionCallback callback,
                 ProgressCallback progress_callback,
                 device::mojom::WakeLockProvider* wake_lock_provider)
      : file_path(local_path),
        content_type(content_type),
        completion_callback(std::move(callback)),
        progress_callback(progress_callback),
        content_length(0),
        next_start_position(-1),
        cancelled(false) {
    if (wake_lock_provider) {
      wake_lock_provider->GetWakeLockWithoutContext(
          device::mojom::WakeLockType::kPreventAppSuspension,
          device::mojom::WakeLockReason::kOther, "Upload in progress",
          wake_lock.BindNewPipeAndPassReceiver());
      wake_lock->RequestWakeLock();
    }
  }

  UploadFileInfo(const UploadFileInfo&) = delete;
  UploadFileInfo& operator=(const UploadFileInfo&) = delete;

  ~UploadFileInfo() = default;

  // Useful for printf debugging.
  std::string DebugString() const {
    return "file_path=[" + file_path.AsUTF8Unsafe() + "], content_type=[" +
           content_type + "], content_length=[" +
           base::NumberToString(content_length) + "]";
  }

  // Returns the callback to cancel the upload represented by this struct.
  CancelCallbackOnce GetCancelCallback() {
    return base::BindOnce(&UploadFileInfo::Cancel,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // The local file path of the file to be uploaded.
  const base::FilePath file_path;

  // Content-Type of file.
  const std::string content_type;

  // Callback to be invoked once the upload has finished.
  UploadCompletionCallback completion_callback;

  // Callback to periodically notify the upload progress.
  const ProgressCallback progress_callback;

  // Location URL where file is to be uploaded to, returned from
  // InitiateUpload. Used for the subsequent ResumeUpload requests.
  GURL upload_location;

  // Header content-Length.
  int64_t content_length;

  int64_t next_start_position;

  // Blocks system suspend while upload is in progress.
  mojo::Remote<device::mojom::WakeLock> wake_lock;

  // Fields for implementing cancellation. |cancel_callback| is non-null if
  // there is an in-flight HTTP request. In that case, |cancell_callback| will
  // cancel the operation. |cancelled| is initially false and turns to true
  // once Cancel() is called. DriveUploader will check this field before after
  // an async task other than HTTP requests and cancels the subsequent requests
  // if this is flagged to true.
  CancelCallbackOnce cancel_callback;
  bool cancelled;

 private:
  // Cancels the upload represented by this struct.
  void Cancel() {
    cancelled = true;
    if (!cancel_callback.is_null())
      std::move(cancel_callback).Run();
  }

  base::WeakPtrFactory<UploadFileInfo> weak_ptr_factory_{this};
};

DriveUploader::DriveUploader(
    DriveServiceInterface* drive_service,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner,
    mojo::PendingRemote<device::mojom::WakeLockProvider> wake_lock_provider)
    : drive_service_(drive_service),
      blocking_task_runner_(blocking_task_runner),
      wake_lock_provider_(std::move(wake_lock_provider)) {}

DriveUploader::~DriveUploader() = default;

CancelCallbackOnce DriveUploader::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const UploadNewFileOptions& options,
    UploadCompletionCallback callback,
    ProgressCallback progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!parent_resource_id.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!title.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  return StartUploadFile(
      std::make_unique<UploadFileInfo>(local_file_path, content_type,
                                       std::move(callback), progress_callback,
                                       GetWakeLockProvider()),
      base::BindOnce(&DriveUploader::CallUploadServiceAPINewFile,
                     weak_ptr_factory_.GetWeakPtr(), parent_resource_id, title,
                     options, current_batch_request_));
}

void DriveUploader::StartBatchProcessing() {
  DCHECK(!current_batch_request_);
  current_batch_request_ =
      new RefCountedBatchRequest(drive_service_->StartBatchRequest());
}

void DriveUploader::StopBatchProcessing() {
  current_batch_request_ = nullptr;
}

CancelCallbackOnce DriveUploader::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const UploadExistingFileOptions& options,
    UploadCompletionCallback callback,
    ProgressCallback progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!resource_id.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  return StartUploadFile(
      std::make_unique<UploadFileInfo>(local_file_path, content_type,
                                       std::move(callback), progress_callback,
                                       GetWakeLockProvider()),
      base::BindOnce(&DriveUploader::CallUploadServiceAPIExistingFile,
                     weak_ptr_factory_.GetWeakPtr(), resource_id, options,
                     current_batch_request_));
}

CancelCallbackOnce DriveUploader::ResumeUploadFile(
    const GURL& upload_location,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    UploadCompletionCallback callback,
    ProgressCallback progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  auto upload_file_info = std::make_unique<UploadFileInfo>(
      local_file_path, content_type, std::move(callback), progress_callback,
      GetWakeLockProvider());
  upload_file_info->upload_location = upload_location;

  return StartUploadFile(std::move(upload_file_info),
                         base::BindOnce(&DriveUploader::StartGetUploadStatus,
                                        weak_ptr_factory_.GetWeakPtr()));
}

CancelCallbackOnce DriveUploader::StartUploadFile(
    std::unique_ptr<UploadFileInfo> upload_file_info,
    StartInitiateUploadCallback start_initiate_upload_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Uploading file: " << upload_file_info->DebugString();

  UploadFileInfo* info_ptr = upload_file_info.get();
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetFileSizeWrapper, info_ptr->file_path),
      base::BindOnce(&DriveUploader::StartUploadFileAfterGetFileSize,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(upload_file_info),
                     std::move(start_initiate_upload_callback)));
  return info_ptr->GetCancelCallback();
}

void DriveUploader::StartUploadFileAfterGetFileSize(
    std::unique_ptr<UploadFileInfo> upload_file_info,
    StartInitiateUploadCallback start_initiate_upload_callback,
    std::optional<int64_t> maybe_file_size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!maybe_file_size.has_value()) {
    UploadFailed(std::move(upload_file_info), HTTP_NOT_FOUND);
    return;
  }

  CHECK_GE(maybe_file_size.value(), 0);
  upload_file_info->content_length = maybe_file_size.value();

  if (upload_file_info->cancelled) {
    UploadFailed(std::move(upload_file_info), CANCELLED);
    return;
  }
  std::move(start_initiate_upload_callback).Run(std::move(upload_file_info));
}

void DriveUploader::CallUploadServiceAPINewFile(
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadNewFileOptions& options,
    const scoped_refptr<RefCountedBatchRequest>& batch_request,
    std::unique_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UploadFileInfo* const info_ptr = upload_file_info.get();
  if (info_ptr->content_length <= kMaxMultipartUploadSize) {
    DriveServiceBatchOperationsInterface* service;
    // If this is a batched request, calls the API on the request instead.
    if (batch_request) {
      service = batch_request->configurator();
      RecordDriveUploadProtocol(UPLOAD_METHOD_BATCH);
    } else {
      service = drive_service_;
      RecordDriveUploadProtocol(UPLOAD_METHOD_MULTIPART);
    }
    info_ptr->cancel_callback = service->MultipartUploadNewFile(
        info_ptr->content_type, /*converted_mime_type=*/std::nullopt,
        info_ptr->content_length, parent_resource_id, title,
        info_ptr->file_path, options,
        base::BindOnce(&DriveUploader::OnMultipartUploadComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(upload_file_info)),
        info_ptr->progress_callback);
  } else {
    RecordDriveUploadProtocol(UPLOAD_METHOD_RESUMABLE);
    info_ptr->cancel_callback = drive_service_->InitiateUploadNewFile(
        info_ptr->content_type, info_ptr->content_length, parent_resource_id,
        title, options,
        base::BindOnce(&DriveUploader::OnUploadLocationReceived,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(upload_file_info)));
  }
}

void DriveUploader::CallUploadServiceAPIExistingFile(
    const std::string& resource_id,
    const UploadExistingFileOptions& options,
    const scoped_refptr<RefCountedBatchRequest>& batch_request,
    std::unique_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UploadFileInfo* const info_ptr = upload_file_info.get();
  if (info_ptr->content_length <= kMaxMultipartUploadSize) {
    DriveServiceBatchOperationsInterface* service;
    // If this is a batched request, calls the API on the request instead.
    if (batch_request) {
      service = batch_request->configurator();
      RecordDriveUploadProtocol(UPLOAD_METHOD_BATCH);
    } else {
      service = drive_service_;
      RecordDriveUploadProtocol(UPLOAD_METHOD_MULTIPART);
    }
    info_ptr->cancel_callback = service->MultipartUploadExistingFile(
        info_ptr->content_type, info_ptr->content_length, resource_id,
        info_ptr->file_path, options,
        base::BindOnce(&DriveUploader::OnMultipartUploadComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(upload_file_info)),
        info_ptr->progress_callback);
  } else {
    RecordDriveUploadProtocol(UPLOAD_METHOD_RESUMABLE);
    info_ptr->cancel_callback = drive_service_->InitiateUploadExistingFile(
        info_ptr->content_type, info_ptr->content_length, resource_id, options,
        base::BindOnce(&DriveUploader::OnUploadLocationReceived,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(upload_file_info)));
  }
}

void DriveUploader::OnUploadLocationReceived(
    std::unique_ptr<UploadFileInfo> upload_file_info,
    ApiErrorCode code,
    const GURL& upload_location) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Got upload location [" << upload_location.spec() << "] for ["
           << upload_file_info->file_path.value() << "]";

  if (code != HTTP_SUCCESS) {
    if (code == HTTP_PRECONDITION)
      code = HTTP_CONFLICT;  // ETag mismatch.
    UploadFailed(std::move(upload_file_info), code);
    return;
  }

  upload_file_info->upload_location = upload_location;
  upload_file_info->next_start_position = 0;
  UploadNextChunk(std::move(upload_file_info));
}

void DriveUploader::StartGetUploadStatus(
    std::unique_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(upload_file_info);

  UploadFileInfo* info_ptr = upload_file_info.get();
  info_ptr->cancel_callback = drive_service_->GetUploadStatus(
      info_ptr->upload_location, info_ptr->content_length,
      base::BindOnce(&DriveUploader::OnUploadRangeResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(upload_file_info)));
}

void DriveUploader::UploadNextChunk(
    std::unique_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(upload_file_info);
  DCHECK_GE(upload_file_info->next_start_position, 0);
  DCHECK_LE(upload_file_info->next_start_position,
            upload_file_info->content_length);

  if (upload_file_info->cancelled) {
    UploadFailed(std::move(upload_file_info), CANCELLED);
    return;
  }

  // Limit the size of data uploaded per each request by kUploadChunkSize.
  const int64_t end_position =
      std::min(upload_file_info->content_length,
               upload_file_info->next_start_position + kUploadChunkSize);

  UploadFileInfo* info_ptr = upload_file_info.get();
  info_ptr->cancel_callback = drive_service_->ResumeUpload(
      info_ptr->upload_location, info_ptr->next_start_position, end_position,
      info_ptr->content_length, info_ptr->content_type, info_ptr->file_path,
      base::BindOnce(&DriveUploader::OnUploadRangeResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(upload_file_info)),
      base::BindRepeating(
          &DriveUploader::OnUploadProgress, weak_ptr_factory_.GetWeakPtr(),
          info_ptr->progress_callback, info_ptr->next_start_position,
          info_ptr->content_length));
}

void DriveUploader::OnUploadRangeResponseReceived(
    std::unique_ptr<UploadFileInfo> upload_file_info,
    const UploadRangeResponse& response,
    std::unique_ptr<FileResource> entry) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (response.code == HTTP_CREATED || response.code == HTTP_SUCCESS) {
    // When uploading a new file, we expect HTTP_CREATED, and when uploading
    // an existing file (to overwrite), we expect HTTP_SUCCESS.
    // There is an exception: if we uploading an empty file, uploading a new
    // file also returns HTTP_SUCCESS on Drive API v2. The correct way of the
    // fix should be uploading the metadata only. However, to keep the
    // compatibility with GData WAPI during the migration period, we just
    // relax the condition here.
    // TODO(hidehiko): Upload metadata only for empty files, after GData WAPI
    // code is gone.
    DVLOG(1) << "Successfully created uploaded file=["
             << upload_file_info->file_path.value() << "]";

    // Done uploading.
    std::move(upload_file_info->completion_callback)
        .Run(HTTP_SUCCESS, GURL(), std::move(entry));
    return;
  }

  // ETag mismatch.
  if (response.code == HTTP_PRECONDITION) {
    UploadFailed(std::move(upload_file_info), HTTP_CONFLICT);
    return;
  }

  // If code is 308 (RESUME_INCOMPLETE) and |range_received| starts with 0
  // (meaning that the data is uploaded from the beginning of the file),
  // proceed to upload the next chunk.
  if (response.code != HTTP_RESUME_INCOMPLETE ||
      response.start_position_received != 0) {
    DVLOG(1) << "UploadNextChunk http code=" << response.code
             << ", start_position_received=" << response.start_position_received
             << ", end_position_received=" << response.end_position_received;
    UploadFailed(std::move(upload_file_info), response.code == HTTP_FORBIDDEN
                                                  ? DRIVE_NO_SPACE
                                                  : response.code);
    return;
  }

  DVLOG(1) << "Received range " << response.start_position_received << "-"
           << response.end_position_received << " for ["
           << upload_file_info->file_path.value() << "]";

  upload_file_info->next_start_position = response.end_position_received;
  UploadNextChunk(std::move(upload_file_info));
}

void DriveUploader::OnUploadProgress(ProgressCallback callback,
                                     int64_t start_position,
                                     int64_t total_size,
                                     int64_t progress_of_chunk,
                                     int64_t total_of_chunk) {
  if (!callback.is_null())
    callback.Run(start_position + progress_of_chunk, total_size);
}

void DriveUploader::UploadFailed(
    std::unique_ptr<UploadFileInfo> upload_file_info,
    ApiErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Upload failed " << upload_file_info->DebugString();

  if (upload_file_info->next_start_position < 0) {
    // Discard the upload location because no request could succeed with it.
    // Maybe it's obsolete.
    upload_file_info->upload_location = GURL();
  }

  std::move(upload_file_info->completion_callback)
      .Run(error, upload_file_info->upload_location,
           std::unique_ptr<FileResource>());
}

void DriveUploader::OnMultipartUploadComplete(
    std::unique_ptr<UploadFileInfo> upload_file_info,
    google_apis::ApiErrorCode error,
    std::unique_ptr<FileResource> entry) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error == HTTP_CREATED || error == HTTP_SUCCESS) {
    DVLOG(1) << "Successfully created uploaded file=["
             << upload_file_info->file_path.value() << "]";
    // Done uploading.
    std::move(upload_file_info->completion_callback)
        .Run(HTTP_SUCCESS, upload_file_info->upload_location, std::move(entry));
  } else {
    DVLOG(1) << "Upload failed " << upload_file_info->DebugString();
    if (error == HTTP_PRECONDITION)
      error = HTTP_CONFLICT;  // ETag mismatch.
    std::move(upload_file_info->completion_callback)
        .Run(error, upload_file_info->upload_location,
             std::unique_ptr<FileResource>());
  }
}

device::mojom::WakeLockProvider* DriveUploader::GetWakeLockProvider() {
  return wake_lock_provider_ ? wake_lock_provider_.get() : nullptr;
}

}  // namespace drive
