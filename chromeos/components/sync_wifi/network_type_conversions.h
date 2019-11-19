// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_TYPE_CONVERSIONS_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_TYPE_CONVERSIONS_H_

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace chromeos {

namespace sync_wifi {

std::string SecurityTypeStringFromMojo(
    const network_config::mojom::SecurityType& security_type);

std::string SecurityTypeStringFromProto(
    const sync_pb::WifiConfigurationSpecificsData_SecurityType& security_type);

network_config::mojom::SecurityType MojoSecurityTypeFromString(
    const std::string& security_type);

network_config::mojom::SecurityType MojoSecurityTypeFromProto(
    const sync_pb::WifiConfigurationSpecificsData_SecurityType& security_type);

network_config::mojom::ConfigPropertiesPtr MojoNetworkConfigFromProto(
    const sync_pb::WifiConfigurationSpecificsData& specifics);

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_TYPE_CONVERSIONS_H_
