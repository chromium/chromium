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

  void VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType event_type) {
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.ConnectionToHostResult.Failure.ClientConnection",
        event_type, 1);
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
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_PROVISIONING_FAILED,
      test_devices_[0].GetDeviceId());

  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::
              PROVISIONING_FAILED);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultSuccess) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      test_devices_[0].GetDeviceId());

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultSuccess_Background_DifferentDevice) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      test_devices_[1].GetDeviceId());

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultFailure) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailure_Background_DifferentDevice) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
      test_devices_[1].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureClientConnection_Timeout) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT,
      test_devices_[0].GetDeviceId());

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
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureClientConnection_CanceledByUser) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
      test_devices_[0].GetDeviceId());

  VerifyFailure_ClientConnection(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_FailureClientConnectionEventType::
              CANCELED_BY_USER);
  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          CLIENT_CONNECTION_ERROR);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureClientConnection_InternalError) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR,
      test_devices_[0].GetDeviceId());

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
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureTetheringTimeout_SetupRequired) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED,
      test_devices_[0].GetDeviceId());

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
}

TEST_F(
    HostConnectionMetricsLoggerTest,
    DISABLED_RecordConnectionResultFailureTetheringTimeout_SetupNotRequired) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED,
      test_devices_[0].GetDeviceId());

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
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureTetheringUnsupported) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_UNSUPPORTED);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureNoCellData) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_CELL_DATA,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::NO_CELL_DATA);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureEnablingHotspotFailed) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          ENABLING_HOTSPOT_FAILED);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureEnablingHotspotTimeout) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          ENABLING_HOTSPOT_TIMEOUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectToHostDuration) {
  VerifyConnectToHostDuration();
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureNoResponse) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_RESPONSE,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::NO_RESPONSE);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

TEST_F(HostConnectionMetricsLoggerTest,
       DISABLED_RecordConnectionResultFailureInvalidHotspotCredentials) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          INVALID_HOTSPOT_CREDENTIALS);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE);
}

}  // namespace tether

}  // namespace ash
