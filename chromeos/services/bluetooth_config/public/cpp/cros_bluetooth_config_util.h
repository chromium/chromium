// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_PUBLIC_CPP_CROS_BLUETOOTH_CONFIG_UTIL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_PUBLIC_CPP_CROS_BLUETOOTH_CONFIG_UTIL_H_

#include <string>

#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace chromeos {
namespace bluetooth_config {

// Returns |true| if the given Bluetooth state |system_state| is either enabled
// or being enabled.
bool IsBluetoothEnabledOrEnabling(
    const mojom::BluetoothSystemState system_state);

// Returns the nickname of the provided device if it is set, otherwise returns
// the public device name.
std::u16string GetPairedDeviceName(
    const mojom::PairedBluetoothDevicePropertiesPtr& paired_device_properties);

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_PUBLIC_CPP_CROS_BLUETOOTH_CONFIG_UTIL_H_
