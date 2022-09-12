// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"

#include "base/run_loop.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"

namespace ash::bluetooth_config {

FakeAdapterStateController::FakeAdapterStateController() = default;

FakeAdapterStateController::~FakeAdapterStateController() = default;

mojom::BluetoothSystemState FakeAdapterStateController::GetAdapterState()
    const {
  return system_state_;
}

void FakeAdapterStateController::SetSystemState(
    mojom::BluetoothSystemState system_state) {
  if (system_state_ == system_state)
    return;

  system_state_ = system_state;
  NotifyAdapterStateChanged();
  base::RunLoop().RunUntilIdle();
}

void FakeAdapterStateController::SetBluetoothEnabledState(bool enabled) {
  if (system_state_ == mojom::BluetoothSystemState::kUnavailable)
    return;

  if (IsBluetoothEnabledOrEnabling(system_state_) == enabled)
    return;

  SetSystemState(enabled ? mojom::BluetoothSystemState::kEnabling
                         : mojom::BluetoothSystemState::kDisabling);
}

}  // namespace ash::bluetooth_config
