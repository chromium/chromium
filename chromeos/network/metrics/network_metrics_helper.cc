// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/metrics/network_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/network/network_state.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kNetworkMetricsPrefix[] = "Network.";
const char kAllConnectionResultSuffix[] = ".ConnectionResult.All";

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
  // TODO(b/207589664): Determine histogram variant names for Ethernet.
  return {};
}

const std::vector<std::string> GetWifiNetworkTypeHistograms(
    const NetworkState* network_state) {
  // TODO(b/207589664): Determine histogram variant names for Wifi.
  return {};
}

const std::vector<std::string> GetTetherNetworkTypeHistograms(
    const NetworkState* network_state) {
  // TODO(b/207589664): Determine histogram variant names for Tether.
  return {};
}

const std::vector<std::string> GetVpnNetworkTypeHistograms(
    const NetworkState* network_state) {
  // TODO(b/207589664): Determine histogram variant names for VPN.
  return {};
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

NetworkMetricsHelper::NetworkMetricsHelper() = default;

NetworkMetricsHelper::~NetworkMetricsHelper() = default;

void NetworkMetricsHelper::Init(NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
}

void NetworkMetricsHelper::LogAllConnectionResult(
    const std::string& guid,
    const absl::optional<std::string>& shill_error) {
  DCHECK(network_state_handler_);
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(guid);

  ShillConnectResult connect_result =
      shill_error.has_value() ? ShillErrorToConnectResult(*shill_error)
                              : ShillConnectResult::kSuccess;

  for (const auto& network_type : GetNetworkTypeHistogramNames(network_state)) {
    base::UmaHistogramEnumeration(
        kNetworkMetricsPrefix + network_type + kAllConnectionResultSuffix,
        connect_result);
  }
}

NetworkMetricsHelper::ShillConnectResult
NetworkMetricsHelper::ShillErrorToConnectResult(const std::string& error_name) {
  if (error_name == shill::kErrorBadPassphrase)
    return NetworkMetricsHelper::ShillConnectResult::kBadPassphrase;
  else if (error_name == shill::kErrorBadWEPKey)
    return NetworkMetricsHelper::ShillConnectResult::kBadWepKey;
  else if (error_name == shill::kErrorConnectFailed)
    return NetworkMetricsHelper::ShillConnectResult::kFailedToConnect;
  else if (error_name == shill::kErrorDhcpFailed)
    return NetworkMetricsHelper::ShillConnectResult::kDhcpFailure;
  else if (error_name == shill::kErrorDNSLookupFailed)
    return NetworkMetricsHelper::ShillConnectResult::kDnsLookupFailure;
  else if (error_name == shill::kErrorEapAuthenticationFailed)
    return NetworkMetricsHelper::ShillConnectResult::kEapAuthentication;
  else if (error_name == shill::kErrorEapLocalTlsFailed)
    return NetworkMetricsHelper::ShillConnectResult::kEapLocalTls;
  else if (error_name == shill::kErrorEapRemoteTlsFailed)
    return NetworkMetricsHelper::ShillConnectResult::kEapRemoteTls;
  else if (error_name == shill::kErrorOutOfRange)
    return NetworkMetricsHelper::ShillConnectResult::kOutOfRange;
  else if (error_name == shill::kErrorPinMissing)
    return NetworkMetricsHelper::ShillConnectResult::kPinMissing;
  else if (error_name == shill::kErrorNoFailure)
    return NetworkMetricsHelper::ShillConnectResult::kNoFailure;
  else if (error_name == shill::kErrorNotAssociated)
    return NetworkMetricsHelper::ShillConnectResult::kNotAssociated;
  else if (error_name == shill::kErrorNotAuthenticated)
    return NetworkMetricsHelper::ShillConnectResult::kNotAuthenticated;
  else if (error_name == shill::kErrorSimLocked)
    return NetworkMetricsHelper::ShillConnectResult::kErrorSimLocked;
  else if (error_name == shill::kErrorNotRegistered)
    return NetworkMetricsHelper::ShillConnectResult::kErrorNotRegistered;
  return NetworkMetricsHelper::ShillConnectResult::kUnknown;
}

}  // namespace chromeos