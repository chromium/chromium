// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/proto_test_util.h"

namespace ash {

namespace tether {

DeviceStatus CreateTestDeviceStatus(const std::string& cell_provider_name,
                                    int battery_percentage,
                                    int connection_strength) {
  // TODO(khorimoto): Once a ConnectedWifiSsid field is added as a property of
  // Tether networks, give an option to pass a parameter for that field as well.
  WifiStatus wifi_status;
  wifi_status.set_status_code(
      WifiStatus_StatusCode::WifiStatus_StatusCode_CONNECTED);
  wifi_status.set_ssid("WifiSsid");

  DeviceStatus device_status;
  if (battery_percentage != proto_test_util::kDoNotSetIntField) {
    device_status.set_battery_percentage(battery_percentage);
  }
  if (cell_provider_name != proto_test_util::kDoNotSetStringField) {
    device_status.set_cell_provider(cell_provider_name);
  }
  if (connection_strength != proto_test_util::kDoNotSetIntField) {
    device_status.set_connection_strength(connection_strength);
  }

  device_status.mutable_wifi_status()->CopyFrom(wifi_status);

  return device_status;
}

DeviceStatus CreateDeviceStatusWithFakeFields() {
  return CreateTestDeviceStatus("Google Fi", 75 /* battery_percentage */,
                                4 /* connection_strength */);
}

}  // namespace tether

}  // namespace ash
