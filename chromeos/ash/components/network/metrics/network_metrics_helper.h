// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_

#include <optional>

#include "base/component_export.h"
#include "chromeos/ash/components/network/metrics/connection_results.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

// Provides APIs for logging to general network metrics that apply to all
// network types and their variants.
// TODO(b/211823175): Replace NetworkMetricsHelper with plain helper methods.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkMetricsHelper {
 public:
  // Network connection state. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should
  // never be reused.
  enum class ConnectionState {
    kConnected = 0,
    kDisconnectedWithoutUserAction = 1,
    kMaxValue = kDisconnectedWithoutUserAction
  };

  // Logs connection result for network with given |guid| in
  // Network.Ash.{NetworkType}.ConnectionResult.All,
  // Network.Ash.{NetworkType}.ConnectionResult.All.Filtered and also the
  // result to Network.Ash.{NetworkType}.ConnectionResult.Auto if
  // `is_auto_connect` is set to true. `is_repeated_error` is true if
  // the current shill error is the same as the previous shill error.
  // If `shill_error` has no value, a connection success is logged.
  static void LogAllConnectionResult(
      const std::string& guid,
      bool is_auto_connect,
      bool is_repeated_error,
      const std::optional<std::string>& shill_error = std::nullopt);

  // Logs result of a user initiated connection attempt for a network with a
  // given |guid|. If |network_connection_error| has no value, a connection
  // success is logged.
  static void LogUserInitiatedConnectionResult(
      const std::string& guid,
      const std::optional<std::string>& network_connection_error =
          std::nullopt);

  // Logs to relevant connection states such as non-user initiated
  // disconnections from a connected state and successful connections. More may
  // be added if relevant.
  static void LogConnectionStateResult(
      const std::string& guid,
      const ConnectionState status,
      const std::optional<ShillConnectResult> shill_error);

  // Logs result of an attempt to enable a shill associated network technology
  // type.
  static void LogEnableTechnologyResult(
      const std::string& technology,
      bool success,
      const std::optional<std::string>& shill_error = std::nullopt);

  // Logs result of an attempt to disable a shill associated network technology
  // type.
  static void LogDisableTechnologyResult(
      const std::string& technology,
      bool success,
      const std::optional<std::string>& shill_error = std::nullopt);

  NetworkMetricsHelper();
  ~NetworkMetricsHelper();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_
