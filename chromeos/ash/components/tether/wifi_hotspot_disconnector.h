// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_H_

#include "base/functional/callback.h"
#include "chromeos/ash/components/network/network_connection_handler.h"

namespace ash {

namespace tether {

// Disconnects from Wi-Fi hotspots provided by Tether hosts.
class WifiHotspotDisconnector {
 public:
  using StringErrorCallback =
      NetworkConnectionHandler::TetherDelegate::StringErrorCallback;

  WifiHotspotDisconnector() {}

  WifiHotspotDisconnector(const WifiHotspotDisconnector&) = delete;
  WifiHotspotDisconnector& operator=(const WifiHotspotDisconnector&) = delete;

  virtual ~WifiHotspotDisconnector() {}

  // Disconnects from the Wi-Fi network with GUID |wifi_network_guid| and
  // removes the corresponding network configuration (i.e., removes the "known
  // network" from network settings).
  virtual void DisconnectFromWifiHotspot(
      const std::string& wifi_network_guid,
      base::OnceClosure success_callback,
      StringErrorCallback error_callback) = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_WIFI_HOTSPOT_DISCONNECTOR_H_
