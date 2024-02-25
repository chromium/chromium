// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/messenger_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/proximity_auth/messenger_observer.h"
#include "chromeos/ash/components/proximity_auth/remote_status_update.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"
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

class MockMessengerObserver : public MessengerObserver {
 public:
  explicit MockMessengerObserver(Messenger* messenger) : messenger_(messenger) {
    messenger_->AddObserver(this);
  }

  MockMessengerObserver(const MockMessengerObserver&) = delete;
  MockMessengerObserver& operator=(const MockMessengerObserver&) = delete;

  virtual ~MockMessengerObserver() { messenger_->RemoveObserver(this); }

  MOCK_METHOD1(OnUnlockEventSent, void(bool success));
  MOCK_METHOD1(OnRemoteStatusUpdate,
               void(const RemoteStatusUpdate& status_update));
  MOCK_METHOD1(OnUnlockResponse, void(bool success));
  MOCK_METHOD0(OnDisconnected, void());

 private:
  // The messenger that |this| instance observes.
  const raw_ptr<Messenger> messenger_;
};

class TestMessenger : public MessengerImpl {
 public:
  TestMessenger(std::unique_ptr<ash::secure_channel::ClientChannel> channel)
      : MessengerImpl(std::move(channel)) {}

  TestMessenger(const TestMessenger&) = delete;
  TestMessenger& operator=(const TestMessenger&) = delete;

  ~TestMessenger() override {}
};

}  // namespace

class ProximityAuthMessengerImplTest : public testing::Test {
 public:
  ProximityAuthMessengerImplTest(const ProximityAuthMessengerImplTest&) =
      delete;
  ProximityAuthMessengerImplTest& operator=(
      const ProximityAuthMessengerImplTest&) = delete;

 protected:
  ProximityAuthMessengerImplTest() = default;

  void CreateMessenger(bool is_multi_device_api_enabled) {
    auto fake_channel =
        std::make_unique<ash::secure_channel::FakeClientChannel>();
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

  raw_ptr<ash::secure_channel::FakeClientChannel, DanglingUntriaged>
      fake_channel_;

  std::unique_ptr<TestMessenger> messenger_;

  std::unique_ptr<MockMessengerObserver> observer_;
};

TEST_F(ProximityAuthMessengerImplTest,
       DispatchUnlockEvent_SendsExpectedMessage) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->DispatchUnlockEvent();

  EXPECT_EQ(
      "{"
      "\"name\":\"easy_unlock\","
      "\"type\":\"event\""
      "}",
      GetLastSentMessage());
}

TEST_F(ProximityAuthMessengerImplTest, RequestUnlock_SendsExpectedMessage) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  EXPECT_EQ("{\"type\":\"unlock_request\"}", GetLastSentMessage());
}

TEST_F(ProximityAuthMessengerImplTest,
       RequestUnlock_SendSucceeds_NotifiesObserversOnReply) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  messenger_->RequestUnlock();

  EXPECT_CALL(*observer_, OnUnlockResponse(true));
  fake_channel_->NotifyMessageReceived("{\"type\":\"unlock_response\"}");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_RemoteStatusUpdate_Invalid) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  // Receive a status update message that's missing all the data.
  EXPECT_CALL(*observer_, OnRemoteStatusUpdate(_)).Times(0);
  fake_channel_->NotifyMessageReceived(
      "{\"type\":\"status_update\"}, but encoded");
}

TEST_F(ProximityAuthMessengerImplTest,
       OnMessageReceived_RemoteStatusUpdate_Valid) {
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
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());
  messenger_->RequestUnlock();

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived("Not JSON");
}

TEST_F(ProximityAuthMessengerImplTest, OnMessageReceived_MissingTypeField) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());
  messenger_->RequestUnlock();

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived(
      "{\"some key that's not 'type'\":\"some value\"}");
}

TEST_F(ProximityAuthMessengerImplTest, OnMessageReceived_UnexpectedReply) {
  CreateMessenger(true /* is_multi_device_api_enabled */);

  StrictMock<MockMessengerObserver> observer(messenger_.get());

  // The StrictMock will verify that no observer methods are called.
  fake_channel_->NotifyMessageReceived("{\"type\":\"unlock_response\"}");
}

}  // namespace proximity_auth
