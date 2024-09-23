// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/vpn_network_metrics_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

// The buckets of the histogram that captures the metrics of the configuration
// sources of created VPNs.
const char kVpnConfigurationSourceBucketArc[] =
    "Network.Ash.VPN.ARC.ConfigurationSource";
const char kVpnConfigurationSourceBucketIKEv2[] =
    "Network.Ash.VPN.IKEv2.ConfigurationSource";
const char kVpnConfigurationSourceBucketL2tpIpsec[] =
    "Network.Ash.VPN.L2TPIPsec.ConfigurationSource";
const char kVpnConfigurationSourceBucketOpenVpn[] =
    "Network.Ash.VPN.OpenVPN.ConfigurationSource";
const char kVpnConfigurationSourceBucketThirdParty[] =
    "Network.Ash.VPN.ThirdParty.ConfigurationSource";
const char kVpnConfigurationSourceBucketWireGuard[] =
    "Network.Ash.VPN.WireGuard.ConfigurationSource";
const char kVpnConfigurationSourceBucketUnknown[] =
    "Network.Ash.VPN.Unknown.ConfigurationSource";

const char* GetBucketForVpnProviderType(const std::string& vpn_provider_type) {
  if (vpn_provider_type == shill::kProviderArcVpn) {
    return kVpnConfigurationSourceBucketArc;
  } else if (vpn_provider_type == shill::kProviderIKEv2) {
    return kVpnConfigurationSourceBucketIKEv2;
  } else if (vpn_provider_type == shill::kProviderL2tpIpsec) {
    return kVpnConfigurationSourceBucketL2tpIpsec;
  } else if (vpn_provider_type == shill::kProviderOpenVpn) {
    return kVpnConfigurationSourceBucketOpenVpn;
  } else if (vpn_provider_type == shill::kProviderThirdPartyVpn) {
    return kVpnConfigurationSourceBucketThirdParty;
  } else if (vpn_provider_type == shill::kProviderWireGuard) {
    return kVpnConfigurationSourceBucketWireGuard;
  }
  DUMP_WILL_BE_NOTREACHED();
  return kVpnConfigurationSourceBucketUnknown;
}

}  // namespace

VpnNetworkMetricsHelper::VpnNetworkMetricsHelper() = default;

VpnNetworkMetricsHelper::~VpnNetworkMetricsHelper() = default;

void VpnNetworkMetricsHelper::Init(
    NetworkConfigurationHandler* network_configuration_handler) {
  if (network_configuration_handler)
    network_configuration_observation_.Observe(network_configuration_handler);
}

void VpnNetworkMetricsHelper::OnConfigurationCreated(
    const std::string& service_path,
    const std::string& guid) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);

  if (!network_state || network_state->GetNetworkTechnologyType() !=
                            NetworkState::NetworkTechnologyType::kVPN) {
    return;
  }

  const char* vpn_provider_type_bucket =
      GetBucketForVpnProviderType(network_state->GetVpnProviderType());

  DCHECK(vpn_provider_type_bucket);

  base::UmaHistogramEnumeration(
      vpn_provider_type_bucket,
      network_state->IsManagedByPolicy()
          ? VPNConfigurationSource::kConfiguredByPolicy
          : VPNConfigurationSource::kConfiguredManually);
}

}  // namespace ash
