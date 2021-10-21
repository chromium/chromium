// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/camera_roll_manager.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/camera_roll_item.h"

namespace chromeos {
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

void CameraRollManager::Observer::OnCameraRollViewUiStateUpdated() {}

}  // namespace phonehub
}  // namespace chromeos
