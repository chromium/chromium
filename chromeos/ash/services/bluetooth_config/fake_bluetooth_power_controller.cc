// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_bluetooth_power_controller.h"

namespace ash::bluetooth_config {

FakeBluetoothPowerController::FakeBluetoothPowerController(
    AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {}

FakeBluetoothPowerController::~FakeBluetoothPowerController() = default;

void FakeBluetoothPowerController::SetBluetoothEnabledState(bool enabled) {
  last_enabled_ = enabled;
  adapter_state_controller_->SetBluetoothEnabledState(enabled);
}

void FakeBluetoothPowerController::SetBluetoothEnabledWithoutPersistence() {
  adapter_state_controller_->SetBluetoothEnabledState(true);
}

void FakeBluetoothPowerController::SetBluetoothHidDetectionInactive(
    bool is_using_bluetooth) {
  // If Bluetooth is being used, don't restore the persisted adapter state.
  if (is_using_bluetooth)
    return;

  adapter_state_controller_->SetBluetoothEnabledState(last_enabled_);
}

}  // namespace ash::bluetooth_config
