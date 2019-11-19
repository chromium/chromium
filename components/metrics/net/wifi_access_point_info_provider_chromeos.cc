// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/wifi_access_point_info_provider_chromeos.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkHandler;

namespace metrics {

WifiAccessPointInfoProviderChromeos::WifiAccessPointInfoProviderChromeos() {
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);

  // Update initial connection state.
  DefaultNetworkChanged(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
}

WifiAccessPointInfoProviderChromeos::~WifiAccessPointInfoProviderChromeos() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
}

bool WifiAccessPointInfoProviderChromeos::GetInfo(WifiAccessPointInfo* info) {
  // Wifi access point information is not provided if the BSSID is empty.
  // This assumes the BSSID is never empty when access point information exists.
  if (wifi_access_point_info_.bssid.empty())
    return false;

  *info = wifi_access_point_info_;
  return true;
}

void WifiAccessPointInfoProviderChromeos::DefaultNetworkChanged(
    const chromeos::NetworkState* default_network) {
  // Reset access point info to prevent reporting of out-dated data.
  wifi_access_point_info_ = WifiAccessPointInfo();

  // Skip non-wifi connections
  if (!default_network || default_network->type() != shill::kTypeWifi)
    return;

  // Retrieve access point info for wifi connection.
  NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
      default_network->path(),
      base::Bind(&WifiAccessPointInfoProviderChromeos::ParseInfo, AsWeakPtr()),
      chromeos::network_handler::ErrorCallback());
}

void WifiAccessPointInfoProviderChromeos::ParseInfo(
    const std::string &service_path,
    const base::DictionaryValue& properties) {
  // Skip services that contain "_nomap" in the SSID.
  std::string ssid = chromeos::shill_property_util::GetSSIDFromProperties(
      properties, false /* verbose_logging */, nullptr);
  if (ssid.find("_nomap", 0) != std::string::npos)
    return;

  std::string bssid;
  if (!properties.GetStringWithoutPathExpansion(shill::kWifiBSsid, &bssid) ||
      bssid.empty())
    return;

  // Filter out BSSID with local bit set in the first byte.
  uint32_t first_octet;
  if (!base::HexStringToUInt(bssid.substr(0, 2), &first_octet))
    NOTREACHED();
  if (first_octet & 0x2)
    return;
  wifi_access_point_info_.bssid = bssid;

  // Parse security info.
  std::string security;
  properties.GetStringWithoutPathExpansion(
      shill::kSecurityProperty, &security);
  wifi_access_point_info_.security = WIFI_SECURITY_UNKNOWN;
  if (security == shill::kSecurityWpa)
    wifi_access_point_info_.security = WIFI_SECURITY_WPA;
  else if (security == shill::kSecurityWep)
    wifi_access_point_info_.security = WIFI_SECURITY_WEP;
  else if (security == shill::kSecurityRsn)
    wifi_access_point_info_.security = WIFI_SECURITY_RSN;
  else if (security == shill::kSecurity8021x)
    wifi_access_point_info_.security = WIFI_SECURITY_802_1X;
  else if (security == shill::kSecurityPsk)
    wifi_access_point_info_.security = WIFI_SECURITY_PSK;
  else if (security == shill::kSecurityNone)
    wifi_access_point_info_.security = WIFI_SECURITY_NONE;

  properties.GetStringWithoutPathExpansion(
      shill::kWifiBSsid, &wifi_access_point_info_.bssid);
  const base::DictionaryValue* vendor_dict = nullptr;
  if (!properties.GetDictionaryWithoutPathExpansion(
          shill::kWifiVendorInformationProperty,
          &vendor_dict))
    return;

  vendor_dict->GetStringWithoutPathExpansion(
      shill::kVendorWPSModelNumberProperty,
      &wifi_access_point_info_.model_number);
  vendor_dict->GetStringWithoutPathExpansion(
      shill::kVendorWPSModelNameProperty,
      &wifi_access_point_info_.model_name);
  vendor_dict->GetStringWithoutPathExpansion(
      shill::kVendorWPSDeviceNameProperty,
      &wifi_access_point_info_.device_name);
  vendor_dict->GetStringWithoutPathExpansion(shill::kVendorOUIListProperty,
                                             &wifi_access_point_info_.oui_list);
}

}  // namespace metrics
