// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_connection_metrics_logger.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {
constexpr base::TimeDelta kConnectToHostTime = base::Seconds(13);
const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";
}  // namespace

using ConnectionToHostResult =
    HostConnectionMetricsLogger::ConnectionToHostResult;
using ConnectionToHostInternalError =
    HostConnectionMetricsLogger::ConnectionToHostInternalError;

class HostConnectionMetricsLoggerTest : public testing::Test {
 public:
  HostConnectionMetricsLoggerTest(const HostConnectionMetricsLoggerTest&) =
      delete;
  HostConnectionMetricsLoggerTest& operator=(
      const HostConnectionMetricsLoggerTest&) = delete;

 protected:
  HostConnectionMetricsLoggerTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(2u)) {}

  void SetUp() override {
    fake_active_host_ = std::make_unique<FakeActiveHost>();

    metrics_logger_ =
        std::make_unique<HostConnectionMetricsLogger>(fake_active_host_.get());

    test_clock_.SetNow(base::Time::UnixEpoch());
    metrics_logger_->SetClockForTesting(&test_clock_);
  }

  void VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.ProvisioningFailureRate",
        event_type, 1);
  }

  void VerifySuccess(
      HostConnectionMetricsLogger::ConnectionToHostResult_SuccessEventType
          event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.SuccessRate.Background",
        event_type, 1);
  }

  void VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType
          event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure", event_type, 1);
  }

  void VerifyEndResult(ConnectionToHostResult result) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.EndResult", result, 1);
  }

  void VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure.ClientConnection",
        event_type, 1);
  }

  void VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.UnavoidableError", event_type,
        1);
  }

  void VerifyFailure_TetheringTimeout(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureTetheringTimeoutEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure.TetheringTimeout",
        event_type, 1);
  }

  void VerifyConnectToHostDuration() {
    std::string device_id = test_devices_[0].GetDeviceId();
    SetActiveHostToConnecting(device_id);

    test_clock_.Advance(kConnectToHostTime);

    fake_active_host_->SetActiveHostConnected(device_id, kTetherNetworkGuid,
                                              kWifiNetworkGuid);

    histogram_tester_.ExpectTimeBucketCount(
        "InstantTethering.Performance.ConnectToHostDuration.Background",
        kConnectToHostTime, 1);
  }

  void SetActiveHostToConnecting(const std::string& device_id) {
    fake_active_host_->SetActiveHostConnecting(device_id, kTetherNetworkGuid);
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<HostConnectionMetricsLogger> metrics_logger_;

  base::HistogramTester histogram_tester_;
  base::SimpleTestClock test_clock_;
};

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultProvisioningFailure) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::PROVISIONING_FAILURE,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::
              PROVISIONING_FAILED);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::
              PROVISIONING_FAILED);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultSuccess) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::SUCCESS,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::SUCCESS);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultSuccess_Background_DifferentDevice) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::SUCCESS,
      test_devices_[1].GetDeviceId(), std::nullopt);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::SUCCESS);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultFailure) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::UNKNOWN_ERROR);

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_Background_DifferentDevice) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[1].GetDeviceId(),
      ConnectionToHostInternalError::UNKNOWN_ERROR);

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(
    HostConnectionMetricsLoggerTest,
    RecordConnectionResultFailureClientConnection_NetworkConnectionHandlerFailed) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::
          CLIENT_CONNECTION_NETWORK_CONNECTION_HANDLER_FAILED);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              NETWORK_CONNECTION_HANDLER_FAILED);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_NetworkStateWasNull) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::CLIENT_CONNECTION_NETWORK_STATE_WAS_NULL);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              NETWORK_STATE_WAS_NULL);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_Timeout) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::CLIENT_CONNECTION_TIMEOUT);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::TIMEOUT);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_InvalidWifiApConfig) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::INVALID_WIFI_AP_CONFIG);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          INVALID_WIFI_AP_CONFIG);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_InvalidActiveExistingSoftApConfig) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_InvalidNewSoftApConfig) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::INVALID_NEW_SOFT_AP_CONFIG);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          INVALID_NEW_SOFT_AP_CONFIG);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureClientConnection_CanceledByUser) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::USER_CANCELLATION,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::USER_CANCELLATION);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::USER_CANCELLATION);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_WifiFailedToEnable) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::CLIENT_CONNECTION_WIFI_FAILED_TO_ENABLE);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              WIFI_FAILED_TO_ENABLED);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_InternalError) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::CLIENT_CONNECTION_INTERNAL_ERROR);

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              INTERNAL_ERROR);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringTimeout_SetupRequired) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::
          TETHERING_TIMED_OUT_FIRST_TIME_SETUP_REQUIRED);

  VerifyFailure_TetheringTimeout(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_REQUIRED);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_TIMED_OUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringTimeout_SetupNotRequired) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::
          TETHERING_TIMED_OUT_FIRST_TIME_SETUP_NOT_REQUIRED);

  VerifyFailure_TetheringTimeout(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureTetheringTimeoutEventType::
              FIRST_TIME_SETUP_WAS_NOT_REQUIRED);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_TIMED_OUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringUnsupported) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          TETHERING_UNSUPPORTED,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifyEndResult(ConnectionToHostResult::TETHERING_UNSUPPORTED);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::
              TETHERING_UNSUPPORTED);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureNoCellData) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::NO_CELLULAR_DATA,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifyEndResult(ConnectionToHostResult::NO_CELLULAR_DATA);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::NO_CELLULAR_DATA);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureShutDownDuringConnectionAttempt) {
  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          TETHER_SHUTDOWN_DURING_CONNECTION,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifyEndResult(ConnectionToHostResult::TETHER_SHUTDOWN_DURING_CONNECTION);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::
              SHUT_DOWN_DURING_CONNECTION);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureCancelledForNewerConnection) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CANCELLED_FOR_NEWER_CONNECTION,
      test_devices_[0].GetDeviceId(), std::nullopt);

  VerifyEndResult(ConnectionToHostResult::CANCELLED_FOR_NEWER_CONNECTION);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::
              CANCELLED_FOR_NEWER_CONNECTION_ATTEMPT);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureEnablingHotspotFailed) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::ENABLING_HOTSPOT_FAILED);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          ENABLING_HOTSPOT_FAILED);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureEnablingHotspotTimeout) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::ENABLING_HOTSPOT_TIMEOUT);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          ENABLING_HOTSPOT_TIMEOUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectToHostDuration) {
  VerifyConnectToHostDuration();
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureNoResponse) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::NO_RESPONSE);

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::NO_RESPONSE);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureInvalidHotspotCredentials) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::INTERNAL_ERROR,
      test_devices_[0].GetDeviceId(),
      ConnectionToHostInternalError::INVALID_HOTSPOT_CREDENTIALS);

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          INVALID_HOTSPOT_CREDENTIALS);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyEndResult(ConnectionToHostResult::INTERNAL_ERROR);
  VerifyUnavoidableErrorResult(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_UnavoidableErrorEventType::OTHER);
}

}  // namespace tether

}  // namespace ash
