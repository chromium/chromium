// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/connect_tethering_operation.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace ash::tether {

namespace {

constexpr base::TimeDelta kConnectTetheringResponseTimeSeconds =
    base::Seconds(15);

// Used to verify the ConnectTetheringOperation notifies the observer
// when appropriate.
class MockOperationObserver : public ConnectTetheringOperation::Observer {
 public:
  MockOperationObserver() = default;

  MockOperationObserver(const MockOperationObserver&) = delete;
  MockOperationObserver& operator=(const MockOperationObserver&) = delete;

  ~MockOperationObserver() = default;

  MOCK_METHOD0(OnConnectTetheringRequestSent, void());
  MOCK_METHOD2(OnSuccessfulConnectTetheringResponse,
               void(const std::string&, const std::string&));
  MOCK_METHOD1(OnConnectTetheringFailure,
               void(ConnectTetheringOperation::HostResponseErrorCode));
};

}  // namespace

class ConnectTetheringOperationTest : public testing::Test {
 protected:
  ConnectTetheringOperationTest()
      : tether_host_(TetherHost(multidevice::CreateRemoteDeviceRefForTest())) {}

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();

    operation_ = base::WrapUnique(new ConnectTetheringOperation(
        tether_host_, fake_host_connection_factory_.get(),
        /*setup_required=*/false));

    operation_->SetTimerFactoryForTest(
        std::make_unique<ash::timer_factory::FakeTimerFactory>());
    operation_->AddObserver(&mock_observer_);

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation_->SetClockForTest(&test_clock_);

    fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);
  }

  const TetherHost tether_host_;

  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;
  base::SimpleTestClock test_clock_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ConnectTetheringOperation> operation_;
};

TEST_F(ConnectTetheringOperationTest, SuccessWithValidResponse) {
  static const std::string kTestSsid = "testSsid";
  static const std::string kTestPassword = "testPassword";

  // Verify that the Observer is called with success and the correct parameters.
  EXPECT_CALL(mock_observer_,
              OnSuccessfulConnectTetheringResponse(kTestSsid, kTestPassword));

  operation_->Initialize();

  // Advance the clock in order to verify a non-zero response duration is
  // recorded and verified (below).
  test_clock_.Advance(kConnectTetheringResponseTimeSeconds);

  // The ConnectTetheringResponse message contains the success response code and
  // the required SSID and password parameters.
  ConnectTetheringResponse response;
  response.set_response_code(ConnectTetheringResponse_ResponseCode::
                                 ConnectTetheringResponse_ResponseCode_SUCCESS);
  response.set_ssid(kTestSsid);
  response.set_password(kTestPassword);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));

  // Verify the response duration metric is recorded.
  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.ConnectTetheringResponseDuration",
      kConnectTetheringResponseTimeSeconds, 1);
}

// Tests that the SSID and password parameters are a required parameters of the
// success response code; failure to provide these parameters results in a
// failed tethering connection.
TEST_F(ConnectTetheringOperationTest, SuccessButInvalidResponse) {
  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(mock_observer_,
              OnConnectTetheringFailure(
                  ConnectTetheringOperation::HostResponseErrorCode::
                      INVALID_HOTSPOT_CREDENTIALS));

  operation_->Initialize();
  // The ConnectTetheringResponse message does not contain the required SSID and
  // password fields.
  ConnectTetheringResponse response;
  response.set_response_code(ConnectTetheringResponse_ResponseCode::
                                 ConnectTetheringResponse_ResponseCode_SUCCESS);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));
}

TEST_F(ConnectTetheringOperationTest, UnknownError) {
  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(
      mock_observer_,
      OnConnectTetheringFailure(
          ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR));

  operation_->Initialize();
  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));
}

TEST_F(ConnectTetheringOperationTest, ProvisioningFailed) {
  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(mock_observer_,
              OnConnectTetheringFailure(
                  ConnectTetheringOperation::HostResponseErrorCode::
                      PROVISIONING_FAILED));

  operation_->Initialize();
  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_PROVISIONING_FAILED);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));
}

TEST_F(ConnectTetheringOperationTest, InvalidWifiApConfig) {
  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(mock_observer_,
              OnConnectTetheringFailure(
                  ConnectTetheringOperation::HostResponseErrorCode::
                      INVALID_WIFI_AP_CONFIG));

  operation_->Initialize();
  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_INVALID_WIFI_AP_CONFIG);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));
}

TEST_F(ConnectTetheringOperationTest, InvalidActiveExistingSoftApConfig) {
  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(mock_observer_,
              OnConnectTetheringFailure(
                  ConnectTetheringOperation::HostResponseErrorCode::
                      INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG));

  operation_->Initialize();
  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_INVALID_ACTIVE_EXISTING_SOFT_AP_CONFIG);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));
}

TEST_F(ConnectTetheringOperationTest, InvalidNewSoftApConfig) {
  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(mock_observer_,
              OnConnectTetheringFailure(
                  ConnectTetheringOperation::HostResponseErrorCode::
                      INVALID_NEW_SOFT_AP_CONFIG));

  operation_->Initialize();
  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_INVALID_NEW_SOFT_AP_CONFIG);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message));
}

// Tests that the message timeout value varies based on whether setup is
// required or not.
TEST_F(ConnectTetheringOperationTest, GetMessageTimeoutSeconds) {
  // Setup required case.
  std::unique_ptr<ConnectTetheringOperation> operation(
      new ConnectTetheringOperation(tether_host_,
                                    fake_host_connection_factory_.get(),
                                    true /* setup_required */));

  EXPECT_EQ(ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds,
            operation->GetMessageTimeoutSeconds());

  // Setup not required case.
  operation.reset(new ConnectTetheringOperation(
      tether_host_, fake_host_connection_factory_.get(),
      false /* setup_required */));

  EXPECT_EQ(ConnectTetheringOperation::kSetupNotRequiredResponseTimeoutSeconds,
            operation->GetMessageTimeoutSeconds());
}

// Tests that the ConnectTetheringRequest message is sent to the remote device
// once the communication channel is connected and authenticated.
TEST_F(ConnectTetheringOperationTest, ConnectRequestSentOnceAuthenticated) {
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Connect and authenticate the client channel.
  operation_->Initialize();

  // Verify the ConnectTetheringRequest message is sent.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(ConnectTetheringRequest());
  auto& sent_messages = fake_host_connection_factory_
                            ->GetActiveConnection(tether_host_.GetDeviceId())
                            ->sent_messages();
  std::string expected_payload = message_wrapper->ToRawMessage();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first->ToRawMessage());
}

}  // namespace ash::tether
