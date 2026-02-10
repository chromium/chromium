// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/camera_save_handler.h"

#include "ash/system/camera/camera_app_prefs.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/experiences/camera/camera_upload_notification.h"
#include "chromeos/ash/experiences/camera/cancel_camera_upload_dialog.h"
#include "chromeos/ash/experiences/camera/upload_done_notification.h"
#include "chromeos/ash/experiences/camera/upload_error_notification.h"
#include "ui/gfx/image/image.h"

namespace {

const void* const kUserDataKey = &kUserDataKey;
constexpr char kDefaultCameraFolderName[] = "Camera";
constexpr char kOneDriveCacheFolderName[] = "CameraOneDriveCache";

void DeleteFileAsync(const base::FilePath& path) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), path));
}

}  // namespace

CameraSaveHandler::Upload::Upload(base::OnceCallback<void(bool)> done_callback,
                                  int64_t file_size)
    : done_callback(std::move(done_callback)), file_size(file_size) {}

CameraSaveHandler::Upload::~Upload() {
  CHECK(!done_callback)
      << "Upload done callback not called before destruction.";
}

// static
void CameraSaveHandler::Create(base::SupportsUserData& context,
                               std::unique_ptr<Delegate> delegate) {
  CHECK(!context.GetUserData(kUserDataKey));
  context.SetUserData(
      kUserDataKey,
      base::WrapUnique(new CameraSaveHandler(std::move(delegate))));
}

// static
CameraSaveHandler* CameraSaveHandler::Get(
    const base::SupportsUserData& context) {
  return static_cast<CameraSaveHandler*>(context.GetUserData(kUserDataKey));
}

CameraSaveHandler::CameraSaveHandler(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

CameraSaveHandler::~CameraSaveHandler() {
  // Cancel and cleanup ongoing uploads.
  CHECK(delegate_);
  if (delegate_->GetDestination() != FileSaveDestination::kLocal) {
    CancelUploads();
  }
}

CameraSaveHandler::FileSaveDestination CameraSaveHandler::GetDestination()
    const {
  CHECK(delegate_);
  return delegate_->GetDestination();
}

base::FilePath CameraSaveHandler::GetWritableRoot() const {
  CHECK(delegate_);
  switch (delegate_->GetDestination()) {
    case FileSaveDestination::kOneDrive: {
      base::FilePath path;
      CHECK(base::GetTempDir(&path));
      return path;
    }
    case FileSaveDestination::kGoogleDrive:
      return delegate_->GetGoogleDriveRoot();
    case FileSaveDestination::kLocal:
      return delegate_->GetMyFilesFolder();
  }
}

base::FilePath CameraSaveHandler::GetWritablePathRelativeToRoot() const {
  CHECK(delegate_);
  switch (delegate_->GetDestination()) {
    case FileSaveDestination::kOneDrive:
      return base::FilePath(kOneDriveCacheFolderName);
    case FileSaveDestination::kGoogleDrive: {
      auto path = delegate_->GetFinalPathRelativeToRoot();
      if (path.empty()) {
        return base::FilePath(kDefaultCameraFolderName);
      }
      return path;
    }
    case FileSaveDestination::kLocal:
      // Only support Camera folder in MyFiles for local destination.
      return base::FilePath(kDefaultCameraFolderName);
  }
}

base::FilePath CameraSaveHandler::GetWritablePath() const {
  return GetWritableRoot().Append(GetWritablePathRelativeToRoot());
}

base::FilePath CameraSaveHandler::GetFilePathBeforeUpload(
    const base::FilePath& base_name) const {
  CHECK_EQ(base_name.BaseName(), base_name)
      << "Expected base name: " << base_name;
  return GetWritablePath().Append(base_name);
}

base::FilePath CameraSaveHandler::GetFinalPath() const {
  return delegate_->GetDestination() == FileSaveDestination::kOneDrive
             ? delegate_->GetOneDriveUploadFolder()
             : GetWritablePath();
}

void CameraSaveHandler::UploadFile(const std::string& name,
                                   const gfx::Image& thumbnail,
                                   base::OnceCallback<void(bool)> callback) {
  CHECK(delegate_);
  CHECK_NE(delegate_->GetDestination(), FileSaveDestination::kLocal);
  auto upload_from_path = GetFilePathBeforeUpload(base::FilePath(name));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetFileSizeCallback(upload_from_path),
      base::BindOnce(&CameraSaveHandler::PerformUpload,
                     weak_ptr_factory_.GetWeakPtr(), upload_from_path,
                     thumbnail, std::move(callback)));
}

