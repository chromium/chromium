// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::bluetooth_config {

// AdapterStateController implementation which uses BluetoothAdapter.
class AdapterStateControllerImpl : public AdapterStateController,
                                   public device::BluetoothAdapter::Observer {
 public:
  AdapterStateControllerImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  ~AdapterStateControllerImpl() override;

 private:
  friend class AdapterStateControllerImplTest;

  enum class PowerStateChange { kNoChange, kEnable, kDisable };

  friend std::ostream& operator<<(std::ostream& stream,
                                  const PowerStateChange& power_state_change);

  // AdapterStateController:
  mojom::BluetoothSystemState GetAdapterState() const override;
  void SetBluetoothEnabledState(bool enabled) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  void AttemptQueuedStateChange();
  void AttemptSetEnabled(bool enabled);

  void OnSetPoweredError(bool enabled);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // The in-progress state change, if one exists. If this value is kNoChange,
  // this class is not attempting to change the state. If this value is kEnable
  // or kDisable, this class is currently attempting to change to the
  // associated state.
  PowerStateChange in_progress_state_change_ = PowerStateChange::kNoChange;

  // The queued state change, if one exists. When a state change is in progress
  // and this class is requested to change the state again (e.g., turning on and
  // off in quick succession), we queue the second state change and only attempt
  // it after the first change completes.
  PowerStateChange queued_state_change_ = PowerStateChange::kNoChange;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};

  base::WeakPtrFactory<AdapterStateControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_IMPL_H_
