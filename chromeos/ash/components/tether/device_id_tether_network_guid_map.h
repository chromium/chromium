// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_DEVICE_ID_TETHER_NETWORK_GUID_MAP_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_DEVICE_ID_TETHER_NETWORK_GUID_MAP_H_

#include <string>

namespace ash {

namespace tether {

// Keeps a mapping between device ID and the tether network GUID associated with
// tethering to that device.
// TODO(hansberry): Currently, this class is stubbed out by simply returning the
//                  same value for both device ID and tether network GUID.
//                  Figure out a real mapping system.
class DeviceIdTetherNetworkGuidMap {
 public:
  DeviceIdTetherNetworkGuidMap();

  DeviceIdTetherNetworkGuidMap(const DeviceIdTetherNetworkGuidMap&) = delete;
  DeviceIdTetherNetworkGuidMap& operator=(const DeviceIdTetherNetworkGuidMap&) =
      delete;

  virtual ~DeviceIdTetherNetworkGuidMap();

  // Returns the device ID for a given tether network GUID.
  virtual std::string GetDeviceIdForTetherNetworkGuid(
      const std::string& tether_network_guid);

  // Returns the tether network GUID for a given device ID.
  virtual std::string GetTetherNetworkGuidForDeviceId(
      const std::string& device_id);
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_DEVICE_ID_TETHER_NETWORK_GUID_MAP_H_
