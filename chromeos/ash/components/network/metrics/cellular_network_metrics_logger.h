// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/metrics/connection_info_metrics_logger.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace ash {

class NetworkMetadataStore;

// Provides APIs for logging metrics related to cellular networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularNetworkMetricsLogger
    : public ConnectionInfoMetricsLogger::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ApnTypes {
    kDefault = 0,
    kAttach = 1,
    kDefaultAndAttach = 2,
    kMaxValue = kDefaultAndAttach
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UnmanagedApnMigrationType {
    kMatchesLastGoodApn = 0,
    kDoesNotMatchLastGoodApn = 1,
    kMatchesLastConnectedAttachAndDefault = 2,
    kMatchesLastConnectedAttachHasMatchingDatabaseApn = 3,
    kMatchesLastConnectedAttachHasNoMatchingDatabaseApn = 4,
    kMatchesLastConnectedDefaultNoLastConnectedAttach = 5,
    kNoMatchingConnectedApn = 6,
    kMaxValue = kNoMatchingConnectedApn
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ManagedApnMigrationType {
    kMatchesSelectedApn = 0,
    kDoesNotMatchSelectedApn = 1,
    kMaxValue = kDoesNotMatchSelectedApn
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
  static constexpr char kModifyCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.ModifyCustomApn.Result";
  static constexpr char kModifyCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.ModifyCustomApn.ApnTypes";
  static constexpr char kEnableCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.EnableCustomApn.Result";
  static constexpr char kEnableCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.EnableCustomApn.ApnTypes";
  static constexpr char kDisableCustomApnResultHistogram[] =
      "Network.Ash.Cellular.Apn.DisableCustomApn.Result";
  static constexpr char kDisableCustomApnApnTypesHistogram[] =
      "Network.Ash.Cellular.Apn.DisableCustomApn.ApnTypes";
  static constexpr char kConnectResultHasEnabledCustomApnsAllHistogram[] =
      "Network.Ash.Cellular.ConnectionResult.HasEnabledCustomApns.All";
  static constexpr char kConnectResultNoEnabledCustomApnsAllHistogram[] =
      "Network.Ash.Cellular.ConnectionResult.NoEnabledCustomApns.All";
  static constexpr char kCustomApnsCountHistogram[] =
      "Network.Ash.Cellular.Apn.CustomApns.Count";
  static constexpr char kCustomApnsEnabledCountHistogram[] =
      "Network.Ash.Cellular.Apn.CustomApns.Enabled.Count";
  static constexpr char kCustomApnsDisabledCountHistogram[] =
      "Network.Ash.Cellular.Apn.CustomApns.Disabled.Count";
  static constexpr char kCustomApnsUnmanagedMigrationTypeHistogram[] =
      "Network.Ash.Cellular.Apn.Unmanaged.MigrationType";
  static constexpr char kCustomApnsManagedMigrationTypeHistogram[] =
      "Network.Ash.Cellular.Apn.Managed.MigrationType";

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
  static void LogModifyCustomApnResult(
      bool success,
      std::vector<chromeos::network_config::mojom::ApnType> old_apn_types,
      absl::optional<chromeos::network_config::mojom::ApnState> apn_state,
      absl::optional<chromeos::network_config::mojom::ApnState> old_apn_state);
  static void LogUnmanagedCustomApnMigrationType(
      UnmanagedApnMigrationType type);
  static void LogManagedCustomApnMigrationType(ManagedApnMigrationType type);

 private:
  // ConnectionInfoMetricsLogger::Observer:
  void OnConnectionResult(
      const std::string& guid,
      const absl::optional<std::string>& shill_error) override;

  raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_ =
      nullptr;
  raw_ptr<NetworkMetadataStore, DanglingUntriaged | ExperimentalAsh>
      network_metadata_store_ = nullptr;

  base::ScopedObservation<ConnectionInfoMetricsLogger,
                          ConnectionInfoMetricsLogger::Observer>
      connection_info_metrics_logger_observation_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_CELLULAR_NETWORK_METRICS_LOGGER_H_
