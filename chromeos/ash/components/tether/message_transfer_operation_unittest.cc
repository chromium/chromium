// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/message_transfer_operation.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/cross_device/timer_factory/fake_one_shot_timer.h"
#include "components/cross_device/timer_factory/fake_timer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

namespace {

// Arbitrarily chosen value. The MessageType used in this test does not matter
// except that it must be consistent throughout the test.
const MessageType kTestMessageType = MessageType::TETHER_AVAILABILITY_REQUEST;

const uint32_t kTestTimeoutSeconds = 5;

const char kTetherFeature[] = "magic_tether";

// A test double for MessageTransferOperation is needed because
// MessageTransferOperation has pure virtual methods which must be overridden in
// order to create a concrete instantiation of the class.
class TestOperation : public MessageTransferOperation {
 public:
  TestOperation(const multidevice::RemoteDeviceRef& device_to_connect,
                device_sync::DeviceSyncClient* device_sync_client,
                secure_channel::SecureChannelClient* secure_channel_client)
      : MessageTransferOperation(TetherHost(device_to_connect),
                                 secure_channel::ConnectionPriority::kLow,
                                 device_sync_client,
                                 secure_channel_client) {}
  ~TestOperation() override = default;

  // MessageTransferOperation:
  void OnDeviceAuthenticated() override { has_device_authenticated_ = true; }

  void OnMessageReceived(
      std::unique_ptr<MessageWrapper> message_wrapper) override {
    received_messages_.push_back(std::move(message_wrapper));

    if (should_stop_operation_on_message_received_) {
      StopOperation();
    }
  }

  void OnOperationStarted() override { has_operation_started_ = true; }

  void OnOperationFinished() override { has_operation_finished_ = true; }

  MessageType GetMessageTypeForConnection() override {
    return kTestMessageType;
  }

  void OnMessageSent(int sequence_number) override {
    last_sequence_number_ = sequence_number;
  }

  uint32_t GetMessageTimeoutSeconds() override { return timeout_seconds_; }

  void set_timeout_seconds(uint32_t timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
  }

  void set_should_stop_operation_on_message_received(
      bool should_stop_operation_on_message_received) {
    should_stop_operation_on_message_received_ =
        should_stop_operation_on_message_received;
  }

  bool has_device_authenticated() { return has_device_authenticated_; }

  bool has_operation_started() { return has_operation_started_; }

  bool has_operation_finished() { return has_operation_finished_; }

  std::optional<int> last_sequence_number() { return last_sequence_number_; }

  const std::vector<std::unique_ptr<MessageWrapper>>& get_received_messages() {
    return received_messages_;
  }

  secure_channel::FakeConnectionAttempt* get_connection_attempt() {
    return static_cast<secure_channel::FakeConnectionAttempt*>(
        connection_attempt_.get());
  }

  secure_channel::FakeClientChannel* get_client_channel() {
    return static_cast<secure_channel::FakeClientChannel*>(
        client_channel_.get());
  }

 private:
  bool has_device_authenticated_ = false;
  std::vector<std::unique_ptr<MessageWrapper>> received_messages_;

  uint32_t timeout_seconds_ = kTestTimeoutSeconds;
  bool should_stop_operation_on_message_received_ = false;
  bool has_operation_started_ = false;
  bool has_operation_finished_ = false;
  std::optional<int> last_sequence_number_;
};

TetherAvailabilityResponse CreateTetherAvailabilityResponse() {
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());
  return response;
}

}  // namespace

class MessageTransferOperationTest : public testing::Test {
 public:
  MessageTransferOperationTest(const MessageTransferOperationTest&) = delete;
  MessageTransferOperationTest& operator=(const MessageTransferOperationTest&) =
      delete;

 protected:
  MessageTransferOperationTest()
      : test_local_device_(multidevice::RemoteDeviceRefBuilder()
                               .SetPublicKey("local device")
                               .Build()),
        test_device_(multidevice::CreateRemoteDeviceRefForTest()) {}

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    // Prepare for connection timeout timers to be made for the remote
    // device.
    fake_secure_channel_client_->set_next_listen_connection_attempt(
        test_device_, test_local_device_,
        std::make_unique<secure_channel::FakeConnectionAttempt>());

