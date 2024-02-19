// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_POWER_CONTROLLER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_POWER_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/bluetooth_power_controller.h"

#include "base/scoped_observation.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "components/user_manager/user_type.h"

class PrefRegistrySimple;

namespace ash::bluetooth_config {

// Concrete BluetoothPowerController implementation that uses prefs to save and
// apply the Bluetooth power state.
class BluetoothPowerControllerImpl : public BluetoothPowerController,
                                     public AdapterStateController::Observer {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit BluetoothPowerControllerImpl(
      AdapterStateController* adapter_state_controller);
  ~BluetoothPowerControllerImpl() override;

 private:
  // BluetoothPowerController:
  void SetBluetoothEnabledState(bool enabled) override;
  void SetBluetoothEnabledWithoutPersistence() override;
  void SetBluetoothHidDetectionInactive(bool is_using_bluetooth) override;
  void SetPrefs(PrefService* primary_profile_prefs,
                PrefService* local_state) override;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  void InitLocalStatePrefService(PrefService* local_state);

  // At login screen startup, applies the local state Bluetooth power setting
  // or sets the default pref value if the device doesn't have the setting yet.
  void ApplyBluetoothLocalStatePref();

  void InitPrimaryUserPrefService(PrefService* primary_profile_prefs);

  // At primary user session startup, applies the user's Bluetooth power setting
  // or sets the default pref value if the user doesn't have the setting yet.
  void ApplyBluetoothPrimaryUserPref();

  // Sets the Bluetooth power state.
  void SetAdapterState(bool enabled);

  // Saves to prefs the current Bluetooth power state.
  void SaveCurrentPowerStateToPrefs(PrefService* prefs, const char* pref_name);

  // Remembers whether we have ever attempted to apply the primary user's
  // Bluetooth setting. If this variable is true, we will ignore any active
  // user change event since we know that the primary user's Bluetooth setting
  // has been attempted to be applied.
  bool has_attempted_apply_primary_user_pref_ = false;

  // The state the adapter should be set to once it is available. This is set if
  // SetAdapterState() is called before the adapter is available.
  std::optional<bool> pending_adapter_enabled_state_;

  raw_ptr<PrefService> primary_profile_prefs_ = nullptr;
  raw_ptr<PrefService> local_state_ = nullptr;

  raw_ptr<AdapterStateController> adapter_state_controller_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_POWER_CONTROLLER_IMPL_H_
