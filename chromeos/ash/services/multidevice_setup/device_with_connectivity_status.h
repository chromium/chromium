// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_DEVICE_WITH_CONNECTIVITY_STATUS_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_DEVICE_WITH_CONNECTIVITY_STATUS_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"

namespace ash {

namespace multidevice {

// A struct to hold information about a device that can be used in
// the multidevice setup flow. This is separate from RemoteDevice
// since it holds information about connectivity status which is
// used to sort devices in the setup dropdown.
struct DeviceWithConnectivityStatus {
  DeviceWithConnectivityStatus(
      RemoteDeviceRef remote_device,
      cryptauthv2::ConnectivityStatus connectivity_status);
  DeviceWithConnectivityStatus(const DeviceWithConnectivityStatus& other);
  ~DeviceWithConnectivityStatus();

  bool operator==(const DeviceWithConnectivityStatus& other) const;

  RemoteDeviceRef remote_device;
  cryptauthv2::ConnectivityStatus connectivity_status;
};

typedef std::vector<DeviceWithConnectivityStatus>
    DeviceWithConnectivityStatusList;

}  // namespace multidevice

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_DEVICE_WITH_CONNECTIVITY_STATUS_H_
