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
#include "chromeos/ash/components/tether/test_timer_factory.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

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
  TestOperation(const multidevice::RemoteDeviceRefList& devices_to_connect,
                device_sync::DeviceSyncClient* device_sync_client,
                secure_channel::SecureChannelClient* secure_channel_client)
      : MessageTransferOperation(devices_to_connect,
                                 secure_channel::ConnectionPriority::kLow,
                                 device_sync_client,
                                 secure_channel_client) {}
  ~TestOperation() override = default;

  bool HasDeviceAuthenticated(multidevice::RemoteDeviceRef remote_device) {
    const auto iter = device_authenticated_map_.find(remote_device);
    if (iter == device_authenticated_map_.end())
      return false;

    return iter->second;
  }

  const std::vector<std::unique_ptr<MessageWrapper>>& GetReceivedMessages(
      multidevice::RemoteDeviceRef remote_device) {
    static const std::vector<std::unique_ptr<MessageWrapper>> empty;

    const auto iter = device_message_map_.find(remote_device);
    if (iter == device_message_map_.end())
      return empty;

    return iter->second;
  }

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) override {
    device_authenticated_map_[remote_device] = true;
  }

  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         multidevice::RemoteDeviceRef remote_device) override {
    device_message_map_[remote_device].push_back(std::move(message_wrapper));

    if (should_unregister_device_on_message_received_)
      UnregisterDevice(remote_device);
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

  void set_should_unregister_device_on_message_received(
      bool should_unregister_device_on_message_received) {
    should_unregister_device_on_message_received_ =
        should_unregister_device_on_message_received;
  }

  bool has_operation_started() { return has_operation_started_; }

  bool has_operation_finished() { return has_operation_finished_; }

  absl::optional<int> last_sequence_number() { return last_sequence_number_; }

 private:
  base::flat_map<multidevice::RemoteDeviceRef, bool> device_authenticated_map_;
  base::flat_map<multidevice::RemoteDeviceRef,
                 std::vector<std::unique_ptr<MessageWrapper>>>
      device_message_map_;

  uint32_t timeout_seconds_ = kTestTimeoutSeconds;
  bool should_unregister_device_on_message_received_ = false;
  bool has_operation_started_ = false;
  bool has_operation_finished_ = false;
  absl::optional<int> last_sequence_number_;
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
        test_devices_(multidevice::CreateRemoteDeviceRefListForTest(4)) {}

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
  }

  void ConstructOperation(multidevice::RemoteDeviceRefList remote_devices) {
    test_timer_factory_ = new TestTimerFactory();

    for (auto remote_device : remote_devices) {
      // Prepare for connection timeout timers to be made for each remote
      // device.
      test_timer_factory_->set_device_id_for_next_timer(
          remote_device.GetDeviceId());

      auto fake_connection_attempt =
          std::make_unique<secure_channel::FakeConnectionAttempt>();
      remote_device_to_fake_connection_attempt_map_[remote_device] =
          fake_connection_attempt.get();
      fake_secure_channel_client_->set_next_listen_connection_attempt(
          remote_device, test_local_device_,
          std::move(fake_connection_attempt));
    }

    operation_ = base::WrapUnique(
        new TestOperation(remote_devices, fake_device_sync_client_.get(),
                          fake_secure_channel_client_.get()));
    operation_->SetTimerFactoryForTest(
        base::WrapUnique(test_timer_factory_.get()));
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
  }

  void InitializeOperation() {
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

    for (const auto& remote_device : operation_->remote_devices())
      VerifyConnectionTimerCreatedForDevice(remote_device);
  }

  void VerifyOperationStartedAndFinished(bool has_started, bool has_finished) {
    EXPECT_EQ(has_started, operation_->has_operation_started());
    EXPECT_EQ(has_finished, operation_->has_operation_finished());
  }

  void CreateAuthenticatedChannelForDevice(
      multidevice::RemoteDeviceRef remote_device) {
    test_timer_factory_->set_device_id_for_next_timer(
        remote_device.GetDeviceId());

    auto fake_client_channel =
        std::make_unique<secure_channel::FakeClientChannel>();
    remote_device_to_fake_client_channel_map_[remote_device] =
        fake_client_channel.get();
    remote_device_to_fake_connection_attempt_map_[remote_device]
        ->NotifyConnection(std::move(fake_client_channel));
  }

  base::MockOneShotTimer* GetTimerForDevice(
      multidevice::RemoteDeviceRef remote_device) {
    return test_timer_factory_->GetTimerForDeviceId(
        remote_device.GetDeviceId());
  }

  void VerifyDefaultTimerCreatedForDevice(
      multidevice::RemoteDeviceRef remote_device) {
    VerifyTimerCreatedForDevice(remote_device, kTestTimeoutSeconds);
  }

  void VerifyConnectionTimerCreatedForDevice(
      multidevice::RemoteDeviceRef remote_device) {
    VerifyTimerCreatedForDevice(
        remote_device, MessageTransferOperation::kConnectionTimeoutSeconds);
  }

  void VerifyTimerCreatedForDevice(multidevice::RemoteDeviceRef remote_device,
                                   uint32_t timeout_seconds) {
    EXPECT_TRUE(GetTimerForDevice(remote_device));
    EXPECT_EQ(base::Seconds(timeout_seconds),
              GetTimerForDevice(remote_device)->GetCurrentDelay());
  }

  int SendMessageToDevice(multidevice::RemoteDeviceRef remote_device,
                          std::unique_ptr<MessageWrapper> message_wrapper) {
    return operation_->SendMessageToDevice(test_devices_[0],
                                           std::move(message_wrapper));
  }

  const multidevice::RemoteDeviceRef test_local_device_;
  const multidevice::RemoteDeviceRefList test_devices_;

  base::flat_map<multidevice::RemoteDeviceRef,
                 secure_channel::FakeConnectionAttempt*>
      remote_device_to_fake_connection_attempt_map_;
  base::flat_map<multidevice::RemoteDeviceRef,
                 secure_channel::FakeClientChannel*>
      remote_device_to_fake_client_channel_map_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  raw_ptr<TestTimerFactory, DanglingUntriaged | ExperimentalAsh>
      test_timer_factory_;
  std::unique_ptr<TestOperation> operation_;
};

