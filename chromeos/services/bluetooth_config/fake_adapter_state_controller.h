// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_ADAPTER_STATE_CONTROLLER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_ADAPTER_STATE_CONTROLLER_H_

#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace chromeos {
namespace bluetooth_config {

// Fake AdapterStateController implementation.
class FakeAdapterStateController : public AdapterStateController {
 public:
  FakeAdapterStateController();
  ~FakeAdapterStateController() override;

  // AdapterStateController:
  mojom::BluetoothSystemState GetAdapterState() const override;

  void SetSystemState(mojom::BluetoothSystemState system_state);

 private:
  // AdapterStateController:
  void SetBluetoothEnabledState(bool enabled) override;

  mojom::BluetoothSystemState system_state_ =
      mojom::BluetoothSystemState::kEnabled;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_ADAPTER_STATE_CONTROLLER_H_
