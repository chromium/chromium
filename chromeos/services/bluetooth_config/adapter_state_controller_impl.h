// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace bluetooth_config {

// AdapterStateController implementation which uses BluetoothAdapter.
class AdapterStateControllerImpl : public AdapterStateController,
                                   public device::BluetoothAdapter::Observer {
 public:
  AdapterStateControllerImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  ~AdapterStateControllerImpl() override;

 private:
  friend class AdapterStateControllerImplTest;

  // AdapterStateController:
  mojom::BluetoothSystemState GetAdapterState() const override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_ADAPTER_STATE_CONTROLLER_IMPL_H_
