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

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

class CellularESimProfileHandler;
class CellularMetricsLoggerTest;
class ESimFeatureUsageMetrics;
class NetworkConnectionHandler;
class NetworkState;
class NetworkStateHandler;

// Cellular network SIM types.
enum class SimType {
  kPSim,
  kESim,
};

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
  // Histograms associated with SIM Pin operations.
  static const char kSimPinLockSuccessHistogram[];
  static const char kSimPinUnlockSuccessHistogram[];
  static const char kSimPinUnblockSuccessHistogram[];
  static const char kSimPinChangeSuccessHistogram[];

  // PIN operations that are tracked by metrics.
  enum class SimPinOperation {
    kLock = 0,
    kUnlock = 1,
    kUnblock = 2,
    kChange = 3,
  };

  // Records the result of pin operations performed.
  static void RecordSimPinOperationResult(
      const SimPinOperation& pin_operation,
      const base::Optional<std::string>& shill_error_name = base::nullopt);

  // Registers device preferences used by this class in the provided
  // |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  CellularMetricsLogger();
  ~CellularMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConnectionHandler* network_connection_handler,
            CellularESimProfileHandler* cellular_esim_profile_handler);

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // NetworkStateHandlerObserver::
  void DeviceListChanged() override;
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // NetworkConnectionObserver::
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;
  void DisconnectRequested(const std::string& service_path) override;

  void SetDevicePrefs(PrefService* device_prefs);

 private:
  friend class CellularMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           DuplicateCellularServiceGuids);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest, CellularConnectResult);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularESimProfileStatusAtLoginTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularServiceAtLoginTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest, CellularUsageCountTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularUsageCountDongleTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularPSimActivationStateAtLoginTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularTimeToConnectedTest);
  FRIEND_TEST_ALL_PREFIXES(CellularMetricsLoggerTest,
                           CellularDisconnectionsTest);
  FRIEND_TEST_ALL_PREFIXES(NetworkDeviceHandlerTest, RequirePin);
  FRIEND_TEST_ALL_PREFIXES(NetworkDeviceHandlerTest, EnterPin);
  FRIEND_TEST_ALL_PREFIXES(NetworkDeviceHandlerTest, UnblockPin);
  FRIEND_TEST_ALL_PREFIXES(NetworkDeviceHandlerTest, ChangePin);

  // The amount of time after cellular device is added to device list,
  // after which cellular device is considered initialized.
  static const base::TimeDelta kInitializationTimeout;

  // The amount of time after a disconnect request within which any
  // disconnections are considered user initiated.
  static const base::TimeDelta kDisconnectRequestTimeout;

  // Stores connection related information for a cellular network.
  struct ConnectionInfo {
    explicit ConnectionInfo(const std::string& network_guid);
    ConnectionInfo(const std::string& network_guid,
                   bool is_connected,
                   bool is_connecting);
    ~ConnectionInfo();
    const std::string network_guid;
    base::Optional<bool> is_connected;
    bool is_connecting = false;
    base::Optional<base::TimeTicks> last_disconnect_request_time;
    base::Optional<base::TimeTicks> last_connect_start_time;
  };

  // Usage type for cellular network. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never
  // be reused.
  enum class CellularUsage {
    kConnectedAndOnlyNetwork = 0,
    kConnectedWithOtherNetwork = 1,
    kNotConnected = 2,
    kMaxValue = kNotConnected
  };

  // Activation state for PSim cellular network.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class PSimActivationState {
    kActivated = 0,
    kActivating = 1,
    kNotActivated = 2,
    kPartiallyActivated = 3,
    kUnknown = 4,
    kMaxValue = kUnknown
  };

  // Profile status for ESim cellular network.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class ESimProfileStatus {
    kActive = 0,
    kActiveWithPendingProfiles = 1,
    kPendingProfilesOnly = 2,
    kNoProfiles = 3,
    kMaxValue = kNoProfiles
  };

  // Cellular connection state. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should
  // never be reused.
  enum class ConnectionState {
    kConnected = 0,
    kDisconnected = 1,
    kMaxValue = kDisconnected
  };

  // Result of PIN operations.
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  // Note: With the exception of Success, enums should match the
  // error names listed near the top of NetworkDeviceHandler.
  enum class SimPinOperationResult {
    kSuccess = 0,
    kErrorDeviceMissing = 1,
    kErrorFailure = 2,
    kErrorIncorrectPin = 3,
    kErrorNotFound = 4,
    kErrorNotSupported = 5,
    kErrorPinBlocked = 6,
    kErrorPinRequired = 7,
    kErrorTimeout = 8,
    kErrorUnknown = 9,
    kMaxValue = kErrorUnknown,
  };

  // Result of connecting to a cellular network. These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  enum class ConnectResult {
    kUnknown = 0,
    kInvalidGuid = 1,
    kInvalidState = 2,
    kCanceled = 3,
    kNotConfigured = 4,
    kBlocked = 5,
    kSuccess = 6,
    kMaxValue = kSuccess
  };

  // Convert shill error name string to SimPinOperationResult enum.
  static SimPinOperationResult GetSimPinOperationResultForShillError(
      const std::string& shill_error_name);

  // Convert shill activation state string to PSimActivationState enum
  PSimActivationState PSimActivationStateToEnum(const std::string& state);

  // Converts a NetworkConnectionHandler string error to a ConnectResult enum.
  ConnectResult NetworkConnectionErrorToConnectResult(
      const std::string& error_name);

  // Helper method to save cellular disconnections histogram.
  void LogCellularDisconnectionsHistogram(ConnectionState connection_state,
                                          SimType sim_type);

  void LogCellularConnectionSuccessHistogram(ConnectResult start_connect_result,
                                             SimType sim_type);

  void OnInitializationTimeout();

  // Tracks cellular network connection state and logs time to connected.
  void CheckForTimeToConnectedMetric(const NetworkState* network);

  // Tracks cellular network connected states and non user initiated
  // disconnections.
  void CheckForConnectionStateMetric(const NetworkState* network);

  // Tracks the activation state of the PSim cellular network if available and
  // if |is_psim_activation_state_logged_| is false.
  void CheckForPSimActivationStateMetric();

  // Tracks the activation state of ESim cellular networks if available and
  // if |is_esim_profile_status_logged_| is false.
  void CheckForESimProfileStatusMetric();

  // Tracks errors from shill that result in an unsuccessful connection.
  void CheckForShillConnectionFailureMetric(const NetworkState* network);

  // This checks the state of connected networks and logs
  // cellular network usage histogram. Histogram is only logged
  // when usage state changes.
  void CheckForCellularUsageMetrics();

  // Tracks how many eSIM profiles are installed on the device and how many pSIM
  // networks are available on the device if |is_service_count_logged_| is true.
  void CheckForCellularServiceCountMetric();

  // Returns the ConnectionInfo for given |cellular_network_guid|.
  ConnectionInfo* GetConnectionInfoForCellularNetwork(
      const std::string& cellular_network_guid);

  // Tracks the last cellular network usage state.
  base::Optional<CellularUsage> last_cellular_usage_;

  // Tracks the last PSim cellular network usage state.
  base::Optional<CellularUsage> last_psim_cellular_usage_;

  // Tracks the last time the PSim network's cellular usage changed.
  base::Optional<base::Time> last_psim_usage_change_timestamp_;

  // Tracks the last ESim cellular network usage state.
  base::Optional<CellularUsage> last_esim_cellular_usage_;

  // Tracks the last time the ESim network's cellular usage changed.
  base::Optional<base::Time> last_esim_usage_change_timestamp_;

  // Tracks whether cellular device is available or not.
  bool is_cellular_available_ = false;

  NetworkStateHandler* network_state_handler_ = nullptr;

  NetworkConnectionHandler* network_connection_handler_ = nullptr;

  CellularESimProfileHandler* cellular_esim_profile_handler_ = nullptr;

  // A timer to wait for cellular initialization. This is useful
  // to avoid tracking intermediate states when cellular network is
  // starting up.
  base::OneShotTimer initialization_timer_;

  // Tracks whether the PSim activation state is already logged for this
  // session.
  bool is_psim_activation_state_logged_ = false;

  // Tracks whether the ESim profile status is already logged for this
  // session.
  bool is_esim_profile_status_logged_ = false;

  // Tracks whether service count is already logged for this session.
  bool is_service_count_logged_ = false;

  // Stores connection information for all cellular networks.
  base::flat_map<std::string, std::unique_ptr<ConnectionInfo>>
      guid_to_connection_info_map_;

  bool initialized_ = false;

  // Tracks ESim feature usage for the Standard Feature Usage Logging Framework.
  std::unique_ptr<ESimFeatureUsageMetrics> esim_feature_usage_metrics_;

  DISALLOW_COPY_AND_ASSIGN(CellularMetricsLogger);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_METRICS_LOGGER_H_
