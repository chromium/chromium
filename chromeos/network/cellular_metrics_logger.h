// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_
#define CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;
class NetworkState;
class CellularMetricsLoggerTest;
class NetworkConnectionHandler;

// Class for tracking cellular network related metrics.
//
// This class adds observers on network state and makes the following
// measurements on cellular networks:
// 1. Time to connected.
// 2. Connected states and non-user initiated disconnections.
// 3. Activation status at login.
// 4. Cellular network usage type.
//
// Note: This class does not start logging metrics until Init() is
// invoked.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularMetricsLogger
    : public NetworkStateHandlerObserver,
      public LoginState::Observer,
      public NetworkConnectionObserver {
 public:
  CellularMetricsLogger();
  ~CellularMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConnectionHandler* network_connection_handler);

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // NetworkStateHandlerObserver::
  void DeviceListChanged() override;
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // NetworkConnectionObserver::
  void DisconnectRequested(const std::string& service_path) override;

 private:
  friend class CellularMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest, CellularUsageCountTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularUsageCountDongleTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularActivationStateAtLoginTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularTimeToConnectedTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularDisconnectionsTest);

  // The amount of time after cellular device is added to device list,
  // after which cellular device is considered initialized.
  static const base::TimeDelta kInitializationTimeout;

  // The amount of time after a disconnect request within which any
  // disconnections are considered user initiated.
  static const base::TimeDelta kDisconnectRequestTimeout;

  // Stores connection related information for a cellular network.
  struct ConnectionInfo {
    ConnectionInfo(const std::string& network_guid);
    ConnectionInfo(const std::string& network_guid, bool is_connected);
    ~ConnectionInfo();
    const std::string network_guid;
    base::Optional<bool> is_connected;
    base::Optional<base::TimeTicks> last_disconnect_request_time;
    base::Optional<base::TimeTicks> last_connect_start_time;
  };

  // Usage type for cellular network. These values are persisted to logs.
  // Entries should not be renumbered and numberic values should never
  // be reused.
  enum class CellularUsage {
    kConnectedAndOnlyNetwork = 0,
    kConnectedWithOtherNetwork = 1,
    kNotConnected = 2,
    kMaxValue = kNotConnected
  };

  // Activation state for cellular network.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class ActivationState {
    kActivated = 0,
    kActivating = 1,
    kNotActivated = 2,
    kPartiallyActivated = 3,
    kUnknown = 4,
    kMaxValue = kUnknown
  };

  // Cellular connection state. These values are persisted to logs.
  // Entries should not be renumbered and numberic values should
  // never be reused.
  enum class ConnectionState {
    kConnected = 0,
    kDisconnected = 1,
    kMaxValue = kDisconnected
  };

  // Convert shill activation state string to ActivationState enum.
  static ActivationState ActivationStateToEnum(const std::string& state);

  // Checks whether the current logged in user type is an owner or regular.
  static bool IsLoggedInUserOwnerOrRegular();

  // Helper method to save cellular disconnections histogram.
  static void LogCellularDisconnectionsHistogram(
      ConnectionState connection_state);

  void OnInitializationTimeout();

  // Tracks cellular network connection state and logs time to connected.
  void CheckForTimeToConnectedMetric(const NetworkState* network);

  // Tracks cellular network connected states and non user initiated
  // disconnections.
  void CheckForConnectionStateMetric(const NetworkState* network);

  // Tracks the activation state of cellular network if available and
  // if |log_activation_state_| is true.
  void CheckForActivationStateMetric();

  // This checks the state of connected networks and logs
  // cellular network usage histogram. Histogram is only logged
  // when usage state changes.
  void CheckForCellularUsageCountMetric();

  // Returns the ConnectionInfo for given |cellular_network_guid|.
  ConnectionInfo* GetConnectionInfoForCellularNetwork(
      const std::string& cellular_network_guid);

  // Tracks the last cellular network usage state.
  base::Optional<CellularUsage> last_cellular_usage_;

  // Tracks whether cellular device is available or not.
  bool is_cellular_available_ = false;

  NetworkStateHandler* network_state_handler_ = nullptr;

  NetworkConnectionHandler* network_connection_handler_ = nullptr;

  // A timer to wait for cellular initialization. This is useful
  // to avoid tracking intermediate states when cellular network is
  // starting up.
  base::OneShotTimer initialization_timer_;

  // Tracks whether activation state is already logged for this
  // session.
  bool is_activation_state_logged_ = false;

  // Stores connection information for all cellular networks.
  base::flat_map<std::string, std::unique_ptr<ConnectionInfo>>
      guid_to_connection_info_map_;

  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(CellularMetricsLogger);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_