TEST_F(MessageTransferOperationTest, TestFailedConnection) {
  ConstructOperation(multidevice::RemoteDeviceRefList{test_devices_[0]});
  InitializeOperation();

  remote_device_to_fake_connection_attempt_map_[test_devices_[0]]
      ->NotifyConnectionAttemptFailure(
          secure_channel::mojom::ConnectionAttemptFailureReason::
              AUTHENTICATION_ERROR);

  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionSendAndReceiveMessage) {
  ConstructOperation(multidevice::RemoteDeviceRefList{test_devices_[0]});
  InitializeOperation();

  // Simulate how subclasses behave after a successful response: unregister the
  // device.
  operation_->set_should_unregister_device_on_message_received(true);

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  auto message_wrapper =
      std::make_unique<MessageWrapper>(TetherAvailabilityRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  int sequence_number =
      SendMessageToDevice(test_devices_[0], std::move(message_wrapper));
  std::vector<std::pair<std::string, base::OnceClosure>>& sent_messages =
      remote_device_to_fake_client_channel_map_[test_devices_[0]]
          ->sent_messages();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first);

  EXPECT_FALSE(operation_->last_sequence_number());
  std::move(sent_messages[0].second).Run();
  EXPECT_EQ(sequence_number, operation_->last_sequence_number());

  remote_device_to_fake_client_channel_map_[test_devices_[0]]
      ->NotifyMessageReceived(
          MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  const auto& message = operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestTimesOutBeforeAuthentication) {
  ConstructOperation(multidevice::RemoteDeviceRefList{test_devices_[0]});
  InitializeOperation();

  GetTimerForDevice(test_devices_[0])->Fire();
  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest, TestAuthenticatesButThenTimesOut) {
  ConstructOperation(multidevice::RemoteDeviceRefList{test_devices_[0]});
  InitializeOperation();

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  GetTimerForDevice(test_devices_[0])->Fire();

  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest, TestRepeatedInputDevice) {
  // Construct with two copies of the same device.
  ConstructOperation(
      multidevice::RemoteDeviceRefList{test_devices_[0], test_devices_[0]});
  InitializeOperation();

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  remote_device_to_fake_client_channel_map_[test_devices_[0]]
      ->NotifyMessageReceived(
          MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  // Should still have received only one message even though the device was
  // repeated twice in the constructor.
  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  const auto& message = operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, MultipleDevices) {
  ConstructOperation(test_devices_);
  InitializeOperation();

  for (const auto& remote_device : test_devices_)
    test_timer_factory_->ClearTimerForDeviceId(remote_device.GetDeviceId());

  // Authenticate |test_devices_[0]|'s channel.
  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  // Fail to connect to |test_devices_[1]|.
  test_timer_factory_->set_device_id_for_next_timer(
      test_devices_[1].GetDeviceId());
  remote_device_to_fake_connection_attempt_map_[test_devices_[1]]
      ->NotifyConnectionAttemptFailure(
          secure_channel::mojom::ConnectionAttemptFailureReason::
              GATT_CONNECTION_ERROR);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));
  EXPECT_FALSE(GetTimerForDevice(test_devices_[1]));

  // Authenticate |test_devices_[2]|'s channel.
  CreateAuthenticatedChannelForDevice(test_devices_[2]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[2]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[2]);

  // Fail to connect to |test_devices_[3]|.
  test_timer_factory_->set_device_id_for_next_timer(
      test_devices_[3].GetDeviceId());
  remote_device_to_fake_connection_attempt_map_[test_devices_[3]]
      ->NotifyConnectionAttemptFailure(
          secure_channel::mojom::ConnectionAttemptFailureReason::
              GATT_CONNECTION_ERROR);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[3]));
  EXPECT_FALSE(GetTimerForDevice(test_devices_[3]));
}

}  // namespace tether

}  // namespace ash
