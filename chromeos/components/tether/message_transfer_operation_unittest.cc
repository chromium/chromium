// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/message_transfer_operation.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/components/tether/timer_factory.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

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
  TestOperation(const cryptauth::RemoteDeviceRefList& devices_to_connect,
                device_sync::DeviceSyncClient* device_sync_client,
                secure_channel::SecureChannelClient* secure_channel_client,
                BleConnectionManager* connection_manager)
      : MessageTransferOperation(devices_to_connect,
                                 secure_channel::ConnectionPriority::kLow,
                                 device_sync_client,
                                 secure_channel_client,
                                 connection_manager) {}
  ~TestOperation() override = default;

  bool HasDeviceAuthenticated(cryptauth::RemoteDeviceRef remote_device) {
    const auto iter = device_map_.find(remote_device);
    if (iter == device_map_.end())
      return false;

    return iter->second.has_device_authenticated;
  }

  std::vector<std::shared_ptr<MessageWrapper>> GetReceivedMessages(
      cryptauth::RemoteDeviceRef remote_device) {
    const auto iter = device_map_.find(remote_device);
    if (iter == device_map_.end())
      return std::vector<std::shared_ptr<MessageWrapper>>();

    return iter->second.received_messages;
  }

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      cryptauth::RemoteDeviceRef remote_device) override {
    device_map_[remote_device].has_device_authenticated = true;
  }

  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         cryptauth::RemoteDeviceRef remote_device) override {
    device_map_[remote_device].received_messages.push_back(
        std::move(message_wrapper));

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

  base::Optional<int> last_sequence_number() { return last_sequence_number_; }

 private:
  struct DeviceMapValue {
    DeviceMapValue() = default;
    ~DeviceMapValue() = default;

    bool has_device_authenticated;
    std::vector<std::shared_ptr<MessageWrapper>> received_messages;
  };

  base::flat_map<cryptauth::RemoteDeviceRef, DeviceMapValue> device_map_;

  uint32_t timeout_seconds_ = kTestTimeoutSeconds;
  bool should_unregister_device_on_message_received_ = false;
  bool has_operation_started_ = false;
  bool has_operation_finished_ = false;
  base::Optional<int> last_sequence_number_;
};

class TestTimerFactory : public TimerFactory {
 public:
  ~TestTimerFactory() override = default;

  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override {
    EXPECT_FALSE(device_id_for_next_timer_.empty());
    base::MockOneShotTimer* mock_timer = new base::MockOneShotTimer();
    device_id_to_timer_map_[device_id_for_next_timer_] = mock_timer;
    return base::WrapUnique(mock_timer);
  }

  base::MockOneShotTimer* GetTimerForDeviceId(const std::string& device_id) {
    return device_id_to_timer_map_[device_id_for_next_timer_];
  }

  void ClearTimerForDeviceId(const std::string& device_id) {
    device_id_to_timer_map_.erase(device_id_for_next_timer_);
  }

  void set_device_id_for_next_timer(
      const std::string& device_id_for_next_timer) {
    device_id_for_next_timer_ = device_id_for_next_timer;
  }

