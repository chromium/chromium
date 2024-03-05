// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/scanned_device_info.h"

namespace ash::tether {

ScannedDeviceInfo::ScannedDeviceInfo(multidevice::RemoteDeviceRef remote_device,
                                     const DeviceStatus& device_status,
                                     bool setup_required)
    : remote_device(remote_device),
      device_status(device_status),
      setup_required(setup_required) {}

ScannedDeviceInfo::~ScannedDeviceInfo() = default;

bool operator==(const ScannedDeviceInfo& first,
                const ScannedDeviceInfo& second) {
  return first.remote_device == second.remote_device &&
         first.device_status.SerializeAsString() ==
             second.device_status.SerializeAsString() &&
         first.setup_required == second.setup_required;
}

}  // namespace ash::tether
