// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kNetworkMetricsPrefix[] = "Network.Ash.";
const char kAllConnectionResultSuffix[] = ".ConnectionResult.All";
const char kFilteredConnectionResultSuffix[] = ".ConnectionResult.Filtered";
const char kNonUserInitiatedConnectionResultSuffix[] =
    ".ConnectionResult.NonUserInitiated";
const char kUserInitiatedConnectionResultSuffix[] =
    ".ConnectionResult.UserInitiated";
const char kDisconnectionsWithoutUserActionSuffix[] =
    ".DisconnectionsWithoutUserAction";
const char kDisconnectionsWithoutUserActionShillErrorSuffix[] =
    ".DisconnectionsWithoutUserAction.ShillError";

const char kEnableTechnologyResultSuffix[] = ".EnabledState.Enable.Result";
const char kEnableTechnologyResultCodeSuffix[] =
    ".EnabledState.Enable.ResultCode";
const char kDisableTechnologyResultSuffix[] = ".EnabledState.Disable.Result";
const char kDisableTechnologyResultCodeSuffix[] =
    ".EnabledState.Disable.ResultCode";

const char kCellular[] = "Cellular";
const char kCellularESim[] = "Cellular.ESim";
const char kCellularPSim[] = "Cellular.PSim";
const char kPolicySuffix[] = ".Policy";

const char kEthernet[] = "Ethernet";
const char kEthernetEap[] = "Ethernet.Eap";
const char kEthernetNoEap[] = "Ethernet.NoEap";

const char kTether[] = "Tether";

const char kVPN[] = "VPN";
const char kVPNBuiltIn[] = "VPN.TypeBuiltIn";
const char kVPNThirdParty[] = "VPN.TypeThirdParty";
const char kVPNUnknown[] = "VPN.TypeUnknown";

const char kWifi[] = "WiFi";
const char kWifiOpen[] = "WiFi.SecurityOpen";
const char kWifiPasswordProtected[] = "WiFi.SecurityPasswordProtected";

NetworkStateHandler* GetNetworkStateHandler() {
  return NetworkHandler::Get()->network_state_handler();
}

const std::optional<const std::string> GetTechnologyTypeSuffix(
    const std::string& technology) {
  // Note that Tether is a fake technology that does not correspond to shill
  // technology type.
  if (technology == shill::kTypeWifi)
    return kWifi;
  else if (technology == shill::kTypeEthernet)
    return kEthernet;
  else if (technology == shill::kTypeCellular)
    return kCellular;
  else if (technology == shill::kTypeVPN)
    return kVPN;
  return std::nullopt;
}

const std::vector<std::string> GetCellularNetworkTypeHistogams(
    const NetworkState* network_state) {
  std::vector<std::string> cellular_histograms{kCellular};

  const char* cellular_sim_type =
      network_state->eid().empty() ? kCellularPSim : kCellularESim;
  cellular_histograms.emplace_back(cellular_sim_type);
  if (network_state->IsManagedByPolicy()) {
    cellular_histograms.emplace_back(
        base::StrCat({cellular_sim_type, kPolicySuffix}));
  }
  return cellular_histograms;
}

const std::vector<std::string> GetEthernetNetworkTypeHistograms(
    const NetworkState* network_state) {
  std::vector<std::string> ethernet_histograms{kEthernet};
  if (GetNetworkStateHandler()->GetEAPForEthernet(network_state->path(),
                                                  /*connected_only=*/true)) {
    ethernet_histograms.emplace_back(kEthernetEap);
  } else {
    ethernet_histograms.emplace_back(kEthernetNoEap);
  }

  return ethernet_histograms;
}

const std::vector<std::string> GetWifiNetworkTypeHistograms(
    const NetworkState* network_state) {
  std::vector<std::string> wifi_histograms{kWifi};

  if (network_state->GetMojoSecurity() ==
      chromeos::network_config::mojom::SecurityType::kNone) {
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
  const std::string& vpn_provider_type = network_state->GetVpnProviderType();

  if (vpn_provider_type.empty())
    return {};

  std::vector<std::string> vpn_histograms{kVPN};

  if (vpn_provider_type == shill::kProviderThirdPartyVpn ||
      vpn_provider_type == shill::kProviderArcVpn) {
    vpn_histograms.emplace_back(kVPNThirdParty);
  } else if (vpn_provider_type == shill::kProviderIKEv2 ||
             vpn_provider_type == shill::kProviderL2tpIpsec ||
             vpn_provider_type == shill::kProviderOpenVpn ||
             vpn_provider_type == shill::kProviderWireGuard) {
    vpn_histograms.emplace_back(kVPNBuiltIn);
  } else {
    DUMP_WILL_BE_NOTREACHED();
    vpn_histograms.emplace_back(kVPNUnknown);
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
      [[fallthrough]];
    case NetworkState::NetworkTechnologyType::kUnknown:
      [[fallthrough]];
    default:
      return {};
  }
}

}  // namespace

