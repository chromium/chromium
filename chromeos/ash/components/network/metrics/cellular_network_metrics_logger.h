// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_

#include "base/component_export.h"

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkMetadataStore;

// Provides APIs for logging metrics related to cellular networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularNetworkMetricsLogger
    : public NetworkStateHandlerObserver {
 public:
  CellularNetworkMetricsLogger(NetworkStateHandler* network_state_handler,
                               NetworkMetadataStore* network_metadata_store);
  CellularNetworkMetricsLogger(const CellularNetworkMetricsLogger&) = delete;
  CellularNetworkMetricsLogger& operator=(const CellularNetworkMetricsLogger&) =
      delete;
  ~CellularNetworkMetricsLogger() override;

 private:
  // NetworkStateHandlerObserver::
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // Logs the number of custom APNs saved for a network.
  void AttemptLogCustomApnsCount(const NetworkState* network);

  // GUIDs of cellular networks that are currently connected.
  base::flat_set<std::string> connected_cellular_network_guids_;

  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkMetadataStore* network_metadata_store_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
