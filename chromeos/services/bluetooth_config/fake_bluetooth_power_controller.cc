// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_bluetooth_power_controller.h"

namespace chromeos {
namespace bluetooth_config {

FakeBluetoothPowerController::FakeBluetoothPowerController(
    AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {}

FakeBluetoothPowerController::~FakeBluetoothPowerController() = default;

void FakeBluetoothPowerController::SetBluetoothEnabledState(bool enabled) {
  last_enabled_ = enabled;
  adapter_state_controller_->SetBluetoothEnabledState(enabled);
}

void FakeBluetoothPowerController::SetBluetoothHidDetectionActive(bool active) {
  if (active) {
    adapter_state_controller_->SetBluetoothEnabledState(true);
  } else {
    adapter_state_controller_->SetBluetoothEnabledState(last_enabled_);
  }
}

}  // namespace bluetooth_config
}  // namespace chromeos
