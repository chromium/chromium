// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_wifi_hotspot_disconnector.h"

namespace ash {

namespace tether {

FakeWifiHotspotDisconnector::FakeWifiHotspotDisconnector() = default;

FakeWifiHotspotDisconnector::~FakeWifiHotspotDisconnector() = default;

void FakeWifiHotspotDisconnector::DisconnectFromWifiHotspot(
    const std::string& wifi_network_guid,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback) {
  last_disconnected_wifi_network_guid_ = wifi_network_guid;

  if (disconnection_error_name_.empty())
    std::move(success_callback).Run();
  else
    std::move(error_callback).Run(disconnection_error_name_);
}

}  // namespace tether

}  // namespace ash
