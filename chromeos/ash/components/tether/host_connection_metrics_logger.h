// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/tether/active_host.h"

namespace base {
class Clock;
}  // namespace base

namespace ash {

namespace tether {

// Wrapper around metrics reporting for host connection results. Clients are
// expected to report the result of a host connection attempt once it has
// concluded.
class HostConnectionMetricsLogger : public ActiveHost::Observer {
 public:
  enum class ConnectionToHostResult {
    SUCCESS = 0,
    INTERNAL_ERROR = 1,
    USER_CANCELLATION = 2,
    PROVISIONING_FAILURE = 3,
    NO_CELLULAR_DATA = 4,
    TETHERING_UNSUPPORTED = 5,
    CANCELLED_FOR_NEWER_CONNECTION = 6,
    TETHER_SHUTDOWN_DURING_CONNECTION = 7,
    CONNECTION_TO_HOST_RESULT_MAX,
  };

  enum class ConnectionToHostInternalError {
    UNKNOWN_ERROR,
    CLIENT_CONNECTION_TIMEOUT,
    CLIENT_CONNECTION_INTERNAL_ERROR,
    CLIENT_CONNECTION_NETWORK_CONNECTION_HANDLER_FAILED,
    CLIENT_CONNECTION_NETWORK_STATE_WAS_NULL,
    CLIENT_CONNECTION_WIFI_FAILED_TO_ENABLE,
    TETHERING_TIMED_OUT_FIRST_TIME_SETUP_REQUIRED,
    TETHERING_TIMED_OUT_FIRST_TIME_SETUP_NOT_REQUIRED,
    ENABLING_HOTSPOT_FAILED,
    ENABLING_HOTSPOT_TIMEOUT,
    NO_RESPONSE,
    INVALID_HOTSPOT_CREDENTIALS,
    SUCCESSFUL_REQUEST_BUT_NO_RESPONSE,
    UNRECOGNIZED_RESPONSE_ERROR,
    INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG,
    INVALID_NEW_SOFT_AP_CONFIG,
    INVALID_WIFI_AP_CONFIG,
  };

  // Record the result of an attempted host connection.
  virtual void RecordConnectionToHostResult(
      ConnectionToHostResult result,
      const std::string& device_id,
      std::optional<ConnectionToHostInternalError> internal_error);

  HostConnectionMetricsLogger(ActiveHost* active_host);

  HostConnectionMetricsLogger(const HostConnectionMetricsLogger&) = delete;
  HostConnectionMetricsLogger& operator=(const HostConnectionMetricsLogger&) =
      delete;

  virtual ~HostConnectionMetricsLogger();

