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
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/components/tether/test_timer_factory.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

constexpr base::TimeDelta kConnectTetheringResponseTimeSeconds =
    base::TimeDelta::FromSeconds(15);

// Used to verify the ConnectTetheringOperation notifies the observer
// when appropriate.
class MockOperationObserver : public ConnectTetheringOperation::Observer {
 public:
  MockOperationObserver() = default;
  ~MockOperationObserver() = default;

  MOCK_METHOD1(OnConnectTetheringRequestSent,
               void(multidevice::RemoteDeviceRef));
  MOCK_METHOD3(OnSuccessfulConnectTetheringResponse,
               void(multidevice::RemoteDeviceRef,
                    const std::string&,
                    const std::string&));
  MOCK_METHOD2(OnConnectTetheringFailure,
               void(multidevice::RemoteDeviceRef,
                    ConnectTetheringOperation::HostResponseErrorCode));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOperationObserver);
};

}  // namespace

class ConnectTetheringOperationTest : public testing::Test {
 protected:
  ConnectTetheringOperationTest()
      : test_local_device_(multidevice::RemoteDeviceRefBuilder()
                               .SetPublicKey("local device")
                               .Build()),
        remote_device_(multidevice::CreateRemoteDeviceRefForTest()) {}

  void SetUp() override {
    mock_tether_host_response_recorder_ =
        std::make_unique<StrictMock<MockTetherHostResponseRecorder>>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    operation_ = ConstructOperation();
    operation_->Initialize();

    ConnectAuthenticatedChannelForDevice(remote_device_);
  }

  std::unique_ptr<ConnectTetheringOperation> ConstructOperation() {
    std::unique_ptr<ConnectTetheringOperation> operation;
    test_timer_factory_ = new TestTimerFactory();

    // Prepare the connection timeout timer to be made for the remote device.
    test_timer_factory_->set_device_id_for_next_timer(
        remote_device_.GetDeviceId());

    auto fake_connection_attempt =
        std::make_unique<secure_channel::FakeConnectionAttempt>();
    remote_device_to_fake_connection_attempt_map_[remote_device_] =
        fake_connection_attempt.get();
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        remote_device_, test_local_device_, std::move(fake_connection_attempt));

    operation = base::WrapUnique(new ConnectTetheringOperation(
        remote_device_, fake_device_sync_client_.get(),
        fake_secure_channel_client_.get(),
        mock_tether_host_response_recorder_.get(), false /* setup_required */));
    operation->SetTimerFactoryForTest(base::WrapUnique(test_timer_factory_));
    operation->AddObserver(&mock_observer_);

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation->SetClockForTest(&test_clock_);

    return operation;
  }

  void ConnectAuthenticatedChannelForDevice(
      multidevice::RemoteDeviceRef remote_device) {
    auto fake_client_channel =
        std::make_unique<secure_channel::FakeClientChannel>();
    remote_device_to_fake_client_channel_map_[remote_device] =
        fake_client_channel.get();
    remote_device_to_fake_connection_attempt_map_[remote_device]
        ->NotifyConnection(std::move(fake_client_channel));
  }

  const multidevice::RemoteDeviceRef test_local_device_;
  const multidevice::RemoteDeviceRef remote_device_;

  base::flat_map<multidevice::RemoteDeviceRef,
                 secure_channel::FakeConnectionAttempt*>
      remote_device_to_fake_connection_attempt_map_;
  base::flat_map<multidevice::RemoteDeviceRef,
                 secure_channel::FakeClientChannel*>
      remote_device_to_fake_client_channel_map_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<StrictMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  base::SimpleTestClock test_clock_;
  TestTimerFactory* test_timer_factory_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ConnectTetheringOperation> operation_;
};

TEST_F(ConnectTetheringOperationTest, SuccessWithValidResponse) {
  static const std::string kTestSsid = "testSsid";
  static const std::string kTestPassword = "testPassword";

  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(remote_device_));

  // Verify that the Observer is called with success and the correct parameters.
  EXPECT_CALL(mock_observer_, OnSuccessfulConnectTetheringResponse(
                                  remote_device_, kTestSsid, kTestPassword));

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

  operation_->OnMessageReceived(std::move(message), remote_device_);

  // Verify the response duration metric is recorded.
  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.ConnectTetheringResponseDuration",
      kConnectTetheringResponseTimeSeconds, 1);
}

