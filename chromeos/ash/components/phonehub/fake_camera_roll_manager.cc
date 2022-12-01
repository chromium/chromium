// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_camera_roll_manager.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

FakeCameraRollManager::FakeCameraRollManager() = default;

FakeCameraRollManager::~FakeCameraRollManager() = default;

void FakeCameraRollManager::DownloadItem(
    const proto::CameraRollItemMetadata& item_metadata) {
  if (is_simulating_error_) {
    NotifyCameraRollDownloadError(simulated_error_type_, item_metadata);
    PA_LOG(VERBOSE) << "Fake Camera Roll Download: Error";
  } else {
    PA_LOG(VERBOSE) << "Fake Camera Roll Download: Success";
  }
  download_request_count_++;
}

void FakeCameraRollManager::SetIsCameraRollAvailableToBeEnabled(
    bool can_enable) {
  is_avaiable_to_be_enabled_ = can_enable;
  ComputeAndUpdateUiState();
}

void FakeCameraRollManager::SetIsCameraRollAccessible(bool accessiable) {
  is_camera_roll_accessible_ = accessiable;
  ComputeAndUpdateUiState();
}

void FakeCameraRollManager::SetIsAndroidStorageGranted(bool granted) {
  is_android_storage_granted_ = granted;
  ComputeAndUpdateUiState();
}

void FakeCameraRollManager::SetSimulatedDownloadError(bool has_error) {
  is_simulating_error_ = has_error;
}

void FakeCameraRollManager::SetSimulatedErrorType(
    Observer::DownloadErrorType error_type) {
  simulated_error_type_ = error_type;
}

int FakeCameraRollManager::GetDownloadRequestCount() {
  return download_request_count_;
}

void FakeCameraRollManager::ComputeAndUpdateUiState() {
  if (!is_camera_roll_accessible_) {
    ui_state_ = CameraRollUiState::SHOULD_HIDE;
  } else if (!is_android_storage_granted_) {
    ui_state_ = CameraRollUiState::NO_STORAGE_PERMISSION;
  } else if (is_avaiable_to_be_enabled_) {
    ui_state_ = CameraRollUiState::SHOULD_HIDE;
  } else if (current_items().empty()) {
    ui_state_ = CameraRollUiState::SHOULD_HIDE;
  } else {
    ui_state_ = CameraRollUiState::ITEMS_VISIBLE;
  }
  NotifyCameraRollViewUiStateUpdated();
}

}  // namespace phonehub
}  // namespace ash
