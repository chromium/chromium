// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/geolocation_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

constexpr const char* kDevicePropertyNames[] = {
    shill::kGeoWifiAccessPointsProperty, shill::kGeoCellTowersProperty};

std::string HexToDecimal(std::string hex_str) {
  int result;
  if (!base::HexStringToInt(hex_str, &result))
    return std::string();
  return base::NumberToString(result);
}

std::string FindStringOrEmpty(const base::Value::Dict& dict,
                              std::string_view key) {
  const std::string* val = dict.FindString(key);
  return val ? *val : std::string();
}

}  // namespace

GeolocationHandler::GeolocationHandler() = default;

GeolocationHandler::~GeolocationHandler() {
  if (ShillManagerClient::Get())
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
}

void GeolocationHandler::Init() {
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&GeolocationHandler::ManagerPropertiesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
}

bool GeolocationHandler::GetWifiAccessPoints(
    WifiAccessPointVector* access_points,
    int64_t* age_ms) {
  if (!wifi_enabled_)
    return false;
  // Always request updated info.
  RequestGeolocationObjects();
  // If no data has been received, return false.
  if (geolocation_received_time_.is_null() || wifi_access_points_.size() == 0)
    return false;
  if (access_points)
    *access_points = wifi_access_points_;
  if (age_ms) {
    base::TimeDelta dtime = base::Time::Now() - geolocation_received_time_;
    *age_ms = dtime.InMilliseconds();
  }
  return true;
}

bool GeolocationHandler::GetNetworkInformation(
    WifiAccessPointVector* access_points,
    CellTowerVector* cell_towers) {
  if (!cellular_enabled_ && !wifi_enabled_)
    return false;

  // Always request updated info.
  RequestGeolocationObjects();

  // If no data has been received, return false.
  if (geolocation_received_time_.is_null())
    return false;

  if (cell_towers)
    *cell_towers = cell_towers_;
  if (access_points)
    *access_points = wifi_access_points_;

  return true;
}

void GeolocationHandler::OnPropertyChanged(const std::string& key,
                                           const base::Value& value) {
  HandlePropertyChanged(key, value);
}

//------------------------------------------------------------------------------
// Private methods

void GeolocationHandler::ManagerPropertiesCallback(
    std::optional<base::Value::Dict> properties) {
  if (!properties)
    return;

  const base::Value* value =
      properties->Find(shill::kEnabledTechnologiesProperty);
  if (value)
    HandlePropertyChanged(shill::kEnabledTechnologiesProperty, *value);
}

void GeolocationHandler::HandlePropertyChanged(const std::string& key,
                                               const base::Value& value) {
  if (key != shill::kEnabledTechnologiesProperty)
    return;
  if (!value.is_list())
    return;
  bool wifi_was_enabled = wifi_enabled_;
  bool cellular_was_enabled = cellular_enabled_;
  cellular_enabled_ = false;
  wifi_enabled_ = false;
  for (const auto& entry : value.GetList()) {
    const std::string* technology = entry.GetIfString();
    if (technology && *technology == shill::kTypeWifi) {
      wifi_enabled_ = true;
    } else if (technology && *technology == shill::kTypeCellular) {
      cellular_enabled_ = true;
    }
    if (wifi_enabled_ && cellular_enabled_)
      break;
  }

  // Request initial location data.
  if ((!wifi_was_enabled && wifi_enabled_) ||
      (!cellular_was_enabled && cellular_enabled_)) {
    RequestGeolocationObjects();
  }
}

void GeolocationHandler::RequestGeolocationObjects() {
  ShillManagerClient::Get()->GetNetworksForGeolocation(
      base::BindOnce(&GeolocationHandler::GeolocationCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GeolocationHandler::GeolocationCallback(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    LOG(ERROR) << "Failed to get Geolocation data";
    return;
  }
  wifi_access_points_.clear();
  cell_towers_.clear();
  if (properties->empty()) {
    return;  // No enabled devices, don't update received time.
  }

  // Dictionary<device_type, entry_list>
  // Example dict returned from shill:
  // {
  //   kGeoWifiAccessPointsProperty: [ {kGeoMacAddressProperty: mac_value, ...},
  //                                   ...
  //                                 ],
  //   kGeoCellTowersProperty: [ {kGeoCellIdProperty: cell_id_value, ...}, ... ]
  // }
  for (auto* device_type : kDevicePropertyNames) {
    const base::Value::List* entry_list = properties->FindList(device_type);
    if (!entry_list) {
      if (properties->contains(device_type)) {
        LOG(WARNING) << "Geolocation dictionary value not a List: "
                     << device_type;
      }
      continue;
    }

    // List[Dictionary<key, value_str>]
    for (const auto& entry : *entry_list) {
      if (!entry.is_dict()) {
        LOG(WARNING) << "Geolocation list value not a Dictionary";
        continue;
      }
      if (device_type == shill::kGeoWifiAccessPointsProperty) {
        AddAccessPointFromDict(entry.GetDict());
      } else if (device_type == shill::kGeoCellTowersProperty) {
        AddCellTowerFromDict(entry.GetDict());
      }
    }
  }
  geolocation_received_time_ = base::Time::Now();
}

void GeolocationHandler::AddAccessPointFromDict(
    const base::Value::Dict& entry) {
  // Docs: developers.google.com/maps/documentation/business/geolocation
  WifiAccessPoint wap;

  const std::string* age_str = entry.FindString(shill::kGeoAgeProperty);
  if (age_str) {
    int64_t age_seconds;
    if (base::StringToInt64(*age_str, &age_seconds)) {
      wap.timestamp = base::Time::Now() - base::Seconds(age_seconds);
    }
  }

  wap.mac_address = FindStringOrEmpty(entry, shill::kGeoMacAddressProperty);

  const std::string* strength_str =
      entry.FindString(shill::kGeoSignalStrengthProperty);
  if (strength_str) {
    base::StringToInt(*strength_str, &wap.signal_strength);
  }

  const std::string* signal_str =
      entry.FindString(shill::kGeoSignalToNoiseRatioProperty);
  if (signal_str) {
    base::StringToInt(*signal_str, &wap.signal_to_noise);
  }

  const std::string* channel_str = entry.FindString(shill::kGeoChannelProperty);
  if (channel_str) {
    base::StringToInt(*channel_str, &wap.channel);
  }

  wifi_access_points_.push_back(wap);
}

void GeolocationHandler::AddCellTowerFromDict(const base::Value::Dict& entry) {
  // Docs: developers.google.com/maps/documentation/business/geolocation

  // Create object.
  CellTower ct;

  // Read time fields into object.
  const std::string* age_str = entry.FindString(shill::kGeoAgeProperty);
  if (age_str) {
    int64_t age_seconds;
    if (base::StringToInt64(*age_str, &age_seconds)) {
      ct.timestamp = base::Time::Now() - base::Seconds(age_seconds);
    }
  }

  // Read hex fields into object.
  const std::string* hex_cell_id = entry.FindString(shill::kGeoCellIdProperty);
  if (hex_cell_id) {
    ct.ci = HexToDecimal(*hex_cell_id);
  }

  const std::string* hex_lac =
      entry.FindString(shill::kGeoLocationAreaCodeProperty);
  if (hex_lac) {
    ct.lac = HexToDecimal(*hex_lac);
  }

  // Read decimal fields into object.
  ct.mcc = FindStringOrEmpty(entry, shill::kGeoMobileCountryCodeProperty);
  ct.mnc = FindStringOrEmpty(entry, shill::kGeoMobileNetworkCodeProperty);

  // Add new object to vector.
  cell_towers_.push_back(ct);
}

}  // namespace ash
