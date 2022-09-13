// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_DEVICE_CONDITIONS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_DEVICE_CONDITIONS_H_

#include "net/base/network_change_notifier.h"

namespace offline_pages {

// Device network and power conditions.
class DeviceConditions {
 public:
  DeviceConditions(
      bool power_connected,
      int battery_percentage,
      net::NetworkChangeNotifier::ConnectionType net_connection_type)
      : power_connected_(power_connected),
        battery_percentage_(battery_percentage),
        net_connection_type_(net_connection_type) {}

  DeviceConditions()
      : power_connected_(true),
        battery_percentage_(75),
        net_connection_type_(net::NetworkChangeNotifier::CONNECTION_WIFI) {}

  // Returns whether power is connected.
  bool IsPowerConnected() const { return power_connected_; }

  // Returns percentage of remaining battery power (0-100).
  int GetBatteryPercentage() const { return battery_percentage_; }

  // Returns the current type of network connection, if any.
  net::NetworkChangeNotifier::ConnectionType GetNetConnectionType() const {
    return net_connection_type_;
  }

 private:
  const bool power_connected_;
  const int battery_percentage_;
  const net::NetworkChangeNotifier::ConnectionType net_connection_type_;

  // NOTE: We intentionally allow the default copy constructor and assignment.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_DEVICE_CONDITIONS_H_
