// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_connection_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"

namespace ash {

namespace tether {

HostConnectionMetricsLogger::HostConnectionMetricsLogger(
    ActiveHost* active_host)
    : active_host_(active_host), clock_(base::DefaultClock::GetInstance()) {
  active_host_->AddObserver(this);
}

HostConnectionMetricsLogger::~HostConnectionMetricsLogger() {
  active_host_->RemoveObserver(this);
}

void HostConnectionMetricsLogger::RecordConnectionToHostResult(
    ConnectionToHostResult result,
    const std::string& device_id) {
  // Persist this value for later use in RecordConnectionResultSuccess(). It
  // will be cleared once used.
  active_host_device_id_ = device_id;

  switch (result) {
    case ConnectionToHostResult::CONNECTION_RESULT_PROVISIONING_FAILED:
      RecordConnectionResultProvisioningFailure(
          ConnectionToHostResult_ProvisioningFailureEventType::
              PROVISIONING_FAILED);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_SUCCESS:
      RecordConnectionResultSuccess(
          ConnectionToHostResult_SuccessEventType::SUCCESS);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::TIMEOUT);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::
              CANCELED_BY_USER);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::
              INTERNAL_ERROR);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED:
      RecordConnectionResultFailureTetheringTimeout(
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_REQUIRED);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED:
      RecordConnectionResultFailureTetheringTimeout(
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_NOT_REQUIRED);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::TETHERING_UNSUPPORTED);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_FAILURE_NO_CELL_DATA:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::NO_CELL_DATA);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::ENABLING_HOTSPOT_FAILED);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::ENABLING_HOTSPOT_TIMEOUT);
      break;
    case ConnectionToHostResult::CONNECTION_RESULT_FAILURE_NO_RESPONSE:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::NO_RESPONSE);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::INVALID_HOTSPOT_CREDENTIALS);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_SUCCESSFUL_REQUEST_BUT_NO_RESPONSE:
      RecordConnectionResultFailure(ConnectionToHostResult_FailureEventType::
                                        SUCCESSFUL_REQUEST_BUT_NO_RESPONSE);
      break;
    case ConnectionToHostResult::
        CONNECTION_RESULT_FAILURE_UNRECOGNIZED_RESPONSE_ERROR:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::UNRECOGNIZED_RESPONSE_ERROR);
      break;
    default:
      NOTREACHED();
  };
}

void HostConnectionMetricsLogger::OnActiveHostChanged(
    const ActiveHost::ActiveHostChangeInfo& change_info) {
  if (change_info.new_status == ActiveHost::ActiveHostStatus::CONNECTING) {
    connect_to_host_start_time_ = clock_->Now();
  } else if (change_info.new_status ==
             ActiveHost::ActiveHostStatus::CONNECTED) {
    RecordConnectToHostDuration(change_info.new_active_host->GetDeviceId());
  }
}

void HostConnectionMetricsLogger::RecordConnectionResultProvisioningFailure(
    ConnectionToHostResult_ProvisioningFailureEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.ProvisioningFailureRate",
      event_type,
      ConnectionToHostResult_ProvisioningFailureEventType::
          PROVISIONING_FAILURE_MAX);
}

void HostConnectionMetricsLogger::RecordConnectionResultSuccess(
    ConnectionToHostResult_SuccessEventType event_type) {
  DCHECK(!active_host_device_id_.empty());

  active_host_device_id_.clear();

  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.SuccessRate.Background",
      event_type, ConnectionToHostResult_SuccessEventType::SUCCESS_MAX);

  RecordConnectionResultProvisioningFailure(
      ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

void HostConnectionMetricsLogger::RecordConnectionResultFailure(
    ConnectionToHostResult_FailureEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure", event_type,
      ConnectionToHostResult_FailureEventType::FAILURE_MAX);

  RecordConnectionResultSuccess(
      ConnectionToHostResult_SuccessEventType::FAILURE);
}

void HostConnectionMetricsLogger::RecordConnectionResultFailureClientConnection(
    ConnectionToHostResult_FailureClientConnectionEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure.ClientConnection",
      event_type,
      ConnectionToHostResult_FailureClientConnectionEventType::
          FAILURE_CLIENT_CONNECTION_MAX);

  RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType::CLIENT_CONNECTION_ERROR);
}

void HostConnectionMetricsLogger::RecordConnectionResultFailureTetheringTimeout(
    ConnectionToHostResult_FailureTetheringTimeoutEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure.TetheringTimeout",
      event_type,
      ConnectionToHostResult_FailureTetheringTimeoutEventType::
          FAILURE_TETHERING_TIMEOUT_MAX);

  RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType::TETHERING_TIMED_OUT);
}

void HostConnectionMetricsLogger::RecordConnectToHostDuration(
    const std::string device_id) {
  DCHECK(!connect_to_host_start_time_.is_null());

  base::TimeDelta connect_to_host_duration =
      clock_->Now() - connect_to_host_start_time_;
  connect_to_host_start_time_ = base::Time();

  UMA_HISTOGRAM_MEDIUM_TIMES(
      "InstantTethering.Performance.ConnectToHostDuration.Background",
      connect_to_host_duration);
}

void HostConnectionMetricsLogger::SetClockForTesting(base::Clock* test_clock) {
  clock_ = test_clock;
}

}  // namespace tether

}  // namespace ash
