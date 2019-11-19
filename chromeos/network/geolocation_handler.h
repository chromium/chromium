// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_GEOLOCATION_HANDLER_H_
#define CHROMEOS_NETWORK_GEOLOCATION_HANDLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_util.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// This class provices Shill Wifi Access Point and Cell Tower data. It
// currently relies on polling because that is the usage model in
// content::WifiDataProvider. This class requests data asynchronously,
// returning the most recent available data. A typical usage pattern,
// assuming a wifi device is enabled, is:
//   Initialize();  // Makes an initial request
//   GetWifiAccessPoints();  // returns true + inital data, requests update
//   (Delay some amount of time, ~10s)
//   GetWifiAccessPoints();  // returns true + updated data, requests update
//   (Delay some amount of time after data changed, ~10s)
//   GetWifiAccessPoints();  // returns true + same data, requests update
//   (Delay some amount of time after data did not change, ~2 mins)

class COMPONENT_EXPORT(CHROMEOS_NETWORK) GeolocationHandler
    : public ShillPropertyChangedObserver {
 public:
  ~GeolocationHandler() override;

  // This sends a request for geolocation (both wifi AP and cell tower) data.
  // If AP data is already available, fills |access_points| with the latest
  // access point data, and similarly for cell tower data and |cell_towers|.
  // Returns |true| if either type of data is already available upon call.
  bool GetNetworkInformation(WifiAccessPointVector* access_points,
                             CellTowerVector* cell_towers);

  // This sends a request for geolocation (both wifi AP and cell tower) data.
  // If wifi data is already available, returns |true|, fills |access_points|
  // with the latest access point data, and sets |age_ms| to the time
  // since the last update in MS.
  bool GetWifiAccessPoints(WifiAccessPointVector* access_points,
                           int64_t* age_ms);

  bool wifi_enabled() const { return wifi_enabled_; }

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

 private:
  friend class NetworkHandler;
  friend class GeolocationHandlerTest;
  friend class SimpleGeolocationWirelessTest;

  GeolocationHandler();

  void Init();

  // ShillManagerClient callback
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);

  // Called from OnPropertyChanged or ManagerPropertiesCallback.
  void HandlePropertyChanged(const std::string& key, const base::Value& value);

  // Asynchronously request geolocation objects (wifi access points and
  // cell towers) from Shill.Manager.
  void RequestGeolocationObjects();

  // Callback for receiving Geolocation data.
  void GeolocationCallback(DBusMethodCallStatus call_status,
                           const base::DictionaryValue& properties);

  bool cellular_enabled_;
  bool wifi_enabled_;

  void AddCellTowerFromDict(const base::DictionaryValue* entry);
  void AddAccessPointFromDict(const base::DictionaryValue* entry);

  // Cached netork information and update time
  WifiAccessPointVector wifi_access_points_;
  CellTowerVector cell_towers_;
  base::Time geolocation_received_time_;

  // For Shill client callbacks
  base::WeakPtrFactory<GeolocationHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GeolocationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_GEOLOCATION_HANDLER_H_
