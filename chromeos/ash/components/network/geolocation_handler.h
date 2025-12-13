// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_GEOLOCATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_GEOLOCATION_HANDLER_H_

#include "chromeos/ash/components/network/network_util.h"

namespace ash {

// Delegate for chromeos/ash/components/geolocation/location_fetcher.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) GeolocationHandler {
 public:
  virtual ~GeolocationHandler() = default;

  // This sends a request for geolocation (both wifi AP and cell tower) data.
  // If AP data is already available, fills `access_points` with the latest
  // access point data, and similarly for cell tower data and `cell_towers`.
  // Returns `true` if either type of data is already available upon call.
  virtual bool GetNetworkInformation(WifiAccessPointVector* access_points,
                                     CellTowerVector* cell_towers) = 0;

  // This sends a request for geolocation (both wifi AP and cell tower) data.
  // If wifi data is already available, returns |true|, fills |access_points|
  // with the latest access point data, and sets |age_ms| to the time
  // since the last update in MS.
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* access_points,
                                   int64_t* age_ms) = 0;

  virtual bool wifi_enabled() const = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_GEOLOCATION_HANDLER_H_
