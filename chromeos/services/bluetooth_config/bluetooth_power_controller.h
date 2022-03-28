// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_POWER_CONTROLLER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_POWER_CONTROLLER_H_

class PrefService;

namespace chromeos {
namespace bluetooth_config {

// Sets the Bluetooth power state and saves the state to prefs. Also initializes
// the Bluetooth power state during system startup and user session startup.
//
// Classes that wish to set the Bluetooth adapter state and save that value to
// prefs should use this class. Classes that do not want to persist the state to
// prefs should use AdapterStateController instead.
class BluetoothPowerController {
 public:
  virtual ~BluetoothPowerController() = default;

  // Changes the Bluetooth power setting to |enabled|, persisting |enabled| to
  // user prefs if a user is logged in. If no user is logged in, the pref is
  // persisted to local state.
  virtual void SetBluetoothEnabledState(bool enabled) = 0;

  // If |active| is true, enables Bluetooth but doesn't persist the state to
  // prefs. If |active| is false, restores the Bluetooth enabled state that was
  // last persisted to local state. This should be called to enable Bluetooth
  // when OOBE HID detection starts or to restore the persisted Bluetooth state
  // when OOBE HID detection ends.
  virtual void SetBluetoothHidDetectionActive(bool active) = 0;

  // Sets the PrefServices used to save and retrieve the Bluetooth power state.
  virtual void SetPrefs(PrefService* primary_profile_prefs_,
                        PrefService* local_state) = 0;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_POWER_CONTROLLER_H_
