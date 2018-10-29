// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/messenger_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/messenger_observer.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "components/cryptauth/connection.h"
#include "components/cryptauth/fake_connection.h"
#include "components/cryptauth/fake_secure_context.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/cryptauth/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {

namespace {

const char kTestFeature[] = "testFeature";
const char kChallenge[] = "a most difficult challenge";

class MockMessengerObserver : public MessengerObserver {
 public:
  explicit MockMessengerObserver(Messenger* messenger) : messenger_(messenger) {
    messenger_->AddObserver(this);
  }
  virtual ~MockMessengerObserver() { messenger_->RemoveObserver(this); }

  MOCK_METHOD1(OnUnlockEventSent, void(bool success));
  MOCK_METHOD1(OnRemoteStatusUpdate,
               void(const RemoteStatusUpdate& status_update));
  MOCK_METHOD1(OnDecryptResponseProxy,
               void(const std::string& decrypted_bytes));
  MOCK_METHOD1(OnUnlockResponse, void(bool success));
  MOCK_METHOD0(OnDisconnected, void());

  virtual void OnDecryptResponse(const std::string& decrypted_bytes) {
    OnDecryptResponseProxy(decrypted_bytes);
  }

 private:
  // The messenger that |this| instance observes.
  Messenger* const messenger_;

  DISALLOW_COPY_AND_ASSIGN(MockMessengerObserver);
};

class TestMessenger : public MessengerImpl {
 public:
  TestMessenger(
      std::unique_ptr<chromeos::secure_channel::ClientChannel> channel)
      : MessengerImpl(std::make_unique<cryptauth::FakeConnection>(
                          cryptauth::CreateRemoteDeviceRefForTest()),
                      std::make_unique<cryptauth::FakeSecureContext>(),
                      std::move(channel)) {}
  ~TestMessenger() override {}

  // Simple getters for the fake objects owned by |this| messenger_->
  cryptauth::FakeConnection* GetFakeConnection() {
    return static_cast<cryptauth::FakeConnection*>(connection());
  }
  cryptauth::FakeSecureContext* GetFakeSecureContext() {
    return static_cast<cryptauth::FakeSecureContext*>(GetSecureContext());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMessenger);
};

}  // namespace

class ProximityAuthMessengerImplTest : public testing::Test {
 protected:
  ProximityAuthMessengerImplTest() = default;

  void SetMultiDeviceApiState(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kMultiDeviceApi);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kMultiDeviceApi);
    }
  }

  void CreateMessenger(bool is_multi_device_api_enabled) {
    SetMultiDeviceApiState(is_multi_device_api_enabled);

    auto fake_channel =
        std::make_unique<chromeos::secure_channel::FakeClientChannel>();
    fake_channel_ = fake_channel.get();

    messenger_ = std::make_unique<TestMessenger>(std::move(fake_channel));
    observer_ = std::make_unique<MockMessengerObserver>(messenger_.get());
  }

  std::string GetLastSentMessage() {
    std::vector<std::pair<std::string, base::OnceClosure>>&
        message_and_callbacks = fake_channel_->sent_messages();
    std::move(message_and_callbacks[0].second).Run();

    std::string message_copy = message_and_callbacks[0].first;
    message_and_callbacks.erase(message_and_callbacks.begin());
    return message_copy;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  chromeos::secure_channel::FakeClientChannel* fake_channel_;

  std::unique_ptr<TestMessenger> messenger_;

  std::unique_ptr<MockMessengerObserver> observer_;

 private:

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthMessengerImplTest);
};

TEST_F(ProximityAuthMessengerImplTest,
       SupportsSignIn_ProtocolVersionThreeZero) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->GetFakeSecureContext()->set_protocol_version(
      cryptauth::SecureContext::PROTOCOL_VERSION_THREE_ZERO);
  EXPECT_FALSE(messenger_->SupportsSignIn());
}

TEST_F(ProximityAuthMessengerImplTest, SupportsSignIn_ProtocolVersionThreeOne) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->GetFakeSecureContext()->set_protocol_version(
      cryptauth::SecureContext::PROTOCOL_VERSION_THREE_ONE);
  EXPECT_TRUE(messenger_->SupportsSignIn());
}

