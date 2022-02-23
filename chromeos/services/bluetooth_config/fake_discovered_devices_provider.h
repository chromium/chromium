// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERED_DEVICES_PROVIDER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERED_DEVICES_PROVIDER_H_

#include "chromeos/services/bluetooth_config/discovered_devices_provider.h"

namespace chromeos {
namespace bluetooth_config {

class FakeDiscoveredDevicesProvider : public DiscoveredDevicesProvider {
 public:
  FakeDiscoveredDevicesProvider();
  ~FakeDiscoveredDevicesProvider() override;

  void SetDiscoveredDevices(
      std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices);

 private:
  // DiscoveredDevicesProvider:
  std::vector<mojom::BluetoothDevicePropertiesPtr> GetDiscoveredDevices()
      const override;

  std::vector<mojom::BluetoothDevicePropertiesPtr> discovered_devices_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERED_DEVICES_PROVIDER_H_
