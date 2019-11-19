// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/geolocation_handler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

constexpr const char* kDevicePropertyNames[] = {
    shill::kGeoWifiAccessPointsProperty, shill::kGeoCellTowersProperty};

std::string HexToDecimal(std::string hex_str) {
  return std::to_string(std::stoi(hex_str, nullptr, 16));
}

}  // namespace

GeolocationHandler::GeolocationHandler()
    : cellular_enabled_(false), wifi_enabled_(false) {}

GeolocationHandler::~GeolocationHandler() {
  if (ShillManagerClient::Get())
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
}

void GeolocationHandler::Init() {
  ShillManagerClient::Get()->GetProperties(
      base::Bind(&GeolocationHandler::ManagerPropertiesCallback,
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
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  const base::Value* value = nullptr;
  if (properties.Get(shill::kEnabledTechnologiesProperty, &value) && value)
    HandlePropertyChanged(shill::kEnabledTechnologiesProperty, *value);
}

void GeolocationHandler::HandlePropertyChanged(const std::string& key,
                                               const base::Value& value) {
  if (key != shill::kEnabledTechnologiesProperty)
    return;
  const base::ListValue* technologies = nullptr;
  if (!value.GetAsList(&technologies) || !technologies)
    return;
  bool wifi_was_enabled = wifi_enabled_;
  bool cellular_was_enabled = cellular_enabled_;
  cellular_enabled_ = false;
  wifi_enabled_ = false;
  for (base::ListValue::const_iterator iter = technologies->begin();
       iter != technologies->end(); ++iter) {
    std::string technology;
    iter->GetAsString(&technology);
    if (technology == shill::kTypeWifi) {
      wifi_enabled_ = true;
    } else if (technology == shill::kTypeCellular) {
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
      base::Bind(&GeolocationHandler::GeolocationCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GeolocationHandler::GeolocationCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get Geolocation data: " << call_status;
    return;
  }
  wifi_access_points_.clear();
  cell_towers_.clear();
  if (properties.empty())
    return;  // No enabled devices, don't update received time.

  // Dictionary<device_type, entry_list>
  // Example dict returned from shill:
  // {
  //   kGeoWifiAccessPointsProperty: [ {kGeoMacAddressProperty: mac_value, ...},
  //                                   ...
  //                                 ],
  //   kGeoCellTowersProperty: [ {kGeoCellIdProperty: cell_id_value, ...}, ... ]
  // }
  for (auto* device_type : kDevicePropertyNames) {
    if (!properties.HasKey(device_type)) {
      continue;
    }

    const base::ListValue* entry_list = nullptr;
    if (!properties.GetList(device_type, &entry_list)) {
      LOG(WARNING) << "Geolocation dictionary value not a List: "
                   << device_type;
      continue;
    }

    // List[Dictionary<key, value_str>]
    for (size_t i = 0; i < entry_list->GetSize(); ++i) {
      const base::DictionaryValue* entry = nullptr;
      if (!entry_list->GetDictionary(i, &entry) || !entry) {
        LOG(WARNING) << "Geolocation list value not a Dictionary: " << i;
        continue;
      }
      if (device_type == shill::kGeoWifiAccessPointsProperty) {
        AddAccessPointFromDict(entry);
      } else if (device_type == shill::kGeoCellTowersProperty) {
        AddCellTowerFromDict(entry);
      }
    }
  }
  geolocation_received_time_ = base::Time::Now();
}

void GeolocationHandler::AddAccessPointFromDict(
    const base::DictionaryValue* entry) {
  // Docs: developers.google.com/maps/documentation/business/geolocation
  WifiAccessPoint wap;

  std::string age_str;
  if (entry->GetString(shill::kGeoAgeProperty, &age_str)) {
    int64_t age_ms;
    if (base::StringToInt64(age_str, &age_ms)) {
      wap.timestamp =
          base::Time::Now() - base::TimeDelta::FromMilliseconds(age_ms);
    }
  }
  entry->GetString(shill::kGeoMacAddressProperty, &wap.mac_address);

  std::string strength_str;
  if (entry->GetString(shill::kGeoSignalStrengthProperty, &strength_str))
    base::StringToInt(strength_str, &wap.signal_strength);

  std::string signal_str;
  if (entry->GetString(shill::kGeoSignalToNoiseRatioProperty, &signal_str)) {
    base::StringToInt(signal_str, &wap.signal_to_noise);
  }

  std::string channel_str;
  if (entry->GetString(shill::kGeoChannelProperty, &channel_str))
    base::StringToInt(channel_str, &wap.channel);

  wifi_access_points_.push_back(wap);
}

void GeolocationHandler::AddCellTowerFromDict(
    const base::DictionaryValue* entry) {
  // Docs: developers.google.com/maps/documentation/business/geolocation

  // Create object.
  CellTower ct;

  // Read time fields into object.
  std::string age_str;
  if (entry->GetString(shill::kGeoAgeProperty, &age_str)) {
    int64_t age_ms;
    if (base::StringToInt64(age_str, &age_ms)) {
      ct.timestamp =
          base::Time::Now() - base::TimeDelta::FromMilliseconds(age_ms);
    }
  }

  // Read hex fields into object.
  std::string hex_cell_id;
  if (entry->GetString(shill::kGeoCellIdProperty, &hex_cell_id)) {
    ct.ci = HexToDecimal(hex_cell_id);
  }

  std::string hex_lac;
  if (entry->GetString(shill::kGeoLocationAreaCodeProperty, &hex_lac)) {
    ct.lac = HexToDecimal(hex_lac);
  }

  // Read decimal fields into object.
  entry->GetString(shill::kGeoMobileCountryCodeProperty, &ct.mcc);
  entry->GetString(shill::kGeoMobileNetworkCodeProperty, &ct.mnc);

  // Add new object to vector.
  cell_towers_.push_back(ct);
}

}  // namespace chromeos
