// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/find_my_device_controller.h"

namespace chromeos {
namespace phonehub {

FindMyDeviceController::FindMyDeviceController() = default;

FindMyDeviceController::~FindMyDeviceController() = default;

void FindMyDeviceController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FindMyDeviceController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FindMyDeviceController::NotifyPhoneRingingStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnPhoneRingingStateChanged();
}

}  // namespace phonehub
}  // namespace chromeos