TEST_F(ProximityAuthMessengerImplTest, SupportsSignIn_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  EXPECT_TRUE(messenger_->SupportsSignIn());
}

TEST_F(ProximityAuthMessengerImplTest,
       OnConnectionStatusChanged_ConnectionDisconnects) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  EXPECT_CALL(*observer_, OnDisconnected());
  messenger_->GetFakeConnection()->Disconnect();
}

TEST_F(ProximityAuthMessengerImplTest,
       DispatchUnlockEvent_SendsExpectedMessage) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->DispatchUnlockEvent();

  cryptauth::WireMessage* message =
      messenger_->GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(
      "{"
      "\"name\":\"easy_unlock\","
      "\"type\":\"event\""
      "}, but encoded",
      message->payload());
  EXPECT_EQ("easy_unlock", message->feature());
}

TEST_F(ProximityAuthMessengerImplTest,
       DispatchUnlockEvent_SendsExpectedMessage_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->DispatchUnlockEvent();

  EXPECT_EQ(
      "{"
      "\"name\":\"easy_unlock\","
      "\"type\":\"event\""
      "}",
      GetLastSentMessage());
}

TEST_F(ProximityAuthMessengerImplTest, DispatchUnlockEvent_SendMessageFails) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->DispatchUnlockEvent();

  EXPECT_CALL(*observer_, OnUnlockEventSent(false));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST_F(ProximityAuthMessengerImplTest,
       DispatchUnlockEvent_SendMessageSucceeds) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->DispatchUnlockEvent();

  EXPECT_CALL(*observer_, OnUnlockEventSent(true));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SignInUnsupported_DoesntSendMessage) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->GetFakeSecureContext()->set_protocol_version(
      cryptauth::SecureContext::PROTOCOL_VERSION_THREE_ZERO);
  messenger_->RequestDecryption(kChallenge);
  EXPECT_FALSE(messenger_->GetFakeConnection()->current_message());
}

TEST_F(ProximityAuthMessengerImplTest, RequestDecryption_SendsExpectedMessage) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  cryptauth::WireMessage* message =
      messenger_->GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"YSBtb3N0IGRpZmZpY3VsdCBjaGFsbGVuZ2U=\","
      "\"type\":\"decrypt_request\""
      "}, but encoded",
      message->payload());
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendsExpectedMessage_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"YSBtb3N0IGRpZmZpY3VsdCBjaGFsbGVuZ2U=\","
      "\"type\":\"decrypt_request\""
      "}",
      GetLastSentMessage());
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendsExpectedMessage_UsingBase64UrlEncoding) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption("\xFF\xE6");

  cryptauth::WireMessage* message =
      messenger_->GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"_-Y=\","
      "\"type\":\"decrypt_request\""
      "}, but encoded",
      message->payload());
}

TEST_F(
    ProximityAuthMessengerImplTest,
    RequestDecryption_SendsExpectedMessage_UsingBase64UrlEncoding_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption("\xFF\xE6");

  EXPECT_EQ(
      "{"
      "\"encrypted_data\":\"_-Y=\","
      "\"type\":\"decrypt_request\""
      "}",
      GetLastSentMessage());
}

TEST_F(ProximityAuthMessengerImplTest, RequestDecryption_SendMessageFails) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(std::string()));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendSucceeds_WaitsForReply) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(_)).Times(0);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendSucceeds_NotifiesObserversOnReply_NoData) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(std::string()));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{\"type\":\"decrypt_response\"}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    RequestDecryption_SendSucceeds_NotifiesObserversOnReply_NoData_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(std::string()));
  fake_channel_->NotifyMessageReceived("{\"type\":\"decrypt_response\"}");
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendSucceeds_NotifiesObserversOnReply_InvalidData) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(std::string()));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"not a base64-encoded string\""
      "}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    RequestDecryption_SendSucceeds_NotifiesObserversOnReply_InvalidData_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(std::string()));
  fake_channel_->NotifyMessageReceived(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"not a base64-encoded string\""
      "}");
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendSucceeds_NotifiesObserversOnReply_ValidData) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy("a winner is you"));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""  // "a winner is you", base64-encoded
      "}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    RequestDecryption_SendSucceeds_NotifiesObserversOnReply_ValidData_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy("a winner is you"));
  fake_channel_->NotifyMessageReceived(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""  // "a winner is you", base64-encoded
      "}");
}

