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

namespace {

const void* const kUserDataKey = &kUserDataKey;
constexpr char kDefaultCameraFolderName[] = "Camera";

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
    case FileSaveDestination::kOneDrive:
      // TODO(crbug.com/454152412) Implement this.
      NOTIMPLEMENTED_LOG_ONCE();
      return base::FilePath();
    case FileSaveDestination::kGoogleDrive:
      // TODO(crbug.com/454152412) Implement this.
      NOTIMPLEMENTED_LOG_ONCE();
      return base::FilePath();
    case FileSaveDestination::kLocal:
      return delegate_->GetMyFilesFolder();
  }
}

base::FilePath CameraSaveHandler::GetWritablePathRelativeToRoot() const {
  CHECK(delegate_);
  switch (delegate_->GetDestination()) {
    case FileSaveDestination::kOneDrive:
      // TODO(crbug.com/454152412) Implement this.
      NOTIMPLEMENTED_LOG_ONCE();
      return base::FilePath();
    case FileSaveDestination::kGoogleDrive:
      // TODO(crbug.com/454152412) Implement this.
      NOTIMPLEMENTED_LOG_ONCE();
      return base::FilePath();
    case FileSaveDestination::kLocal:
      // Only support Camera folder in MyFiles for local destination.
      return base::FilePath(kDefaultCameraFolderName);
  }
}

base::FilePath CameraSaveHandler::GetWritablePath() const {
  return GetWritableRoot().Append(GetWritablePathRelativeToRoot());
}

base::FilePath CameraSaveHandler::GetFinalPath() const {
  return GetWritablePath();
}