 private:
  std::string device_id_for_next_timer_;
  base::flat_map<std::string, base::MockOneShotTimer*> device_id_to_timer_map_;
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
 protected:
  MessageTransferOperationTest()
      : test_local_device_(cryptauth::RemoteDeviceRefBuilder()
                               .SetPublicKey("local device")
                               .Build()),
        test_devices_(cryptauth::CreateRemoteDeviceRefListForTest(4)) {
    // These tests are written under the assumption that there are a maximum of
    // 3 "empty scan" connection attempts and 6 "GATT" connection attempts; the
    // tests need to be edited if these values change.
    EXPECT_EQ(3u, MessageTransferOperation::kMaxEmptyScansPerDevice);
    EXPECT_EQ(6u,
              MessageTransferOperation::kMaxGattConnectionAttemptsPerDevice);
  }

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();
  }

  void SetMultiDeviceApiState(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kMultiDeviceApi);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kMultiDeviceApi);
    }
  }

  void ConstructOperation(cryptauth::RemoteDeviceRefList remote_devices,
                          bool is_multidevice_api_enabled) {
    SetMultiDeviceApiState(is_multidevice_api_enabled /* enabled */);

    test_timer_factory_ = new TestTimerFactory();

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
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
    }

    operation_ = base::WrapUnique(new TestOperation(
        remote_devices, fake_device_sync_client_.get(),
        fake_secure_channel_client_.get(), fake_ble_connection_manager_.get()));
    operation_->SetTimerFactoryForTest(base::WrapUnique(test_timer_factory_));
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
  }

  void InitializeOperation() {
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
    operation_->Initialize();

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      for (const auto* arguments :
           fake_secure_channel_client_
               ->last_listen_for_connection_request_arguments_list()) {
        EXPECT_EQ(kTetherFeature, arguments->feature);
      }
    }

    VerifyOperationStartedAndFinished(true /* has_started */,
                                      false /* has_finished */);

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      for (const auto& remote_device : operation_->remote_devices())
        VerifyConnectionTimerCreatedForDevice(remote_device);
    }
  }

  void VerifyOperationStartedAndFinished(bool has_started, bool has_finished) {
    EXPECT_EQ(has_started, operation_->has_operation_started());
    EXPECT_EQ(has_finished, operation_->has_operation_finished());
  }

  void CreateAuthenticatedChannelForDevice(
      cryptauth::RemoteDeviceRef remote_device) {
    test_timer_factory_->set_device_id_for_next_timer(
        remote_device.GetDeviceId());

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      auto fake_client_channel =
          std::make_unique<secure_channel::FakeClientChannel>();
      remote_device_to_fake_client_channel_map_[remote_device] =
          fake_client_channel.get();
      remote_device_to_fake_connection_attempt_map_[remote_device]
          ->NotifyConnection(std::move(fake_client_channel));
    } else {
      fake_ble_connection_manager_->SetDeviceStatus(
          remote_device.GetDeviceId(),
          cryptauth::SecureChannel::Status::CONNECTING,
          BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
      fake_ble_connection_manager_->SetDeviceStatus(
          remote_device.GetDeviceId(),
          cryptauth::SecureChannel::Status::CONNECTED,
          BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
      fake_ble_connection_manager_->SetDeviceStatus(
          remote_device.GetDeviceId(),
          cryptauth::SecureChannel::Status::AUTHENTICATING,
          BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
      fake_ble_connection_manager_->SetDeviceStatus(
          remote_device.GetDeviceId(),
          cryptauth::SecureChannel::Status::AUTHENTICATED,
          BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
    }
  }

  base::MockOneShotTimer* GetTimerForDevice(
      cryptauth::RemoteDeviceRef remote_device) {
    return test_timer_factory_->GetTimerForDeviceId(
        remote_device.GetDeviceId());
  }

  void VerifyDefaultTimerCreatedForDevice(
      cryptauth::RemoteDeviceRef remote_device) {
    VerifyTimerCreatedForDevice(remote_device, kTestTimeoutSeconds);
  }

  void VerifyConnectionTimerCreatedForDevice(
      cryptauth::RemoteDeviceRef remote_device) {
    VerifyTimerCreatedForDevice(
        remote_device, MessageTransferOperation::kConnectionTimeoutSeconds);
  }

  void VerifyTimerCreatedForDevice(cryptauth::RemoteDeviceRef remote_device,
                                   uint32_t timeout_seconds) {
    EXPECT_TRUE(GetTimerForDevice(remote_device));
    EXPECT_EQ(base::TimeDelta::FromSeconds(timeout_seconds),
              GetTimerForDevice(remote_device)->GetCurrentDelay());
  }

  int SendMessageToDevice(cryptauth::RemoteDeviceRef remote_device,
                          std::unique_ptr<MessageWrapper> message_wrapper) {
    return operation_->SendMessageToDevice(test_devices_[0],
                                           std::move(message_wrapper));
  }

  const cryptauth::RemoteDeviceRef test_local_device_;
  const cryptauth::RemoteDeviceRefList test_devices_;

  base::flat_map<cryptauth::RemoteDeviceRef,
                 secure_channel::FakeConnectionAttempt*>
      remote_device_to_fake_connection_attempt_map_;
  base::flat_map<cryptauth::RemoteDeviceRef, secure_channel::FakeClientChannel*>
      remote_device_to_fake_client_channel_map_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  TestTimerFactory* test_timer_factory_;
  std::unique_ptr<TestOperation> operation_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageTransferOperationTest);
};

TEST_F(MessageTransferOperationTest,
       MultiDeviceApiEnabled_TestFailedConnection) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     true /* is_multidevice_api_enabled */);
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
       MultiDeviceApiEnabled_TestSuccessfulConnectionSendAndReceiveMessage) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     true /* is_multidevice_api_enabled */);
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
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest,
       MultiDeviceApiEnabled_TestTimesOutBeforeAuthentication) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     true /* is_multidevice_api_enabled */);
  InitializeOperation();

  GetTimerForDevice(test_devices_[0])->Fire();
  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest,
       MultiDeviceApiEnabled_TestAuthenticatesButThenTimesOut) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     true /* is_multidevice_api_enabled */);
  InitializeOperation();

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  GetTimerForDevice(test_devices_[0])->Fire();

  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest,
       MultiDeviceApiEnabled_TestRepeatedInputDevice) {
  // Construct with two copies of the same device.
  ConstructOperation(
      cryptauth::RemoteDeviceRefList{test_devices_[0], test_devices_[0]},
      true /* is_multidevice_api_enabled */);
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
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, MultiDeviceApiEnabled_MultipleDevices) {
  ConstructOperation(test_devices_, true /* is_multidevice_api_enabled */);
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

TEST_F(MessageTransferOperationTest, CannotReceiveResponse_RetryLimitReached) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING,
      BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED,
      BleConnectionManager::StateChangeDetail::
          STATE_CHANGE_DETAIL_COULD_NOT_ATTEMPT_CONNECTION);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try and fail again. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING,
      BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED,
      BleConnectionManager::StateChangeDetail::
          STATE_CHANGE_DETAIL_COULD_NOT_ATTEMPT_CONNECTION);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try and fail a third time. The maximum number of unanswered failures has
  // been reached, so the device should be unregistered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING,
      BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED,
      BleConnectionManager::StateChangeDetail::
          STATE_CHANGE_DETAIL_COULD_NOT_ATTEMPT_CONNECTION);
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);

  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest,
       CannotCompleteGattConnection_RetryLimitReached) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  fake_ble_connection_manager_->SimulateGattErrorConnectionAttempts(
      test_devices_[0].GetDeviceId(),
      MessageTransferOperation::kMaxGattConnectionAttemptsPerDevice);
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest, MixedConnectionAttemptFailures) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Fail to establish a connection one fewer time than the maximum allowed. The
  // device should still be registered since the maximum was not hit.
  fake_ble_connection_manager_->SimulateUnansweredConnectionAttempts(
      test_devices_[0].GetDeviceId(),
      MessageTransferOperation::kMaxEmptyScansPerDevice - 1);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Now, fail to establish a connection via GATT errors.
  fake_ble_connection_manager_->SimulateGattErrorConnectionAttempts(
      test_devices_[0].GetDeviceId(),
      MessageTransferOperation::kMaxGattConnectionAttemptsPerDevice);
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest, TestFailsThenConnects_Unanswered) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING,
      BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED,
      BleConnectionManager::StateChangeDetail::
          STATE_CHANGE_DETAIL_COULD_NOT_ATTEMPT_CONNECTION);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try again and succeed.
  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest, TestFailsThenConnects_GattError) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING,
      BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTED,
      BleConnectionManager::StateChangeDetail::STATE_CHANGE_DETAIL_NONE);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED,
      BleConnectionManager::StateChangeDetail::
          STATE_CHANGE_DETAIL_GATT_CONNECTION_WAS_ATTEMPTED);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try again and succeed.
  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionAndReceiveMessage) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Simulate how subclasses behave after a successful response: unregister the
  // device.
  operation_->set_should_unregister_device_on_message_received(true);

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestDevicesUnregisteredAfterDeletion) {
  ConstructOperation(test_devices_, false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));

  // Delete the operation. All registered devices should be unregistered.
  operation_.reset();
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionAndReceiveMessage_TimeoutSeconds) {
  const uint32_t kTimeoutSeconds = 90;

  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  operation_->set_timeout_seconds(kTimeoutSeconds);

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyTimerCreatedForDevice(test_devices_[0], kTimeoutSeconds);

  EXPECT_EQ(base::TimeDelta::FromSeconds(kTimeoutSeconds),
            GetTimerForDevice(test_devices_[0])->GetCurrentDelay());

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestAuthenticatesButTimesOut) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  GetTimerForDevice(test_devices_[0])->Fire();

  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest, TestRepeatedInputDevice) {
  // Construct with two copies of the same device.
  ConstructOperation(
      cryptauth::RemoteDeviceRefList{test_devices_[0], test_devices_[0]},
      false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  // Should still have received only one message even though the device was
  // repeated twice in the constructor.
  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestReceiveEventForOtherDevice) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Simulate the authentication of |test_devices_[1]|'s channel. Since the
  // operation was only constructed with |test_devices_[0]|, this operation
  // should not be affected.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[1].GetDeviceId(), base::UnguessableToken::Create(),
      secure_channel::ConnectionPriority::kLow);
  CreateAuthenticatedChannelForDevice(test_devices_[1]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));

  // Now, receive a message for |test_devices[1]|. Likewise, this operation
  // should not be affected.
  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[1].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_FALSE(operation_->GetReceivedMessages(test_devices_[0]).size());
}

