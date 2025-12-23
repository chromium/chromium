// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/camera_save_handler.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/image/image.h"

namespace {

const void* const kUserDataKey = &kUserDataKey;
constexpr char kDefaultCameraFolderName[] = "Camera";
constexpr char kOneDriveCacheFolderName[] = "CameraOneDriveCache";

void DeleteFileAsync(const base::FilePath& path) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), path));
}

}  // namespace

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

CameraSaveHandler::~CameraSaveHandler() = default;

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
  auto upload_from_path = GetWritablePath().Append(name);
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
  delegate_->PerformUpload(
      upload_from_path, file_size.value(), thumbnail,
      base::BindRepeating(&CameraSaveHandler::OnUploadProgress,
                          weak_ptr_factory_.GetWeakPtr(), upload_from_path),
      base::BindOnce(&CameraSaveHandler::OnUploadDone,
                     weak_ptr_factory_.GetWeakPtr(), upload_from_path,
                     std::move(callback)));
}

void CameraSaveHandler::OnUploadProgress(const base::FilePath&, int64_t) {
  // TODO(crbug.com/454152412) Implement progress notification.
}

void CameraSaveHandler::OnUploadDone(const base::FilePath& upload_from_path,
                                     base::OnceCallback<void(bool)> callback,
                                     bool success) {
  std::move(callback).Run(success);
  DCHECK(delegate_);
  if (!success &&
      delegate_->GetDestination() == FileSaveDestination::kOneDrive) {
    // Clean up the temp file if upload fails, so that the thumbnail for this
    // file isn't shown in the camera app when it is launched next.
    DeleteFileAsync(upload_from_path);
  }
}
