// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_POWER_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_POWER_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/bluetooth_power_controller.h"

namespace ash::bluetooth_config {

class FakeBluetoothPowerController : public BluetoothPowerController {
 public:
  explicit FakeBluetoothPowerController(
      AdapterStateController* adapter_state_controller);
  ~FakeBluetoothPowerController() override;

  // BluetoothPowerController:
  void SetBluetoothEnabledState(bool enabled) override;

 private:
  // BluetoothPowerController:
  void SetBluetoothEnabledWithoutPersistence() override;
  void SetBluetoothHidDetectionInactive(bool is_using_bluetooth) override;
  void SetPrefs(PrefService* logged_in_profile_prefs,
                PrefService* local_state) override {}

  // Mocks the enabled state persisted to local state prefs. This defaults to
  // enabled on a fresh device.
  bool last_enabled_ = true;

  raw_ptr<AdapterStateController> adapter_state_controller_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_POWER_CONTROLLER_H_
