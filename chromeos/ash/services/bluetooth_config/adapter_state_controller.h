// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash::bluetooth_config {

// Controls the state of the Bluetooth adapter and serves as the source of truth
// for the adapter's current state. This class modifies the Bluetooth adapter
// directly and should only be used by classes that do not wish to persist the
// adapter state to prefs. For classes that do wish to persist the adapter state
// to prefs, such as those processing incoming user requests,
// BluetoothPowerController should be used instead.
class AdapterStateController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when the state has changed; use GetAdapterState() to retrieve the
    // updated state.
    virtual void OnAdapterStateChanged() = 0;
  };

  virtual ~AdapterStateController();

  // Returns the system state as obtained from the Bluetooth adapter.
  virtual mojom::BluetoothSystemState GetAdapterState() const = 0;

  // Turns Bluetooth on or off. If Bluetooth is unavailable or already in the
  // desired state, this function is a no-op.
  // This does not save to |enabled| to prefs. If |enabled| is wished to be
  // saved to prefs, BluetoothPowerController::SetBluetoothEnabledState() should
  // be used instead.
  virtual void SetBluetoothEnabledState(bool enabled) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  AdapterStateController();

  // Notifies all observers of property changes; should be called by derived
  // types to notify observers of property changes.
  void NotifyAdapterStateChanged();

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_H_
