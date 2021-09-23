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

void CameraRollManager::SetCurrentItems(
    const std::vector<CameraRollItem>& items) {
  if (current_items_ == items) {
    return;
  }
  current_items_ = items;
  NotifyCameraRollItemsChanged();
}

void CameraRollManager::ClearCurrentItems() {
  if (current_items_.empty()) {
    return;
  }
  current_items_.clear();
  NotifyCameraRollItemsChanged();
}

void CameraRollManager::NotifyCameraRollItemsChanged() {
  PA_LOG(INFO) << "Updated the list of Camera Roll items";
  for (auto& observer : observer_list_) {
    observer.OnCameraRollItemsChanged();
  }
}

}  // namespace phonehub
}  // namespace chromeos