void CameraSaveHandler::PerformUpload(const base::FilePath& upload_from_path,
                                      const gfx::Image& thumbnail,
                                      base::OnceCallback<void(bool)> callback,
                                      std::optional<int64_t> file_size) {
  if (!file_size.has_value()) {
    std::move(callback).Run(false);
    return;
  }
  CHECK(delegate_);
  CHECK_NE(delegate_->GetDestination(), FileSaveDestination::kLocal);
  TrackUpload(upload_from_path,
              std::make_unique<Upload>(std::move(callback), file_size.value()));
  delegate_->PerformUpload(
      upload_from_path, file_size.value(), thumbnail,
      base::BindRepeating(&CameraSaveHandler::OnUploadProgress,
                          weak_ptr_factory_.GetWeakPtr(), upload_from_path),
      base::BindOnce(&CameraSaveHandler::OnUploadDone,
                     weak_ptr_factory_.GetWeakPtr(), upload_from_path,
                     thumbnail));
}

void CameraSaveHandler::TrackUpload(const base::FilePath& upload_from_path,
                                    std::unique_ptr<Upload> upload) {
  total_size_of_uploads_ += upload->file_size;
  CHECK(uploads_
            .emplace(std::pair(upload_from_path.BaseName(), std::move(upload)))
            .second)
      << "Duplicate file upload: " << upload_from_path.BaseName();
  UpdateProgressNotification();
}

void CameraSaveHandler::UntrackUpload(const base::FilePath& upload_from_path,
                                      bool success) {
  auto it = uploads_.find(upload_from_path.BaseName());
  if (it == uploads_.end()) {
    LOG(ERROR) << "Upload not found: " << upload_from_path.BaseName();
    return;
  }
  auto& upload = it->second;
  std::move(upload->done_callback).Run(success);
  total_bytes_uploaded_ -= upload->bytes_uploaded;
  CHECK(total_bytes_uploaded_ >= 0);
  total_size_of_uploads_ -= upload->file_size;
  CHECK(total_size_of_uploads_ >= 0);
  uploads_.erase(it);
  UpdateProgressNotification();
}

