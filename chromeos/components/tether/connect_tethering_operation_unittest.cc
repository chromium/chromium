// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connect_tethering_operation.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

const char kTestSsid[] = "testSsid";
const char kTestPassword[] = "testPassword";

constexpr base::TimeDelta kConnectTetheringResponseTime =
    base::TimeDelta::FromSeconds(15);

class TestObserver final : public ConnectTetheringOperation::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() = default;

  base::Optional<cryptauth::RemoteDeviceRef> remote_device() {
    return remote_device_;
  }
  const std::string& ssid() { return ssid_; }
  const std::string& password() { return password_; }
  bool has_received_failure() { return has_received_failure_; }
  bool has_sent_request() { return has_sent_request_; }
  ConnectTetheringOperation::HostResponseErrorCode error_code() {
    return error_code_;
  }

  // ConnectTetheringOperation::Observer:
  void OnConnectTetheringRequestSent(
      cryptauth::RemoteDeviceRef remote_device) override {
    has_sent_request_ = true;
  }

  void OnSuccessfulConnectTetheringResponse(
      cryptauth::RemoteDeviceRef remote_device,
      const std::string& ssid,
      const std::string& password) override {
    remote_device_ = remote_device;
    ssid_ = ssid;
    password_ = password;
  }

  void OnConnectTetheringFailure(
      cryptauth::RemoteDeviceRef remote_device,
      ConnectTetheringOperation::HostResponseErrorCode error_code) override {
    has_received_failure_ = true;
    remote_device_ = remote_device;
    error_code_ = error_code;
  }

 private:
  base::Optional<cryptauth::RemoteDeviceRef> remote_device_;
  std::string ssid_;
  std::string password_;
  bool has_received_failure_ = false;
  bool has_sent_request_ = false;
  ConnectTetheringOperation::HostResponseErrorCode error_code_;
};

std::string CreateConnectTetheringRequestString() {
  ConnectTetheringRequest request;
  return MessageWrapper(request).ToRawMessage();
}

std::string CreateConnectTetheringResponseString(
    ConnectTetheringResponse_ResponseCode response_code,
    bool use_proto_without_ssid_and_password) {
  ConnectTetheringResponse response;
  response.set_response_code(response_code);

  // Only set SSID/password if |response_code| is SUCCESS.
  if (!use_proto_without_ssid_and_password &&
      response_code == ConnectTetheringResponse_ResponseCode::
                           ConnectTetheringResponse_ResponseCode_SUCCESS) {
    response.set_ssid(std::string(kTestSsid));
    response.set_password(std::string(kTestPassword));
  }

  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());

  return MessageWrapper(response).ToRawMessage();
}

}  // namespace

class ConnectTetheringOperationTest : public testing::Test {
 protected:
  ConnectTetheringOperationTest()
      : connect_tethering_request_string_(
            CreateConnectTetheringRequestString()),
        test_device_(cryptauth::CreateRemoteDeviceRefListForTest(1)[0]) {}

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(features::kMultiDeviceApi);

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();
    mock_tether_host_response_recorder_ =
        std::make_unique<StrictMock<MockTetherHostResponseRecorder>>();
    test_observer_ = base::WrapUnique(new TestObserver());

    operation_ = base::WrapUnique(new ConnectTetheringOperation(
        test_device_, fake_device_sync_client_.get(),
        fake_secure_channel_client_.get(), fake_ble_connection_manager_.get(),
        mock_tether_host_response_recorder_.get(), false /* setup_required */));
    operation_->AddObserver(test_observer_.get());

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation_->SetClockForTest(&test_clock_);

