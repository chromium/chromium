// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_CONNECTOR_H_

#include <string>

#include "chromeos/ash/components/tether/wifi_hotspot_connector.h"

namespace ash {

namespace tether {

// Test double for WifiHotspotConnector.
class FakeWifiHotspotConnector : public WifiHotspotConnector {
 public:
  explicit FakeWifiHotspotConnector(NetworkHandler* network_handler);

  FakeWifiHotspotConnector(const FakeWifiHotspotConnector&) = delete;
  FakeWifiHotspotConnector& operator=(const FakeWifiHotspotConnector&) = delete;

  ~FakeWifiHotspotConnector() override;

  // Pass an empty string for |wifi_guid| to signify a failed connection.
  void CallMostRecentCallback(
      base::expected<std::string,
                     WifiHotspotConnector::WifiHotspotConnectionError> result);

  std::string most_recent_ssid() { return most_recent_ssid_; }

  std::string most_recent_password() { return most_recent_password_; }

  std::string most_recent_tether_network_guid() {
    return most_recent_tether_network_guid_;
  }

  // WifiHotspotConnector:
  void ConnectToWifiHotspot(
      const std::string& ssid,
      const std::string& password,
      const std::string& tether_network_guid,
      WifiHotspotConnector::WifiConnectionCallback callback) override;

 private:
  std::string most_recent_ssid_;
  std::string most_recent_password_;
  std::string most_recent_tether_network_guid_;
  WifiHotspotConnector::WifiConnectionCallback most_recent_callback_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_CONNECTOR_H_
