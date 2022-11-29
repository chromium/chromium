// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_

#include "base/component_export.h"

#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

class NetworkMetadataStore;

// Provides APIs for logging metrics related to cellular networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularNetworkMetricsLogger
    : public ConnectionInfoMetricsLogger::Observer {
 public:
  // This enum is tied directly to the ApnTypes UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).
  enum class ApnTypes {
    kDefault = 0,
    kAttach = 1,
    kDefaultAndAttach = 2,
    kMaxValue = kDefaultAndAttach
  };

  static constexpr char kCreateCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.Result";
  static constexpr char kCreateCustomApnAuthenticationTypeHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.AuthenticationType";
  static constexpr char kCreateCustomApnIpTypeHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.IpType";
  static constexpr char kCreateCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.CreateCustomApn.ApnTypes";
  static constexpr char kRemoveCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.RemoveCustomApn.Result";
  static constexpr char kRemoveCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.RemoveCustomApn.ApnTypes";

  CellularNetworkMetricsLogger(
      NetworkStateHandler* network_state_handler,
      NetworkMetadataStore* network_metadata_store,
      ConnectionInfoMetricsLogger* connection_info_metrics_logger);
  CellularNetworkMetricsLogger(const CellularNetworkMetricsLogger&) = delete;
  CellularNetworkMetricsLogger& operator=(const CellularNetworkMetricsLogger&) =
      delete;
  ~CellularNetworkMetricsLogger() override;

  // Logs results from attempting operations related to custom APNs.
  static void LogCreateCustomApnResult(
      bool success,
      chromeos::network_config::mojom::ApnPropertiesPtr apn);
  static void LogRemoveCustomApnResult(
      bool success,
      std::vector<chromeos::network_config::mojom::ApnType> apn_types);

 private:
  // ConnectionInfoMetricsLogger::Observer:
  void OnConnectionResult(
      const std::string& guid,
      const absl::optional<std::string>& shill_error) override;

  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkMetadataStore* network_metadata_store_ = nullptr;

  base::ScopedObservation<ConnectionInfoMetricsLogger,
                          ConnectionInfoMetricsLogger::Observer>
      connection_info_metrics_logger_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
