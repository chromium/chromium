// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"

namespace ash {

namespace tether {

DeviceIdTetherNetworkGuidMap::DeviceIdTetherNetworkGuidMap() = default;

DeviceIdTetherNetworkGuidMap::~DeviceIdTetherNetworkGuidMap() = default;

std::string DeviceIdTetherNetworkGuidMap::GetDeviceIdForTetherNetworkGuid(
    const std::string& tether_network_guid) {
  // TODO(hansberry): Use real implementation.
  return tether_network_guid;
}

std::string DeviceIdTetherNetworkGuidMap::GetTetherNetworkGuidForDeviceId(
    const std::string& device_id) {
  // TODO(hansberry): Use real implementation.
  return device_id;
}

}  // namespace tether

}  // namespace ash
