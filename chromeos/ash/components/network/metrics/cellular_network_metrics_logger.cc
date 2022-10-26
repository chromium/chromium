// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

CellularNetworkMetricsLogger::CellularNetworkMetricsLogger(
    NetworkStateHandler* network_state_handler,
    NetworkMetadataStore* network_metadata_store)
    : network_state_handler_(network_state_handler),
      network_metadata_store_(network_metadata_store) {
  if (network_state_handler_) {
    network_state_handler_->AddObserver(this, FROM_HERE);
    NetworkListChanged();
  }
}

CellularNetworkMetricsLogger::~CellularNetworkMetricsLogger() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CellularNetworkMetricsLogger::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);
  for (const auto* network : network_list) {
    AttemptLogCustomApnsCount(network);
  }
}

void CellularNetworkMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  AttemptLogCustomApnsCount(network);
}

void CellularNetworkMetricsLogger::AttemptLogCustomApnsCount(
    const NetworkState* network) {
  DCHECK(network_metadata_store_)
      << "AttemptLogAllConnectionResult() called with no NetworkMetadataStore.";

  // Only cellular networks have custom APNs.
  if (network->GetNetworkTechnologyType() !=
      NetworkState::NetworkTechnologyType::kCellular) {
    return;
  }

  // Only log the number of custom APNs if the network just connected.
  if (!network->IsConnectedState()) {
    // If the network was connected and no longer is, remove its guid from
    // |connected_cellular_network_guids_|.
    auto it = connected_cellular_network_guids_.find(network->guid());
    if (it != connected_cellular_network_guids_.end())
      connected_cellular_network_guids_.erase(it);
    return;
  }

  // If the network was already connected, don't log a metric for it again.
  if (base::Contains(connected_cellular_network_guids_, network->guid()))
    return;

  connected_cellular_network_guids_.insert(network->guid());

  size_t count = 0u;
  const base::Value* custom_apn_list =
      network_metadata_store_->GetCustomAPNList(network->guid());
  if (custom_apn_list) {
    DCHECK(custom_apn_list->is_list());
    count = custom_apn_list->GetList().size();
  }

  // TODO(b/162365553): Log the number of enabled/disabled APNs.
  base::UmaHistogramCounts100("Network.Ash.Cellular.Apn.CustomApns.Count",
                              count);
}

}  // namespace ash
