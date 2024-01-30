// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_wifi_hotspot_connector.h"
#include "chromeos/ash/components/network/network_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

FakeWifiHotspotConnector::FakeWifiHotspotConnector(
    NetworkHandler* network_handler)
    : WifiHotspotConnector(network_handler, nullptr /* network_connect */) {}

FakeWifiHotspotConnector::~FakeWifiHotspotConnector() = default;

void FakeWifiHotspotConnector::CallMostRecentCallback(
    base::expected<std::string,
                   WifiHotspotConnector::WifiHotspotConnectionError> result) {
  EXPECT_FALSE(!most_recent_callback_);
  std::move(most_recent_callback_).Run(result);
}

void FakeWifiHotspotConnector::ConnectToWifiHotspot(
    const std::string& ssid,
    const std::string& password,
    const std::string& tether_network_guid,
    WifiHotspotConnector::WifiConnectionCallback callback) {
  most_recent_ssid_ = ssid;
  most_recent_password_ = password;
  most_recent_tether_network_guid_ = tether_network_guid;
  most_recent_callback_ = std::move(callback);
}

}  // namespace tether

}  // namespace ash