// Tests that the SSID and password parameters are a required parameters of the
// success response code; failure to provide these parameters results in a
// failed tethering connection.
TEST_F(ConnectTetheringOperationTest, SuccessButInvalidResponse) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(
      mock_observer_,
      OnConnectTetheringFailure(
          remote_device_, ConnectTetheringOperation::HostResponseErrorCode::
                              INVALID_HOTSPOT_CREDENTIALS));

  // The ConnectTetheringResponse message does not contain the required SSID and
  // password fields.
  ConnectTetheringResponse response;
  response.set_response_code(ConnectTetheringResponse_ResponseCode::
                                 ConnectTetheringResponse_ResponseCode_SUCCESS);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message), remote_device_);
}

TEST_F(ConnectTetheringOperationTest, UnknownError) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(
      mock_observer_,
      OnConnectTetheringFailure(
          remote_device_,
          ConnectTetheringOperation::HostResponseErrorCode::UNKNOWN_ERROR));

  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message), remote_device_);
}

TEST_F(ConnectTetheringOperationTest, ProvisioningFailed) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  // Verify that the observer is called with failure and the appropriate error
  // code.
  EXPECT_CALL(
      mock_observer_,
      OnConnectTetheringFailure(
          remote_device_, ConnectTetheringOperation::HostResponseErrorCode::
                              PROVISIONING_FAILED));

  ConnectTetheringResponse response;
  response.set_response_code(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_PROVISIONING_FAILED);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));

  operation_->OnMessageReceived(std::move(message), remote_device_);
}

// Tests that observers are notified when the connection request is sent.
TEST_F(ConnectTetheringOperationTest, NotifyConnectTetheringRequest) {
  EXPECT_CALL(mock_observer_, OnConnectTetheringRequestSent(remote_device_));

  operation_->OnMessageSent(0 /* sequence_number */);
}

// Tests that the message timeout value varies based on whether setup is
// required or not.
TEST_F(ConnectTetheringOperationTest, GetMessageTimeoutSeconds) {
  // Setup required case.
  std::unique_ptr<ConnectTetheringOperation> operation(
      new ConnectTetheringOperation(remote_device_,
                                    fake_device_sync_client_.get(),
                                    fake_secure_channel_client_.get(),
                                    mock_tether_host_response_recorder_.get(),
                                    true /* setup_required */));

  EXPECT_EQ(ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds,
            operation->GetMessageTimeoutSeconds());

  // Setup not required case.
  operation.reset(new ConnectTetheringOperation(
      remote_device_, fake_device_sync_client_.get(),
      fake_secure_channel_client_.get(),
      mock_tether_host_response_recorder_.get(), false /* setup_required */));

  EXPECT_EQ(ConnectTetheringOperation::kSetupNotRequiredResponseTimeoutSeconds,
            operation->GetMessageTimeoutSeconds());
}

// Tests that the ConnectTetheringRequest message is sent to the remote device
// once the communication channel is connected and authenticated.
TEST_F(ConnectTetheringOperationTest, ConnectRequestSentOnceAuthenticated) {
  std::unique_ptr<ConnectTetheringOperation> operation = ConstructOperation();
  operation->Initialize();

  // Create the client channel to the remote device.
  auto fake_client_channel =
      std::make_unique<secure_channel::FakeClientChannel>();
  remote_device_to_fake_client_channel_map_[remote_device_] =
      fake_client_channel.get();

  // No requests as a result of creating the client channel.
  auto& sent_messages = fake_client_channel->sent_messages();
  EXPECT_EQ(0u, sent_messages.size());

  // Connect and authenticate the client channel.
  remote_device_to_fake_connection_attempt_map_[remote_device_]
      ->NotifyConnection(std::move(fake_client_channel));

  // Verify the ConnectTetheringRequest message is sent.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(ConnectTetheringRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);
}

}  // namespace tether

}  // namespace chromeos
