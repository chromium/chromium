// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_

#include "chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash::bluetooth_config {

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

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_
