// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CONVERSION_UTIL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CONVERSION_UTIL_H_

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace device {
class BluetoothDevice;
}  // namespace device

namespace ash::bluetooth_config {

class FastPairDelegate;

// If |fast_pair_delegate| is non-null it will load device images into
// the returned BluetoothDevicePropertiesPtr.
mojom::BluetoothDevicePropertiesPtr GenerateBluetoothDeviceMojoProperties(
    const device::BluetoothDevice* device,
    FastPairDelegate* fast_pair_delegate);

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CONVERSION_UTIL_H_