 protected:
  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  friend class HostConnectionMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure_InvalidWifiApConfig);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultProvisioningFailure);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess_Background);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess_MultiDeviceApiEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultSuccess_Background_DifferentDevice);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure_Background);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure_MultiDeviceApiEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailure_Background_DifferentDevice);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_Timeout);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_CanceledByUser);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailure_InvalidNewSoftApConfig);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailure_InvalidActiveExistingSoftApConfig);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_InternalError);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureTetheringTimeout_SetupRequired);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureTetheringTimeout_SetupNotRequired);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureTetheringUnsupported);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureNoCellData);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureEnablingHotspotFailed);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureShutDownDuringConnectionAttempt);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_WifiFailedToEnable);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_NetworkConnectionHandlerFailed);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_NetworkStateWasNull);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureEnablingHotspotTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureCancelledForNewerConnection);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResult);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectToHostDuration);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectToHostDuration_Background);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectToHostDuration_MultiDeviceApiEnabled);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureNoResponse);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureInvalidHotspotCredentials);

  void RecordInternalError(ConnectionToHostInternalError internal_error);

  void RecordUnavoidableError(ConnectionToHostResult result);

  // An Instant Tethering connection can fail for several different reasons.
  // Though traditionally success and each failure case would be logged to a
  // single enum, we have chosen to start at a top-level of view of simply
  // "success vs failure", and then iteratively breaking down the failure count
  // into its types (and possibly sub-types). Because of this cascading nature,
  // when a failure sub-type occurs, the code path in question must report that
  // sub-type as well as all the super-types above it. This would be cumbersome,
  // and thus HostConnectionMetricsLogger exists to provide a simple API which
  // handles all the other metrics that need to be reported.
  //
  // The most top-level metric is
  // InstantTethering.ConnectionToHostResult.ProvisioningFailureRate. Its
  // breakdown, and the breakdown of its subsquent metrics, is described in
  // tools/metrics/histograms/histograms.xml.
  enum class ConnectionToHostResult_ProvisioningFailureEventType {
    PROVISIONING_FAILED = 0,
    OTHER = 1,
    PROVISIONING_FAILURE_MAX
  };

  enum class ConnectionToHostResult_SuccessEventType {
    SUCCESS = 0,
    FAILURE = 1,
    SUCCESS_MAX
  };

  enum class ConnectionToHostResult_UnavoidableErrorEventType {
    OTHER = 0,
    PROVISIONING_FAILED = 1,
    USER_CANCELLATION = 2,
    TETHERING_UNSUPPORTED = 3,
    NO_CELLULAR_DATA = 4,
    SHUT_DOWN_DURING_CONNECTION = 5,
    CANCELLED_FOR_NEWER_CONNECTION_ATTEMPT = 6,
    kMax
  };

  enum class ConnectionToHostResult_FailureEventType {
    UNKNOWN_ERROR = 0,
    TETHERING_TIMED_OUT = 1,
    CLIENT_CONNECTION_ERROR = 2,
    TETHERING_UNSUPPORTED = 3,
    NO_CELL_DATA = 4,
    ENABLING_HOTSPOT_FAILED = 5,
    ENABLING_HOTSPOT_TIMEOUT = 6,
    NO_RESPONSE = 7,
    INVALID_HOTSPOT_CREDENTIALS = 8,
    SUCCESSFUL_REQUEST_BUT_NO_RESPONSE = 9,
    UNRECOGNIZED_RESPONSE_ERROR = 10,
    INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG = 11,
    INVALID_NEW_SOFT_AP_CONFIG = 12,
    INVALID_WIFI_AP_CONFIG = 13,
    FAILURE_MAX
  };

  enum class ConnectionToHostResult_FailureClientConnectionEventType {
    TIMEOUT = 0,
    CANCELED_BY_USER = 1,  // Obsolete
    INTERNAL_ERROR = 2,
    NETWORK_CONNECTION_HANDLER_FAILED = 3,
    NETWORK_STATE_WAS_NULL = 4,
    WIFI_FAILED_TO_ENABLED = 5,
    FAILURE_CLIENT_CONNECTION_MAX
  };

  enum class ConnectionToHostResult_FailureTetheringTimeoutEventType {
    FIRST_TIME_SETUP_WAS_REQUIRED = 0,
    FIRST_TIME_SETUP_WAS_NOT_REQUIRED = 1,
    FAILURE_TETHERING_TIMEOUT_MAX
  };

  // Record how a host connection attempt failed. Failure due to client error or
  // tethering timeout is covered by the
  // RecordConnectionResultFailureClientConnection() or
  // RecordConnectionResultFailureTetheringTimeout() methods, respectively.
  void RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType event_type);

  // Record how a host connection attempt failed due to a client error.
  void RecordConnectionResultFailureClientConnection(
      ConnectionToHostResult_FailureClientConnectionEventType event_type);

  // Record the conditions of when host connection attempt failed due to
  // the host timing out during tethering.
  void RecordConnectionResultFailureTetheringTimeout(
      ConnectionToHostResult_FailureTetheringTimeoutEventType event_type);

  void RecordConnectToHostDuration(const std::string device_id);

  void SetClockForTesting(base::Clock* test_clock);

  raw_ptr<ActiveHost> active_host_;
  raw_ptr<base::Clock> clock_;

  base::Time connect_to_host_start_time_;
  std::string active_host_device_id_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_
