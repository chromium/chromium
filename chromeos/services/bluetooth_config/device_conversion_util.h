// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_CONVERSION_UTIL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_CONVERSION_UTIL_H_

#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace chromeos {
namespace bluetooth_config {

mojom::BluetoothDevicePropertiesPtr GenerateBluetoothDeviceMojoProperties(
    const device::BluetoothDevice* device);

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_CONVERSION_UTIL_H_