// static
void NetworkMetricsHelper::LogAllConnectionResult(
    const std::string& guid,
    bool is_auto_connect,
    bool is_repeated_error,
    const std::optional<std::string>& shill_error) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state)
    return;

  ShillConnectResult connect_result =
      shill_error ? ShillErrorToConnectResult(*shill_error)
                  : ShillConnectResult::kSuccess;

  const bool is_not_repeated_error =
      !is_repeated_error || connect_result == ShillConnectResult::kSuccess;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {kNetworkMetricsPrefix, network_type, kAllConnectionResultSuffix}),
        connect_result);
    if (is_auto_connect) {
      base::UmaHistogramEnumeration(
          base::StrCat({kNetworkMetricsPrefix, network_type,
                        kNonUserInitiatedConnectionResultSuffix}),
          connect_result);
    }

    if (is_not_repeated_error) {
      base::UmaHistogramEnumeration(
          base::StrCat({kNetworkMetricsPrefix, network_type,
                        kFilteredConnectionResultSuffix}),
          connect_result);
    }
  }
}

// static
void NetworkMetricsHelper::LogUserInitiatedConnectionResult(
    const std::string& guid,
    const std::optional<std::string>& network_connection_error) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state)
    return;

  UserInitiatedConnectResult connect_result =
      network_connection_error
          ? NetworkConnectionErrorToConnectResult(*network_connection_error,
                                                  network_state->GetError())
          : UserInitiatedConnectResult::kSuccess;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(
        base::StrCat({kNetworkMetricsPrefix, network_type,
                      kUserInitiatedConnectionResultSuffix}),
        connect_result);
  }
}

// static
void NetworkMetricsHelper::LogConnectionStateResult(
    const std::string& guid,
    const ConnectionState connection_state,
    const std::optional<ShillConnectResult> shill_error) {
  DCHECK(GetNetworkStateHandler());
  const NetworkState* network_state =
      GetNetworkStateHandler()->GetNetworkStateFromGuid(guid);
  if (!network_state) {
    return;
  }

  // Only when WiFi network becomes "failure" from a connected state indicates
  // there's a real disconnection without user action. If the network becomes
  // "idle" from a connected state with a shill error, it usually indicates the
  // disconnections are triggered by device suspend. See
  // go/cros-wifi-disconnection-metrics for details.
  if (network_state->GetNetworkTechnologyType() ==
          NetworkState::NetworkTechnologyType::kWiFi &&
      connection_state == ConnectionState::kDisconnectedWithoutUserAction &&
      network_state->connection_state() != shill::kStateFailure) {
    return;
  }

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(kNetworkMetricsPrefix + network_type +
                                      kDisconnectionsWithoutUserActionSuffix,
                                  connection_state);
    if (connection_state == ConnectionState::kDisconnectedWithoutUserAction) {
      DCHECK(shill_error.has_value());
      base::UmaHistogramEnumeration(
          kNetworkMetricsPrefix + network_type +
              kDisconnectionsWithoutUserActionShillErrorSuffix,
          *shill_error);
    }
  }
}

void NetworkMetricsHelper::LogEnableTechnologyResult(
    const std::string& technology,
    bool success,
    const std::optional<std::string>& shill_error) {
  std::optional<const std::string> suffix = GetTechnologyTypeSuffix(technology);

  if (!suffix)
    return;

  if (success == shill_error.has_value()) {
    if (shill_error.has_value()) {
      NET_LOG(ERROR) << "Error code: " << *shill_error
                     << " for successful enable operation on: " << technology;
    } else {
      NET_LOG(ERROR)
          << "Missing error code for unsuccessful enable operation on: "
          << technology;
    }
  }

  ShillConnectResult result = shill_error
                                  ? ShillErrorToConnectResult(*shill_error)
                                  : ShillConnectResult::kSuccess;
  base::UmaHistogramEnumeration(
      base::StrCat(
          {kNetworkMetricsPrefix, *suffix, kEnableTechnologyResultCodeSuffix}),
      result);

  base::UmaHistogramBoolean(base::StrCat({kNetworkMetricsPrefix, *suffix,
                                          kEnableTechnologyResultSuffix}),
                            success);
}

// static
void NetworkMetricsHelper::LogDisableTechnologyResult(
    const std::string& technology,
    bool success,
    const std::optional<std::string>& shill_error) {
  std::optional<const std::string> suffix = GetTechnologyTypeSuffix(technology);

  if (!suffix)
    return;

  if (success == shill_error.has_value()) {
    if (shill_error.has_value()) {
      NET_LOG(ERROR) << "Error code: " << *shill_error
                     << " for successful disable operation on: " << technology;
    } else {
      NET_LOG(ERROR)
          << "Missing error code for unsuccessful disable operation on: "
          << technology;
    }
  }

  ShillConnectResult result = shill_error
                                  ? ShillErrorToConnectResult(*shill_error)
                                  : ShillConnectResult::kSuccess;
  base::UmaHistogramEnumeration(
      base::StrCat(
          {kNetworkMetricsPrefix, *suffix, kDisableTechnologyResultCodeSuffix}),
      result);

  base::UmaHistogramBoolean(base::StrCat({kNetworkMetricsPrefix, *suffix,
                                          kDisableTechnologyResultSuffix}),
                            success);
}

NetworkMetricsHelper::NetworkMetricsHelper() = default;

NetworkMetricsHelper::~NetworkMetricsHelper() = default;

}  // namespace ash
