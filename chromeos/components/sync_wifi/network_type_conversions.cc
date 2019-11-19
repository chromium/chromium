// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/network_type_conversions.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

namespace {

std::string DecodeHexString(const std::string& base_16) {
  std::string decoded;
  DCHECK_EQ(base_16.size() % 2, 0u) << "Must be a multiple of 2";
  decoded.reserve(base_16.size() / 2);

  std::vector<uint8_t> v;
  if (!base::HexStringToBytes(base_16, &v)) {
    NOTREACHED();
  }
  decoded.assign(reinterpret_cast<const char*>(&v[0]), v.size());
  return decoded;
}

}  // namespace

std::string SecurityTypeStringFromMojo(
    const network_config::mojom::SecurityType& security_type) {
  switch (security_type) {
    case network_config::mojom::SecurityType::kWpaPsk:
      return shill::kSecurityPsk;
    case network_config::mojom::SecurityType::kWepPsk:
      return shill::kSecurityWep;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return "";
  }
}

std::string SecurityTypeStringFromProto(
    const sync_pb::WifiConfigurationSpecificsData_SecurityType& security_type) {
  switch (security_type) {
    case sync_pb::WifiConfigurationSpecificsData::SECURITY_TYPE_PSK:
      return shill::kSecurityPsk;
    case sync_pb::WifiConfigurationSpecificsData::SECURITY_TYPE_WEP:
      return shill::kSecurityWep;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return "";
  }
}

network_config::mojom::SecurityType MojoSecurityTypeFromProto(
    const sync_pb::WifiConfigurationSpecificsData_SecurityType& security_type) {
  switch (security_type) {
    case sync_pb::WifiConfigurationSpecificsData::SECURITY_TYPE_PSK:
      return network_config::mojom::SecurityType::kWpaPsk;
    case sync_pb::WifiConfigurationSpecificsData::SECURITY_TYPE_WEP:
      return network_config::mojom::SecurityType::kWepPsk;
    default:
      // Only PSK and WEP secured networks are supported by sync.
      NOTREACHED();
      return network_config::mojom::SecurityType::kNone;
  }
}

network_config::mojom::ConfigPropertiesPtr MojoNetworkConfigFromProto(
    const sync_pb::WifiConfigurationSpecificsData& specifics) {
  auto config = network_config::mojom::ConfigProperties::New();
  auto wifi = network_config::mojom::WiFiConfigProperties::New();

  wifi->ssid = DecodeHexString(specifics.hex_ssid());
  wifi->security = MojoSecurityTypeFromProto(specifics.security_type());
  wifi->passphrase = specifics.passphrase();

  config->type_config =
      network_config::mojom::NetworkTypeConfigProperties::NewWifi(
          std::move(wifi));

  config->auto_connect = network_config::mojom::AutoConnectConfig::New(
      specifics.automatically_connect() ==
      sync_pb::WifiConfigurationSpecificsData::AUTOMATICALLY_CONNECT_ENABLED);

  config->priority = network_config::mojom::PriorityConfig::New(
      specifics.is_preferred() ==
              sync_pb::WifiConfigurationSpecificsData::IS_PREFERRED_ENABLED
          ? 1
          : 0);

  return config;
}

}  // namespace sync_wifi

}  // namespace chromeos