// Verify that the messenger correctly parses base64url encoded data.
TEST_F(ProximityAuthMessengerImplTest,
       RequestDecryption_SendSucceeds_ParsesBase64UrlEncodingInReply) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy("\xFF\xE6"));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"_-Y=\""  // "\0xFF\0xE6", base64url-encoded.
      "}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    RequestDecryption_SendSucceeds_ParsesBase64UrlEncodingInReply_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestDecryption(kChallenge);

  EXPECT_CALL(*observer_, OnDecryptResponseProxy("\xFF\xE6"));
  fake_channel_->NotifyMessageReceived(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"_-Y=\""  // "\0xFF\0xE6", base64url-encoded.
      "}");
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestUnlock_SignInUnsupported_DoesntSendMessage) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->GetFakeSecureContext()->set_protocol_version(
      cryptauth::SecureContext::PROTOCOL_VERSION_THREE_ZERO);
  messenger_->RequestUnlock();
  EXPECT_FALSE(messenger_->GetFakeConnection()->current_message());
}

TEST_F(ProximityAuthMessengerImplTest, RequestUnlock_SendsExpectedMessage) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  cryptauth::WireMessage* message =
      messenger_->GetFakeConnection()->current_message();
  ASSERT_TRUE(message);
  EXPECT_EQ("{\"type\":\"unlock_request\"}, but encoded", message->payload());
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestUnlock_SendsExpectedMessage_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  EXPECT_EQ("{\"type\":\"unlock_request\"}", GetLastSentMessage());
}

TEST_F(ProximityAuthMessengerImplTest, RequestUnlock_SendMessageFails) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  EXPECT_CALL(*observer_, OnUnlockResponse(false));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestUnlock_SendSucceeds_WaitsForReply) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  EXPECT_CALL(*observer_, OnUnlockResponse(_)).Times(0);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestUnlock_SendSucceeds_NotifiesObserversOnReply) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  EXPECT_CALL(*observer_, OnUnlockResponse(true));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature), "{\"type\":\"unlock_response\"}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    RequestUnlock_SendSucceeds_NotifiesObserversOnReply_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  EXPECT_CALL(*observer_, OnUnlockResponse(true));
  fake_channel_->NotifyMessageReceived("{\"type\":\"unlock_response\"}");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_RemoteStatusUpdate_Invalid) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  // Receive a status update message that's missing all the data.
  EXPECT_CALL(*observer_, OnRemoteStatusUpdate(_)).Times(0);
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature), "{\"type\":\"status_update\"}, but encoded");
}

// ryan
TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_RemoteStatusUpdate_Invalid_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  // Receive a status update message that's missing all the data.
  EXPECT_CALL(*observer_, OnRemoteStatusUpdate(_)).Times(0);
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature), "{\"type\":\"status_update\"}, but encoded");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_RemoteStatusUpdate_Valid) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  EXPECT_CALL(*observer_,
              OnRemoteStatusUpdate(
                  AllOf(Field(&RemoteStatusUpdate::user_presence, USER_PRESENT),
                        Field(&RemoteStatusUpdate::secure_screen_lock_state,
                              SECURE_SCREEN_LOCK_ENABLED),
                        Field(&RemoteStatusUpdate::trust_agent_state,
                              TRUST_AGENT_UNSUPPORTED))));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{"
      "\"type\":\"status_update\","
      "\"user_presence\":\"present\","
      "\"secure_screen_lock\":\"enabled\","
      "\"trust_agent\":\"unsupported\""
      "}, but encoded");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_RemoteStatusUpdate_Valid_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  EXPECT_CALL(*observer_,
              OnRemoteStatusUpdate(
                  AllOf(Field(&RemoteStatusUpdate::user_presence, USER_PRESENT),
                        Field(&RemoteStatusUpdate::secure_screen_lock_state,
                              SECURE_SCREEN_LOCK_ENABLED),
                        Field(&RemoteStatusUpdate::trust_agent_state,
                              TRUST_AGENT_UNSUPPORTED))));
  fake_channel_->NotifyMessageReceived(
      "{"
      "\"type\":\"status_update\","
      "\"user_presence\":\"present\","
      "\"secure_screen_lock\":\"enabled\","
      "\"trust_agent\":\"unsupported\""
      "}");
}

