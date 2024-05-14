// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/bluetooth_helper.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::secure_channel {

BluetoothHelper::BluetoothHelper() = default;

BluetoothHelper::~BluetoothHelper() = default;

std::optional<BluetoothHelper::DeviceWithBackgroundBool>
BluetoothHelper::IdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  std::optional<DeviceWithBackgroundBool>
      potential_device_with_background_bool =
          PerformIdentifyRemoteDevice(service_data, device_id_pair_set);

  if (!potential_device_with_background_bool)
    return std::nullopt;

  const std::string remote_device_id =
      potential_device_with_background_bool->first.GetDeviceId();
  for (const auto& device_id_pair : device_id_pair_set) {
    if (remote_device_id == device_id_pair.remote_device_id())
      return potential_device_with_background_bool;
  }

  PA_LOG(ERROR) << "BluetoothHelper::IdentifyRemoteDevice(): Identified "
                   "device was not present in the provided DeviceIdPairSet.";
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

}  // namespace ash::secure_channel
