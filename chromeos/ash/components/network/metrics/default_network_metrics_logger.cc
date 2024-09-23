// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/default_network_metrics_logger.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

// static
const char DefaultNetworkMetricsLogger::kDefaultNetworkMeterSubtypeHistogram[] =
    "Network.Ash.DefaultNetwork.MeterSubtype";

DefaultNetworkMetricsLogger::DefaultNetworkMetricsLogger() = default;

DefaultNetworkMetricsLogger::~DefaultNetworkMetricsLogger() = default;

void DefaultNetworkMetricsLogger::Init(
    NetworkStateHandler* network_state_handler) {
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_observer_.Observe(network_state_handler_.get());
  UpdateAndRecordTechnologyMeterSubtype();
}

void DefaultNetworkMetricsLogger::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void DefaultNetworkMetricsLogger::DefaultNetworkChanged(
    const NetworkState* network) {
  UpdateAndRecordTechnologyMeterSubtype();
}

void DefaultNetworkMetricsLogger::UpdateAndRecordTechnologyMeterSubtype() {
  NetworkStateHandler::NetworkStateList active_networks;

  // Ignore VPN since it is always on top of a physical network.
  const NetworkState* default_non_vpn_network =
      network_state_handler_->ActiveNetworkByType(
          NetworkTypePattern::NonVirtual());

  if (!default_non_vpn_network) {
    return;
  }

  const std::string guid = default_non_vpn_network->guid();
  bool is_metered = default_non_vpn_network->metered();

  if (!guid.empty() && guid_ == guid && is_metered_ == is_metered) {
    return;
  }

  guid_ = guid;
  is_metered_ = is_metered;

  if (auto subtype = GetNetworkTechnologyMeterSubtype(default_non_vpn_network);
      subtype) {
    base::UmaHistogramEnumeration(kDefaultNetworkMeterSubtypeHistogram,
                                  *subtype);
  }
}

std::optional<DefaultNetworkMetricsLogger::NetworkTechnologyMeterSubtype>
DefaultNetworkMetricsLogger::GetNetworkTechnologyMeterSubtype(
    const NetworkState* network) {
  if (!network) {
    return std::nullopt;
  }

  switch (network->GetNetworkTechnologyType()) {
    case NetworkState::NetworkTechnologyType::kCellular:
      return network->metered()
                 ? NetworkTechnologyMeterSubtype::kCellularMetered
                 : NetworkTechnologyMeterSubtype::kCellular;
    case NetworkState::NetworkTechnologyType::kEthernetEap:
      [[fallthrough]];
    case NetworkState::NetworkTechnologyType::kEthernet:
      return NetworkTechnologyMeterSubtype::kEthernet;
    case NetworkState::NetworkTechnologyType::kWiFi:
      return network->metered() ? NetworkTechnologyMeterSubtype::kWifiMetered
                                : NetworkTechnologyMeterSubtype::kWifi;
    case NetworkState::NetworkTechnologyType::kTether:
      return network->metered() ? NetworkTechnologyMeterSubtype::kTetherMetered
                                : NetworkTechnologyMeterSubtype::kTether;
    case NetworkState::NetworkTechnologyType::kVPN:
      [[fallthrough]];
    case NetworkState::NetworkTechnologyType::kUnknown:
      [[fallthrough]];
    default:
      return std::nullopt;
  }
}

}  // namespace ash