    operation_->Initialize();
  }

  void SimulateDeviceAuthenticationAndVerifyMessageSent() {
    VerifyResponseTimeoutSeconds(false /* setup_required */);

    operation_->OnDeviceAuthenticated(test_device_);

    // Verify that the message was sent successfully.
    std::vector<FakeBleConnectionManager::SentMessage>& sent_messages =
        fake_ble_connection_manager_->sent_messages();
    ASSERT_EQ(1u, sent_messages.size());
    EXPECT_EQ(test_device_.GetDeviceId(), sent_messages[0].device_id);
    EXPECT_EQ(connect_tethering_request_string_, sent_messages[0].message);

    // Simulate BleConnectionManager notifying ConnectTetheringOperation that
    // the message was delivered.
    int last_sequence_number =
        fake_ble_connection_manager_->last_sequence_number();
    EXPECT_NE(last_sequence_number, -1);
    fake_ble_connection_manager_->SetMessageSent(last_sequence_number);
    EXPECT_TRUE(test_observer_->has_sent_request());
  }

  void SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode response_code,
      ConnectTetheringOperation::HostResponseErrorCode expected_error_code,
      bool use_proto_without_ssid_and_password) {
    test_clock_.Advance(kConnectTetheringResponseTime);

    fake_ble_connection_manager_->ReceiveMessage(
        test_device_.GetDeviceId(),
        CreateConnectTetheringResponseString(
            response_code, use_proto_without_ssid_and_password));

    bool is_success_response =
        response_code == ConnectTetheringResponse_ResponseCode::
                             ConnectTetheringResponse_ResponseCode_SUCCESS;
    ConnectTetheringResponse_ResponseCode expected_response_code;
    if (is_success_response && use_proto_without_ssid_and_password) {
      expected_response_code = ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR;
    } else if (is_success_response && !use_proto_without_ssid_and_password) {
      expected_response_code = ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS;
    } else {
      expected_response_code = response_code;
    }

    if (expected_response_code ==
        ConnectTetheringResponse_ResponseCode::
            ConnectTetheringResponse_ResponseCode_SUCCESS) {
      EXPECT_EQ(test_device_, test_observer_->remote_device());
      EXPECT_EQ(std::string(kTestSsid), test_observer_->ssid());
      EXPECT_EQ(std::string(kTestPassword), test_observer_->password());
    } else {
      EXPECT_TRUE(test_observer_->has_received_failure());
      EXPECT_EQ(expected_error_code, test_observer_->error_code());
    }

    histogram_tester_.ExpectTimeBucketCount(
        "InstantTethering.Performance.ConnectTetheringResponseDuration",
        kConnectTetheringResponseTime, 1);
  }

  void VerifyResponseTimeoutSeconds(bool setup_required) {
    uint32_t expected_response_timeout_seconds =
        setup_required
            ? ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds
            : ConnectTetheringOperation::
                  kSetupNotRequiredResponseTimeoutSeconds;

    EXPECT_EQ(expected_response_timeout_seconds,
              operation_->GetMessageTimeoutSeconds());
  }

  const std::string connect_tethering_request_string_;
  const cryptauth::RemoteDeviceRef test_device_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::SecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<StrictMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  std::unique_ptr<TestObserver> test_observer_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<ConnectTetheringOperation> operation_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectTetheringOperationTest);
};

TEST_F(ConnectTetheringOperationTest, TestOperation_SuccessButInvalidResponse) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS,
      ConnectTetheringOperation::HostResponseErrorCode::
          INVALID_HOTSPOT_CREDENTIALS,
      true /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_SuccessWithValidResponse) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(test_device_));

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS,
      ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_UnknownError) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR,
      ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_ProvisioningFailed) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_PROVISIONING_FAILED,
      ConnectTetheringOperation::HostResponseErrorCode::PROVISIONING_FAILED,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestCannotConnect) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  // Simulate the device failing to connect.
  fake_ble_connection_manager_->SimulateUnansweredConnectionAttempts(
      test_device_.GetDeviceId(),
      MessageTransferOperation::kMaxEmptyScansPerDevice);

  // The maximum number of connection failures has occurred.
  EXPECT_TRUE(test_observer_->has_received_failure());
  EXPECT_EQ(ConnectTetheringOperation::HostResponseErrorCode::NO_RESPONSE,
            test_observer_->error_code());

  histogram_tester_.ExpectTotalCount(
      "InstantTethering.Performance.ConnectTetheringResponseDuration", 0);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_SetupRequired) {
  operation_ = base::WrapUnique(new ConnectTetheringOperation(
      test_device_, fake_device_sync_client_.get(),
      fake_secure_channel_client_.get(), fake_ble_connection_manager_.get(),
      mock_tether_host_response_recorder_.get(), true /* setup_required */));
  VerifyResponseTimeoutSeconds(true /* setup_required */);
}

}  // namespace tether

}  // namespace chromeos