void CameraSaveHandler::UpdateProgressNotification() {
  CHECK(delegate_);
  if (uploads_.empty()) {
    CHECK_EQ(total_bytes_uploaded_, 0);
    CHECK_EQ(total_size_of_uploads_, 0);
    // Close the cancel upload confirmation dialog because all uploads are done.
    cancel_dialog_.reset();
    progress_notification_.reset();
    return;
  }
  double progress =
      (total_size_of_uploads_ == 0
           ? 100
           : (total_bytes_uploaded_ * 100 / total_size_of_uploads_));
  if (!progress_notification_) {
    progress_notification_ = std::make_unique<CameraUploadNotification>(
        delegate_->GetDestination(),
        base::BindOnce(ash::camera_app_prefs::ShouldSkipCancelUploadDialog()
                           ? &CameraSaveHandler::CancelUploads
                           : &CameraSaveHandler::ShowCancelDialog,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  progress_notification_->UpdateProgress(progress, uploads_.size());
}

void CameraSaveHandler::ShowCancelDialog() {
  cancel_dialog_ = std::make_unique<CancelCameraUploadDialog>(
      base::BindRepeating(&CameraSaveHandler::OnCancelDialogClicked,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CameraSaveHandler::OnCancelDialogClicked(bool cancel,
                                              bool skip_dialog_next_time) {
  if (cancel) {
    CancelUploads();
  }
  if (skip_dialog_next_time) {
    ash::camera_app_prefs::SetSkipCancelUploadDialog();
  }
}

void CameraSaveHandler::CancelUploads() {
  if (uploads_.empty()) {
    return;
  }
  CHECK(delegate_);
  delegate_->CancelUploads();
  for (const auto& [file, upload] : uploads_) {
    if (delegate_->GetDestination() == FileSaveDestination::kOneDrive) {
      // Clean up all temp files used for OneDrive uploads so that the
      // thumbnails for these files aren't shown in the camera app when it is
      // launched next.
      DeleteFileAsync(GetFilePathBeforeUpload(file));
    }
    std::move(upload->done_callback).Run(false);
  }
  uploads_.clear();
  total_bytes_uploaded_ = 0;
  total_size_of_uploads_ = 0;
  progress_notification_.reset();
}

void CameraSaveHandler::OnUploadProgress(const base::FilePath& upload_from_path,
                                         int64_t bytes_uploaded) {
  auto it = uploads_.find(upload_from_path.BaseName());
  if (it == uploads_.end()) {
    return;
  }
  auto prev_bytes_uploaded = it->second->bytes_uploaded;
  CHECK(bytes_uploaded >= prev_bytes_uploaded);
  it->second->bytes_uploaded = bytes_uploaded;
  total_bytes_uploaded_ += (bytes_uploaded - prev_bytes_uploaded);
  UpdateProgressNotification();
}

void CameraSaveHandler::OnUploadDone(
    const base::FilePath& upload_from_path,
    const gfx::Image& thumbnail,
    bool success,
    std::optional<base::FilePath> uploaded_path) {
  UntrackUpload(upload_from_path, success);
  auto uploaded_file_path = uploaded_path.value_or(
      GetFinalPath().Append(upload_from_path.BaseName()));
  DCHECK(delegate_);
  if (success) {
    CreateUploadDoneNotification(
        delegate_->GetDestination() == FileSaveDestination::kOneDrive,
        thumbnail, uploaded_file_path,
        base::BindRepeating(&CameraSaveHandler::OpenFileInImageEditor,
                            weak_ptr_factory_.GetWeakPtr(), uploaded_file_path),
        base::BindRepeating(&CameraSaveHandler::DeleteFileAfterUpload,
                            weak_ptr_factory_.GetWeakPtr(),
                            uploaded_file_path));
  } else {
    CreateUploadErrorNotification(
        thumbnail, upload_from_path,
        base::BindRepeating(&CameraSaveHandler::OnUploadErrorRetake,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  if (!success &&
      delegate_->GetDestination() == FileSaveDestination::kOneDrive) {
    // Clean up the temp file if upload fails, so that the thumbnail for this
    // file isn't shown in the camera app when it is launched next.
    DeleteFileAsync(upload_from_path);
  }
}

void CameraSaveHandler::OpenFileInImageEditor(const base::FilePath& file_path) {
  CHECK(delegate_);
  delegate_->OpenFileInImageEditor(file_path);
}

void CameraSaveHandler::DeleteFileAfterUpload(const base::FilePath& file_path) {
  CHECK(delegate_);
  auto callback = base::BindOnce([](const base::FilePath& path, bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to delete the file: " << path;
    }
  });
  if (delegate_->GetDestination() == FileSaveDestination::kOneDrive) {
    delegate_->DeleteFileOnOneDrive(
        file_path, base::BindOnce(std::move(callback), file_path));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&base::DeleteFile, file_path),
        base::BindOnce(std::move(callback), file_path));
  }
}

void CameraSaveHandler::OnUploadErrorRetake() {
  CHECK(delegate_);
  delegate_->OpenCameraApp();
}
