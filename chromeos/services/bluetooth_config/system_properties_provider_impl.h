// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/system_properties_provider.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
namespace bluetooth_config {

// SystemPropertiesProvider implementation which uses BluetoothAdapter as the
// source of properties.
class SystemPropertiesProviderImpl : public SystemPropertiesProvider,
                                     public device::BluetoothAdapter::Observer {
 public:
  explicit SystemPropertiesProviderImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  ~SystemPropertiesProviderImpl() override;

 private:
  friend class SystemPropertiesProviderImplTest;

  // SystemPropertiesProvider:
  mojom::BluetoothSystemPropertiesPtr GenerateProperties() override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  mojom::BluetoothSystemState ComputeSystemState() const;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_IMPL_H_
