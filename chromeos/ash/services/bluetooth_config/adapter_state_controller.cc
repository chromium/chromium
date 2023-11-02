// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"

namespace ash::bluetooth_config {

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

}  // namespace ash::bluetooth_config
