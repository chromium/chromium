// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_DISCONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_DISCONNECTOR_H_

#include "base/functional/callback.h"
#include "chromeos/ash/components/tether/wifi_hotspot_disconnector.h"

namespace ash {

namespace tether {

// Test double for WifiHotspotDisconnector.
class FakeWifiHotspotDisconnector : public WifiHotspotDisconnector {
 public:
  FakeWifiHotspotDisconnector();

  FakeWifiHotspotDisconnector(const FakeWifiHotspotDisconnector&) = delete;
  FakeWifiHotspotDisconnector& operator=(const FakeWifiHotspotDisconnector&) =
      delete;

  ~FakeWifiHotspotDisconnector() override;

  std::string last_disconnected_wifi_network_guid() {
    return last_disconnected_wifi_network_guid_;
  }

  void set_disconnection_error_name(
      const std::string& disconnection_error_name) {
    disconnection_error_name_ = disconnection_error_name;
  }

  // WifiHotspotDisconnector:
  void DisconnectFromWifiHotspot(const std::string& wifi_network_guid,
                                 base::OnceClosure success_callback,
                                 StringErrorCallback error_callback) override;

 private:
  std::string last_disconnected_wifi_network_guid_;
  std::string disconnection_error_name_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_DISCONNECTOR_H_
