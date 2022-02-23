// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_

#include "chromeos/services/bluetooth_config/bluetooth_device_status_notifier.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace chromeos {
namespace bluetooth_config {

// Fake BluetoothDeviceStatusNotifier implementation.
class FakeBluetoothDeviceStatusNotifier : public BluetoothDeviceStatusNotifier {
 public:
  FakeBluetoothDeviceStatusNotifier();
  ~FakeBluetoothDeviceStatusNotifier() override;

  void SetNewlyPairedDevice(
      const mojom::PairedBluetoothDevicePropertiesPtr& device);

  void SetConnectedDevice(
      const mojom::PairedBluetoothDevicePropertiesPtr& device);

  void SetDisconnectedDevice(
      const mojom::PairedBluetoothDevicePropertiesPtr& device);
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_