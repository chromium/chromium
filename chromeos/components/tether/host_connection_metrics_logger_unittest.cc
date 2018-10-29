// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_connection_metrics_logger.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {
constexpr base::TimeDelta kConnectToHostTime = base::TimeDelta::FromSeconds(13);
const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";
}  // namespace

class HostConnectionMetricsLoggerTest : public testing::Test {
 protected:
  HostConnectionMetricsLoggerTest()
      : test_devices_(cryptauth::CreateRemoteDeviceRefListForTest(2u)) {}

  void SetUp() override {
    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();
    fake_active_host_ = std::make_unique<FakeActiveHost>();

    metrics_logger_ = std::make_unique<HostConnectionMetricsLogger>(
        fake_ble_connection_manager_.get(), fake_active_host_.get());

    test_clock_.SetNow(base::Time::UnixEpoch());
    metrics_logger_->SetClockForTesting(&test_clock_);
  }

  void SetMultiDeviceApiDisabled() {
    scoped_feature_list_.InitAndDisableFeature(features::kMultiDeviceApi);
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
          event_type,
      bool is_background_advertisement) {
    if (is_background_advertisement) {
      histogram_tester_.ExpectUniqueSample(
          "InstantTethering.ConnectionToHostResult.SuccessRate.Background",
          event_type, 1);
    } else {
      histogram_tester_.ExpectUniqueSample(
          "InstantTethering.ConnectionToHostResult.SuccessRate", event_type, 1);
    }
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

  void VerifyConnectToHostDuration(bool is_background_advertisement) {
    std::string device_id = test_devices_[0].GetDeviceId();

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      SetActiveHostToConnecting(device_id);
    } else {
      SetActiveHostToConnectingAndReceiveAdvertisement(
          device_id, is_background_advertisement);
    }

    test_clock_.Advance(kConnectToHostTime);

    fake_active_host_->SetActiveHostConnected(device_id, kTetherNetworkGuid,
                                              kWifiNetworkGuid);

    if (is_background_advertisement) {
      histogram_tester_.ExpectTimeBucketCount(
          "InstantTethering.Performance.ConnectToHostDuration.Background",
          kConnectToHostTime, 1);
    } else {
      histogram_tester_.ExpectTimeBucketCount(
          "InstantTethering.Performance.ConnectToHostDuration",
          kConnectToHostTime, 1);
    }
  }

  void SetActiveHostToConnectingAndReceiveAdvertisement(
      const std::string& device_id,
      bool is_background_advertisement) {
    fake_ble_connection_manager_->NotifyAdvertisementReceived(
        device_id, is_background_advertisement);

    fake_active_host_->SetActiveHostConnecting(device_id, kTetherNetworkGuid);
  }

  void SetActiveHostToConnecting(const std::string& device_id) {
    fake_active_host_->SetActiveHostConnecting(device_id, kTetherNetworkGuid);
  }

  const cryptauth::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<HostConnectionMetricsLogger> metrics_logger_;

  base::HistogramTester histogram_tester_;
  base::SimpleTestClock test_clock_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostConnectionMetricsLoggerTest);
};

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultProvisioningFailure) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

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
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      test_devices_[0].GetDeviceId());

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultSuccess_Background) {
  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), true /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      test_devices_[0].GetDeviceId());

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS,
                true /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultSuccess_MultiDeviceApiEnabled) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      test_devices_[0].GetDeviceId());

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS,
                true /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultSuccess_Background_DifferentDevice) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), true /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_SUCCESS,
      test_devices_[1].GetDeviceId());

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::SUCCESS,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectionResultFailure) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_Background) {
  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), true /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                true /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_MultiDeviceApiEnabled) {
  SetActiveHostToConnecting(test_devices_[0].GetDeviceId());

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                true /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailure_Background_DifferentDevice) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), true /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
      test_devices_[1].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::UNKNOWN_ERROR);

  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_Timeout) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

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
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_CanceledByUser) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

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
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureClientConnection_InternalError) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

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
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringTimeout_SetupRequired) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

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
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringTimeout_SetupNotRequired) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

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
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
  VerifyProvisioningFailure(
      HostConnectionMetricsLogger::
          ConnectionToHostResult_ProvisioningFailureEventType::OTHER);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureTetheringUnsupported) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          TETHERING_UNSUPPORTED);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureNoCellData) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_CELL_DATA,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::NO_CELL_DATA);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureEnablingHotspotFailed) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          ENABLING_HOTSPOT_FAILED);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureEnablingHotspotTimeout) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          ENABLING_HOTSPOT_TIMEOUT);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest, RecordConnectToHostDuration) {
  SetMultiDeviceApiDisabled();

  VerifyConnectToHostDuration(false /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectToHostDuration_Background) {
  VerifyConnectToHostDuration(true /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectToHostDuration_MultiDeviceApiEnabled) {
  VerifyConnectToHostDuration(true /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureNoResponse) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_NO_RESPONSE,
      test_devices_[0].GetDeviceId());

  VerifyFailure(HostConnectionMetricsLogger::
                    ConnectionToHostResult_FailureEventType::NO_RESPONSE);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
}

TEST_F(HostConnectionMetricsLoggerTest,
       RecordConnectionResultFailureInvalidHotspotCredentials) {
  SetMultiDeviceApiDisabled();

  SetActiveHostToConnectingAndReceiveAdvertisement(
      test_devices_[0].GetDeviceId(), false /* is_background_advertisement */);

  metrics_logger_->RecordConnectionToHostResult(
      HostConnectionMetricsLogger::ConnectionToHostResult::
          CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS,
      test_devices_[0].GetDeviceId());

  VerifyFailure(
      HostConnectionMetricsLogger::ConnectionToHostResult_FailureEventType::
          INVALID_HOTSPOT_CREDENTIALS);
  VerifySuccess(HostConnectionMetricsLogger::
                    ConnectionToHostResult_SuccessEventType::FAILURE,
                false /* is_background_advertisement */);
}

}  // namespace tether

}  // namespace chromeos