    operation_ = base::WrapUnique(
        new TestOperation(test_device_, fake_device_sync_client_.get(),
                          fake_secure_channel_client_.get()));
    operation_->SetTimerFactoryForTest(
        std::make_unique<cross_device::FakeTimerFactory>());
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
    operation_->Initialize();

    for (const auto* arguments :
         fake_secure_channel_client_
             ->last_listen_for_connection_request_arguments_list()) {
      EXPECT_EQ(kTetherFeature, arguments->feature);
    }

    VerifyOperationStartedAndFinished(true /* has_started */,
                                      false /* has_finished */);

    VerifyConnectionTimerCreated();
  }

  void VerifyOperationStartedAndFinished(bool has_started, bool has_finished) {
    EXPECT_EQ(has_started, operation_->has_operation_started());
    EXPECT_EQ(has_finished, operation_->has_operation_finished());
  }

  void CreateAuthenticatedChannel() {
    operation_->get_connection_attempt()->NotifyConnection(
        std::make_unique<secure_channel::FakeClientChannel>());
  }

  cross_device::FakeOneShotTimer* GetOperationTimer() {
    return static_cast<cross_device::FakeOneShotTimer*>(
        operation_->remote_device_timer_.get());
  }

  void VerifyDefaultTimerCreated() { VerifyTimerCreated(kTestTimeoutSeconds); }

  void VerifyConnectionTimerCreated() {
    VerifyTimerCreated(MessageTransferOperation::kConnectionTimeoutSeconds);
  }

  void VerifyTimerCreated(uint32_t timeout_seconds) {
    EXPECT_TRUE(GetOperationTimer());
    EXPECT_EQ(base::Seconds(timeout_seconds),
              GetOperationTimer()->GetCurrentDelay());
  }

  int SendMessageToDevice(std::unique_ptr<MessageWrapper> message_wrapper) {
    return operation_->SendMessageToDevice(std::move(message_wrapper));
  }

  const multidevice::RemoteDeviceRef test_local_device_;
  const multidevice::RemoteDeviceRef test_device_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<TestOperation> operation_;
};

TEST_F(MessageTransferOperationTest, TestFailedConnection) {
  operation_->get_connection_attempt()->NotifyConnectionAttemptFailure(
      secure_channel::mojom::ConnectionAttemptFailureReason::
          AUTHENTICATION_ERROR);

  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);
  EXPECT_FALSE(operation_->has_device_authenticated());
  EXPECT_TRUE(operation_->get_received_messages().empty());
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionSendAndReceiveMessage) {
  // Simulate how subclasses behave after a successful response: unregister the
  // device.
  operation_->set_should_stop_operation_on_message_received(true);

  CreateAuthenticatedChannel();
  EXPECT_TRUE(operation_->has_device_authenticated());
  VerifyDefaultTimerCreated();

  auto message_wrapper =
      std::make_unique<MessageWrapper>(TetherAvailabilityRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  int sequence_number = SendMessageToDevice(std::move(message_wrapper));
  std::vector<std::pair<std::string, base::OnceClosure>>& sent_messages =
      operation_->get_client_channel()->sent_messages();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);

  EXPECT_FALSE(operation_->last_sequence_number());
  std::move(sent_messages[0].second).Run();
  EXPECT_EQ(sequence_number, operation_->last_sequence_number());

  operation_->get_client_channel()->NotifyMessageReceived(
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->get_received_messages().size());
  const auto& message = operation_->get_received_messages()[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestTimesOutBeforeAuthentication) {
  GetOperationTimer()->Fire();
  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest, TestAuthenticatesButThenTimesOut) {
  CreateAuthenticatedChannel();
  EXPECT_TRUE(operation_->has_device_authenticated());
  VerifyDefaultTimerCreated();

  GetOperationTimer()->Fire();

  EXPECT_TRUE(operation_->has_operation_finished());
}

}  // namespace ash::tether