TEST_F(ProximityAuthMessengerImplTest, OnMessageReceived_InvalidJSON) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());
  messenger_->RequestUnlock();
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger_->GetFakeConnection()->ReceiveMessage(std::string(kTestFeature),
                                                  "Not JSON, but encoded");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_InvalidJSON_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());
  messenger_->RequestUnlock();

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived("Not JSON");
}

TEST_F(ProximityAuthMessengerImplTest, OnMessageReceived_MissingTypeField) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());
  messenger_->RequestUnlock();
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{\"some key that's not 'type'\":\"some value\"}, but encoded");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_MissingTypeField_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());
  messenger_->RequestUnlock();

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived(
      "{\"some key that's not 'type'\":\"some value\"}");
}

TEST_F(ProximityAuthMessengerImplTest, OnMessageReceived_UnexpectedReply) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  // The StrictMock will verify that no observer methods are called.
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature), "{\"type\":\"unlock_response\"}, but encoded");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_UnexpectedReply_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived("{\"type\":\"unlock_response\"}");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_MismatchedReply_UnlockInReplyToDecrypt) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  messenger_->RequestDecryption(kChallenge);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature), "{\"type\":\"unlock_response\"}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    OnMessageReceived_MismatchedReply_UnlockInReplyToDecrypt_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  messenger_->RequestDecryption(kChallenge);

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived("{\"type\":\"unlock_response\"}");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_MismatchedReply_DecryptInReplyToUnlock) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  messenger_->RequestUnlock();
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // The StrictMock will verify that no observer methods are called.
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}, but encoded");
}

TEST_F(
    ProximityAuthMessengerImplTest,
    OnMessageReceived_MismatchedReply_DecryptInReplyToUnlock_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  messenger_->RequestUnlock();

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}");
}

TEST_F(ProximityAuthMessengerImplTest, BuffersMessages_WhileSending) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  // Initiate a decryption request, and then initiate an unlock request before
  // the decryption request is even finished sending.
  messenger_->RequestDecryption(kChallenge);
  messenger_->RequestUnlock();

  EXPECT_CALL(*observer_, OnDecryptResponseProxy(std::string()));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(false);

  EXPECT_CALL(*observer_, OnUnlockResponse(false));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST_F(ProximityAuthMessengerImplTest, BuffersMessages_WhileAwaitingReply) {
  CreateMessenger(false /* is_multi_device_api_enabled */);

  // Initiate a decryption request, and allow the message to be sent.
  messenger_->RequestDecryption(kChallenge);
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(true);

  // At this point, the messenger is awaiting a reply to the decryption message.
  // While it's waiting, initiate an unlock request.
  messenger_->RequestUnlock();

  // Now simulate a response arriving for the original decryption request.
  EXPECT_CALL(*observer_, OnDecryptResponseProxy("a winner is you"));
  messenger_->GetFakeConnection()->ReceiveMessage(
      std::string(kTestFeature),
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}, but encoded");

  // The unlock request should have remained buffered, and should only now be
  // sent.
  EXPECT_CALL(*observer_, OnUnlockResponse(false));
  messenger_->GetFakeConnection()->FinishSendingMessageWithSuccess(false);
}

TEST_F(ProximityAuthMessengerImplTest, BuffersMessages_MultiDeviceApiEnabled) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  // Initiate a decryption request, and allow the message to be sent.
  messenger_->RequestDecryption(kChallenge);

  // At this point, the messenger is awaiting a reply to the decryption message.
  // While it's waiting, initiate an unlock request.
  messenger_->RequestUnlock();

  // Now simulate a response arriving for the original decryption request.
  EXPECT_CALL(*observer_, OnDecryptResponseProxy("a winner is you"));
  fake_channel_->NotifyMessageReceived(
      "{"
      "\"type\":\"decrypt_response\","
      "\"data\":\"YSB3aW5uZXIgaXMgeW91\""
      "}");

  // The unlock request should have remained buffered, and should only now be
  // sent.
  EXPECT_CALL(*observer_, OnUnlockResponse(true));
  GetLastSentMessage();
  fake_channel_->NotifyMessageReceived("{\"type\":\"unlock_response\"}");
}

}  // namespace proximity_auth