TEST_F(MessageTransferOperationTest,
       TestAlreadyAuthenticatedBeforeInitialization) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);

  // Simulate the authentication of |test_devices_[0]|'s channel before
  // initialization.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0].GetDeviceId(), base::UnguessableToken::Create(),
      secure_channel::ConnectionPriority::kLow);
  CreateAuthenticatedChannelForDevice(test_devices_[0]);

  // Now initialize; the authentication handler should have been invoked.
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  // Receiving a message should work at this point.
  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest,
       AlreadyAuthenticatedBeforeInitialization_TimesOut) {
  ConstructOperation(cryptauth::RemoteDeviceRefList{test_devices_[0]},
                     false /* is_multidevice_api_enabled */);

  // Simulate the authentication of |test_devices_[0]|'s channel before
  // initialization.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0].GetDeviceId(), base::UnguessableToken::Create(),
      secure_channel::ConnectionPriority::kLow);
  CreateAuthenticatedChannelForDevice(test_devices_[0]);

  // Now initialize; the authentication handler should have been invoked.
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  GetTimerForDevice(test_devices_[0])->Fire();
  EXPECT_TRUE(operation_->has_operation_finished());

  // Should still be registered since it was also registered for the
  // CONNECT_TETHERING_REQUEST MessageType.
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
}

