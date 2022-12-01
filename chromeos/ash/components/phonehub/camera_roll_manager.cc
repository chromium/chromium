// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/camera_roll_manager.h"

#include "chromeos/ash/components/phonehub/camera_roll_item.h"

namespace ash {
namespace phonehub {

CameraRollManager::CameraRollManager() = default;

CameraRollManager::~CameraRollManager() = default;

void CameraRollManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CameraRollManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

CameraRollManager::CameraRollUiState CameraRollManager::ui_state() {
  return ui_state_;
}

void CameraRollManager::SetCurrentItems(
    const std::vector<CameraRollItem>& items) {
  current_items_ = items;
  ComputeAndUpdateUiState();
}

void CameraRollManager::ClearCurrentItems() {
  current_items_.clear();
  ComputeAndUpdateUiState();
}

void CameraRollManager::NotifyCameraRollViewUiStateUpdated() {
  for (auto& observer : observer_list_) {
    observer.OnCameraRollViewUiStateUpdated();
  }
}

void CameraRollManager::NotifyCameraRollDownloadError(
    CameraRollManager::Observer::DownloadErrorType error_type,
    const proto::CameraRollItemMetadata& metadata) {
  for (auto& observer : observer_list_) {
    observer.OnCameraRollDownloadError(error_type, metadata);
  }
}

void CameraRollManager::Observer::OnCameraRollViewUiStateUpdated() {}

void CameraRollManager::Observer::OnCameraRollDownloadError(
    DownloadErrorType error_type,
    const proto::CameraRollItemMetadata& metadata) {}

}  // namespace phonehub
}  // namespace ash
