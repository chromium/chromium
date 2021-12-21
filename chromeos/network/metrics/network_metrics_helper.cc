// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/network_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/network/metrics/shill_connect_result.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char kNetworkMetricsPrefix[] = "Network.Ash.";
const char kAllConnectionResultSuffix[] = ".ConnectionResult.All";

const char kWifi[] = "WiFi";
const char kWifiOpen[] = "WiFi.SecurityOpen";
const char kWifiPasswordProtected[] = "WiFi.SecurityPasswordProtected";

const char kTether[] = "Tether";

chromeos::NetworkStateHandler* GetNetworkStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

const std::vector<std::string> GetCellularNetworkTypeHistogams(
    const NetworkState* network_state) {
  const std::string kCellularPrefix = "Cellular";
  const std::string kESimInfix = ".ESim";
  const std::string kPSimInfix = ".PSim";

  std::vector<std::string> cellular_histograms{kCellularPrefix};

  if (network_state->eid().empty())
    cellular_histograms.emplace_back(kCellularPrefix + kPSimInfix);
  else
    cellular_histograms.emplace_back(kCellularPrefix + kESimInfix);
  return cellular_histograms;
}

const std::vector<std::string> GetEthernetNetworkTypeHistograms(
    const NetworkState* network_state) {
  const std::string kEthernetPrefix = "Ethernet";
  const std::string kEapInfix = ".Eap";
  const std::string kNoEapInfix = ".NoEap";

  std::vector<std::string> ethernet_histograms{kEthernetPrefix};
  if (GetNetworkStateHandler()->GetEAPForEthernet(network_state->path(),
                                                  /*connected_only=*/true)) {
    ethernet_histograms.emplace_back(kEthernetPrefix + kEapInfix);
  } else {
    ethernet_histograms.emplace_back(kEthernetPrefix + kNoEapInfix);
  }

  return ethernet_histograms;
}

const std::vector<std::string> GetWifiNetworkTypeHistograms(
    const NetworkState* network_state) {
  std::vector<std::string> wifi_histograms{kWifi};

  if (network_state->GetMojoSecurity() ==
      network_config::mojom::SecurityType::kNone) {
    wifi_histograms.emplace_back(kWifiOpen);
  } else {
    wifi_histograms.emplace_back(kWifiPasswordProtected);
  }

  return wifi_histograms;
}

const std::vector<std::string> GetTetherNetworkTypeHistograms(
    const NetworkState* network_state) {
  return {kTether};
}

const std::vector<std::string> GetVpnNetworkTypeHistograms(
    const NetworkState* network_state) {
  const std::string kVpnPrefix = "VPN";
  const std::string kBuiltInInfix = ".TypeBuiltIn";
  const std::string kThirdPartyInfix = ".TypeThirdParty";

  const std::string& vpn_provider_type = network_state->GetVpnProviderType();

  if (vpn_provider_type.empty())
    return {};

  std::vector<std::string> vpn_histograms{kVpnPrefix};

  if (vpn_provider_type == shill::kProviderThirdPartyVpn ||
      vpn_provider_type == shill::kProviderArcVpn) {
    vpn_histograms.emplace_back(kVpnPrefix + kThirdPartyInfix);
  } else if (vpn_provider_type == shill::kProviderL2tpIpsec ||
             vpn_provider_type == shill::kProviderOpenVpn ||
             vpn_provider_type == shill::kProviderWireGuard) {
    vpn_histograms.emplace_back(kVpnPrefix + kBuiltInInfix);
  } else {
    NOTREACHED();
  }
  return vpn_histograms;
}

const std::vector<std::string> GetNetworkTypeHistogramNames(
    const NetworkState* network_state) {
  switch (network_state->GetNetworkTechnologyType()) {
    case NetworkState::NetworkTechnologyType::kCellular:
      return GetCellularNetworkTypeHistogams(network_state);
    case NetworkState::NetworkTechnologyType::kEthernet:
      return GetEthernetNetworkTypeHistograms(network_state);
    case NetworkState::NetworkTechnologyType::kWiFi:
      return GetWifiNetworkTypeHistograms(network_state);
    case NetworkState::NetworkTechnologyType::kTether:
      return GetTetherNetworkTypeHistograms(network_state);
    case NetworkState::NetworkTechnologyType::kVPN:
      return GetVpnNetworkTypeHistograms(network_state);

    // There should not be connections requests for kEthernetEap type service.
    // kEthernetEap exists only to store auth details for ethernet.
    case NetworkState::NetworkTechnologyType::kEthernetEap:
      FALLTHROUGH;
    case NetworkState::NetworkTechnologyType::kUnknown:
      FALLTHROUGH;
    default:
      return {};
  }
}

}  // namespace

// static
void NetworkMetricsHelper::LogAllConnectionResult(
    const std::string& guid,
    const absl::optional<std::string>& shill_error) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);

  ShillConnectResult connect_result =
      shill_error.has_value() ? ShillErrorToConnectResult(*shill_error)
                              : ShillConnectResult::kSuccess;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(
        kNetworkMetricsPrefix + network_type + kAllConnectionResultSuffix,
        connect_result);
  }
}

NetworkMetricsHelper::NetworkMetricsHelper() = default;

NetworkMetricsHelper::~NetworkMetricsHelper() = default;

}  // namespace chromeos
