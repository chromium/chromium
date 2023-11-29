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
    const std::string& device_id,
    absl::optional<ConnectionToHostInternalError> internal_error) {
  if (!active_host_device_id_.empty()) {
    active_host_device_id_.clear();
  }

  if (result == ConnectionToHostResult::INTERNAL_ERROR) {
    CHECK(internal_error.has_value());
    RecordInternalError(internal_error.value());
  } else {
    CHECK(!internal_error.has_value());
  }

  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.EndResult", result,
      ConnectionToHostResult::CONNECTION_TO_HOST_RESULT_MAX);

  // Preserve legacy
  // InstantTethering.ConnectionToHostResult.ProvisioningFailureRate metric by
  // counting the PROVISIONING_FAILED result as a provisioning failure, and
  // other results as "other"
  if (result == ConnectionToHostResult::PROVISIONING_FAILURE) {
    UMA_HISTOGRAM_ENUMERATION(
        "InstantTethering.ConnectionToHostResult.ProvisioningFailureRate",
        ConnectionToHostResult_ProvisioningFailureEventType::
            PROVISIONING_FAILED,
        ConnectionToHostResult_ProvisioningFailureEventType::
            PROVISIONING_FAILURE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "InstantTethering.ConnectionToHostResult.ProvisioningFailureRate",
        ConnectionToHostResult_ProvisioningFailureEventType::OTHER,
        ConnectionToHostResult_ProvisioningFailureEventType::
            PROVISIONING_FAILURE_MAX);

    // Preserve InstantTethering.ConnectionToHostResult.SuccessRate.Background
    // by counting "success" as success, and all other results as failures.
    if (result == ConnectionToHostResult::SUCCESS) {
      UMA_HISTOGRAM_ENUMERATION(
          "InstantTethering.ConnectionToHostResult.SuccessRate.Background",
          ConnectionToHostResult_SuccessEventType::SUCCESS,
          ConnectionToHostResult_SuccessEventType::SUCCESS_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "InstantTethering.ConnectionToHostResult.SuccessRate.Background",
          ConnectionToHostResult_SuccessEventType::FAILURE,
          ConnectionToHostResult_SuccessEventType::SUCCESS_MAX);
    }
  }
}

void HostConnectionMetricsLogger::RecordInternalError(
    ConnectionToHostInternalError internal_error) {
  switch (internal_error) {
    case ConnectionToHostInternalError::UNKNOWN_ERROR:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);
      break;
    case ConnectionToHostInternalError::CLIENT_CONNECTION_INTERNAL_ERROR:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::
              INTERNAL_ERROR);
      break;
    case ConnectionToHostInternalError::CLIENT_CONNECTION_TIMEOUT:
      RecordConnectionResultFailureClientConnection(
          ConnectionToHostResult_FailureClientConnectionEventType::TIMEOUT);
      break;
    case ConnectionToHostInternalError::
        TETHERING_TIMED_OUT_FIRST_TIME_SETUP_REQUIRED:
      RecordConnectionResultFailureTetheringTimeout(
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_REQUIRED);
      break;
    case ConnectionToHostInternalError::
        TETHERING_TIMED_OUT_FIRST_TIME_SETUP_NOT_REQUIRED:
      RecordConnectionResultFailureTetheringTimeout(
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_NOT_REQUIRED);
      break;
    case ConnectionToHostInternalError::ENABLING_HOTSPOT_FAILED:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::ENABLING_HOTSPOT_FAILED);
      break;
    case ConnectionToHostInternalError::ENABLING_HOTSPOT_TIMEOUT:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::ENABLING_HOTSPOT_TIMEOUT);
      break;
    case ConnectionToHostInternalError::NO_RESPONSE:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::NO_RESPONSE);
      break;
    case ConnectionToHostInternalError::INVALID_HOTSPOT_CREDENTIALS:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::INVALID_HOTSPOT_CREDENTIALS);
      break;
    case ConnectionToHostInternalError::SUCCESSFUL_REQUEST_BUT_NO_RESPONSE:
      RecordConnectionResultFailure(ConnectionToHostResult_FailureEventType::
                                        SUCCESSFUL_REQUEST_BUT_NO_RESPONSE);
      break;
    case ConnectionToHostInternalError::UNRECOGNIZED_RESPONSE_ERROR:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::UNRECOGNIZED_RESPONSE_ERROR);
      break;
    case ConnectionToHostInternalError::INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG:
      RecordConnectionResultFailure(ConnectionToHostResult_FailureEventType::
                                        INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG);
      break;
    case ConnectionToHostInternalError::INVALID_NEW_SOFT_AP_CONFIG:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::INVALID_NEW_SOFT_AP_CONFIG);
      break;
    case ConnectionToHostInternalError::INVALID_WIFI_AP_CONFIG:
      RecordConnectionResultFailure(
          ConnectionToHostResult_FailureEventType::INVALID_WIFI_AP_CONFIG);
      break;
  }
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

void HostConnectionMetricsLogger::RecordConnectionResultFailure(
    ConnectionToHostResult_FailureEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.ConnectionToHostResult.Failure", event_type,
      ConnectionToHostResult_FailureEventType::FAILURE_MAX);
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
