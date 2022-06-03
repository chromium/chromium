// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_TYPE_CONVERSIONS_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_TYPE_CONVERSIONS_H_

#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace chromeos {

class NetworkState;

namespace sync_wifi {

std::string DecodeHexString(const std::string& base_16);

std::string SecurityTypeStringFromMojo(
    const network_config::mojom::SecurityType& security_type);

std::string SecurityTypeStringFromProto(
    const sync_pb::WifiConfigurationSpecifics_SecurityType& security_type);

sync_pb::WifiConfigurationSpecifics_SecurityType SecurityTypeProtoFromMojo(
    const network_config::mojom::SecurityType& security_type);

sync_pb::WifiConfigurationSpecifics_AutomaticallyConnectOption
AutomaticallyConnectProtoFromMojo(
    const network_config::mojom::ManagedBooleanPtr& auto_connect);

sync_pb::WifiConfigurationSpecifics_IsPreferredOption IsPreferredProtoFromMojo(
    const network_config::mojom::ManagedInt32Ptr& is_preferred);

sync_pb::WifiConfigurationSpecifics_ProxyConfiguration_ProxyOption
ProxyOptionProtoFromMojo(
    const network_config::mojom::ManagedProxySettingsPtr& proxy_settings,
    bool is_unspecified);

sync_pb::WifiConfigurationSpecifics_ProxyConfiguration
ProxyConfigurationProtoFromMojo(
    const network_config::mojom::ManagedProxySettingsPtr& proxy_settings,
    bool is_unspecified);

network_config::mojom::SecurityType MojoSecurityTypeFromString(
    const std::string& security_type);

network_config::mojom::SecurityType MojoSecurityTypeFromProto(
    const sync_pb::WifiConfigurationSpecifics_SecurityType& security_type);

network_config::mojom::ProxySettingsPtr MojoProxySettingsFromProto(
    const sync_pb::WifiConfigurationSpecifics_ProxyConfiguration& specifics);

network_config::mojom::ConfigPropertiesPtr MojoNetworkConfigFromProto(
    const sync_pb::WifiConfigurationSpecifics& specifics);

const NetworkState* NetworkStateFromNetworkIdentifier(
    const NetworkIdentifier& id);

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_TYPE_CONVERSIONS_H_
