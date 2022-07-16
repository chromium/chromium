// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/adapter_state_controller.h"

namespace chromeos {
namespace bluetooth_config {

AdapterStateController::AdapterStateController() = default;

AdapterStateController::~AdapterStateController() = default;

void AdapterStateController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AdapterStateController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AdapterStateController::NotifyAdapterStateChanged() {
  for (auto& observer : observers_)
    observer.OnAdapterStateChanged();
}

}  // namespace bluetooth_config
}  // namespace chromeos
