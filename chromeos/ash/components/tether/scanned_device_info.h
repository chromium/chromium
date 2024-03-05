// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_SCANNED_DEVICE_INFO_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_SCANNED_DEVICE_INFO_H_

#include "base/types/expected.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash::tether {

enum ScannedDeviceInfoError { kNotificationsDisabled };

struct ScannedDeviceInfo {
  ScannedDeviceInfo(multidevice::RemoteDeviceRef remote_device,
                    const DeviceStatus& device_status,
                    bool setup_required);
  ~ScannedDeviceInfo();

  friend bool operator==(const ScannedDeviceInfo& first,
                         const ScannedDeviceInfo& second);

  multidevice::RemoteDeviceRef remote_device;
  DeviceStatus device_status;
  bool setup_required;
};

using ScannedDeviceResult =
    base::expected<ScannedDeviceInfo, ScannedDeviceInfoError>;

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_SCANNED_DEVICE_INFO_H_