TEST_F(MessageTransferOperationTest, MultipleDevices) {
  ConstructOperation(test_devices_, false /* is_multidevice_api_enabled */);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));

  // Authenticate |test_devices_[0]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0].GetDeviceId(), base::UnguessableToken::Create(),
      secure_channel::ConnectionPriority::kLow);
  CreateAuthenticatedChannelForDevice(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  // Fail 3 unanswered times to connect to |test_devices_[1]|.
  test_timer_factory_->set_device_id_for_next_timer(
      test_devices_[1].GetDeviceId());
  fake_ble_connection_manager_->SimulateUnansweredConnectionAttempts(
      test_devices_[1].GetDeviceId(),
      MessageTransferOperation::kMaxEmptyScansPerDevice);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(GetTimerForDevice(test_devices_[1]));

  // Authenticate |test_devices_[2]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[2].GetDeviceId(), base::UnguessableToken::Create(),
      secure_channel::ConnectionPriority::kLow);
  CreateAuthenticatedChannelForDevice(test_devices_[2]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[2]));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  VerifyDefaultTimerCreatedForDevice(test_devices_[2]);

  // Fail 3 unanswered times to connect to |test_devices_[3]|.
  test_timer_factory_->set_device_id_for_next_timer(
      test_devices_[3].GetDeviceId());
  fake_ble_connection_manager_->SimulateUnansweredConnectionAttempts(
      test_devices_[3].GetDeviceId(),
      MessageTransferOperation::kMaxEmptyScansPerDevice);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[3]));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));
  EXPECT_FALSE(GetTimerForDevice(test_devices_[3]));
}

}  // namespace tether

}  // namespace chromeos
