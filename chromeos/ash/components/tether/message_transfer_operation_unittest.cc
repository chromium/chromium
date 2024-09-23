// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/message_transfer_operation.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/components/timer_factory/fake_one_shot_timer.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

namespace {

// Arbitrarily chosen value. The MessageType used in this test does not matter
// except that it must be consistent throughout the test.
const MessageType kTestMessageType = MessageType::TETHER_AVAILABILITY_REQUEST;

const uint32_t kTestTimeoutSeconds = 5;

// A test double for MessageTransferOperation is needed because
// MessageTransferOperation has pure virtual methods which must be overridden in
// order to create a concrete instantiation of the class.
class TestOperation : public MessageTransferOperation {
 public:
  TestOperation(const TetherHost& tether_host,
                raw_ptr<HostConnection::Factory> host_connection_factory)
      : MessageTransferOperation(
            tether_host,
            HostConnection::Factory::ConnectionPriority::kLow,
            host_connection_factory) {}

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

  const std::vector<std::unique_ptr<MessageWrapper>>& get_received_messages() {
    return received_messages_;
  }

 private:
  bool has_device_authenticated_ = false;
  std::vector<std::unique_ptr<MessageWrapper>> received_messages_;

  uint32_t timeout_seconds_ = kTestTimeoutSeconds;
  bool should_stop_operation_on_message_received_ = false;
  bool has_operation_started_ = false;
  bool has_operation_finished_ = false;
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
      : tether_host_(TetherHost(multidevice::CreateRemoteDeviceRefForTest())) {}

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();
    operation_ = std::make_unique<TestOperation>(
        tether_host_, fake_host_connection_factory_.get());
    operation_->SetTimerFactoryForTest(
        std::make_unique<ash::timer_factory::FakeTimerFactory>());
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
  }

  void VerifyOperationStartedAndFinished(bool has_started, bool has_finished) {
    EXPECT_EQ(has_started, operation_->has_operation_started());
    EXPECT_EQ(has_finished, operation_->has_operation_finished());
  }

  ash::timer_factory::FakeOneShotTimer* GetOperationTimer() {
    return static_cast<ash::timer_factory::FakeOneShotTimer*>(
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

  void SendMessageToDevice(std::unique_ptr<MessageWrapper> message_wrapper) {
    return operation_->SendMessage(std::move(message_wrapper),
                                   base::DoNothing());
  }

  const TetherHost tether_host_;

  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;
  std::unique_ptr<TestOperation> operation_;
};

TEST_F(MessageTransferOperationTest, TestFailedConnection) {
  fake_host_connection_factory_->FailConnectionAttempt(tether_host_);

  operation_->Initialize();

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

  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  operation_->Initialize();
  EXPECT_TRUE(operation_->has_device_authenticated());
  VerifyDefaultTimerCreated();

  auto message_wrapper =
      std::make_unique<MessageWrapper>(TetherAvailabilityRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  SendMessageToDevice(std::move(message_wrapper));
  const std::vector<std::pair<std::unique_ptr<MessageWrapper>,
                              base::OnceClosure>>& sent_messages =
      fake_host_connection_factory_
          ->GetActiveConnection(tether_host_.GetDeviceId())
          ->sent_messages();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first->ToRawMessage());

  fake_host_connection_factory_->GetActiveConnection(tether_host_.GetDeviceId())
      ->ReceiveMessage(
          std::make_unique<MessageWrapper>(CreateTetherAvailabilityResponse()));

  EXPECT_EQ(1u, operation_->get_received_messages().size());
  const auto& message = operation_->get_received_messages()[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestTimesOutBeforeAuthentication) {
  operation_->Initialize();
  GetOperationTimer()->Fire();
  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest, TestAuthenticatesButThenTimesOut) {
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  operation_->Initialize();

  EXPECT_TRUE(operation_->has_device_authenticated());
  VerifyDefaultTimerCreated();

  GetOperationTimer()->Fire();

  EXPECT_TRUE(operation_->has_operation_finished());
}

}  // namespace ash::tether
