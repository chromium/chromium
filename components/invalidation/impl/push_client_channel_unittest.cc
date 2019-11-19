// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class PushClientChannelTest
    : public ::testing::Test,
      public SyncNetworkChannel::Observer {
 protected:
  PushClientChannelTest()
      : fake_push_client_(new notifier::FakePushClient()),
        push_client_channel_(base::WrapUnique(fake_push_client_)),
        last_invalidator_state_(DEFAULT_INVALIDATION_ERROR) {
    push_client_channel_.AddObserver(this);
    push_client_channel_.SetMessageReceiver(
        invalidation::NewPermanentCallback(
            this, &PushClientChannelTest::OnIncomingMessage));
    push_client_channel_.SetSystemResources(nullptr);
  }

  ~PushClientChannelTest() override {
    push_client_channel_.RemoveObserver(this);
  }

  void OnNetworkChannelStateChanged(
      InvalidatorState invalidator_state) override {
    last_invalidator_state_ = invalidator_state;
  }

  void OnIncomingMessage(std::string incoming_message) {
    last_message_ = incoming_message;
  }

  notifier::FakePushClient* fake_push_client_;
  PushClientChannel push_client_channel_;
  std::string last_message_;
  InvalidatorState last_invalidator_state_;
};

const char kMessage[] = "message";
const char kServiceContext[] = "service context";
const int64_t kSchedulingHash = 100;

// Make sure the channel subscribes to the correct notifications
// channel on construction.
TEST_F(PushClientChannelTest, Subscriptions) {
  notifier::Subscription expected_subscription;
  expected_subscription.channel = "tango_raw";
  EXPECT_TRUE(notifier::SubscriptionListsEqual(
      fake_push_client_->subscriptions(),
      notifier::SubscriptionList(1, expected_subscription)));
}

// Call UpdateCredentials on the channel.  It should propagate it to
// the push client.
TEST_F(PushClientChannelTest, UpdateCredentials) {
  const CoreAccountId kAccountId("foo@bar.com");
  const char kToken[] = "token";
  EXPECT_TRUE(fake_push_client_->email().empty());
  EXPECT_TRUE(fake_push_client_->token().empty());
  // PushClient treats account IDs as emails. See https://crbug.com/1010544
  push_client_channel_.UpdateCredentials(kAccountId, kToken);
  EXPECT_EQ(kAccountId.id, fake_push_client_->email());
  EXPECT_EQ(kToken, fake_push_client_->token());
}

// Simulate push client state changes on the push client.  It should
// propagate to the channel.
TEST_F(PushClientChannelTest, OnPushClientStateChange) {
  EXPECT_EQ(DEFAULT_INVALIDATION_ERROR, last_invalidator_state_);
  fake_push_client_->EnableNotifications();
  EXPECT_EQ(INVALIDATIONS_ENABLED, last_invalidator_state_);
  fake_push_client_->DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, last_invalidator_state_);
  fake_push_client_->DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, last_invalidator_state_);
}

// Call SendMessage on the channel.  It should propagate it to the
// push client.
TEST_F(PushClientChannelTest, SendMessage) {
  EXPECT_TRUE(fake_push_client_->sent_notifications().empty());
  push_client_channel_.SendMessage(kMessage);
  ASSERT_EQ(1u, fake_push_client_->sent_notifications().size());
  std::string expected_encoded_message =
      PushClientChannel::EncodeMessageForTest(
          kMessage,
          push_client_channel_.GetServiceContextForTest(),
          push_client_channel_.GetSchedulingHashForTest());
  ASSERT_EQ(expected_encoded_message,
            fake_push_client_->sent_notifications()[0].data);
}

// Encode a message with some context and then decode it.  The decoded info
// should match the original info.
TEST_F(PushClientChannelTest, EncodeDecode) {
  const std::string& data = PushClientChannel::EncodeMessageForTest(
      kMessage, kServiceContext, kSchedulingHash);
  std::string message;
  std::string service_context;
  int64_t scheduling_hash = 0LL;
  EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
      data, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Encode a message with no context and then decode it.  The decoded message
// should match the original message, but the context and hash should be
// untouched.
TEST_F(PushClientChannelTest, EncodeDecodeNoContext) {
  const std::string& data = PushClientChannel::EncodeMessageForTest(
      kMessage, std::string(), kSchedulingHash);
  std::string message;
  std::string service_context = kServiceContext;
  int64_t scheduling_hash = kSchedulingHash + 1;
  EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
      data, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash + 1, scheduling_hash);
}

// Decode an empty notification. It should result in an empty message
// but should leave the context and hash untouched.
TEST_F(PushClientChannelTest, DecodeEmpty) {
  std::string message = kMessage;
  std::string service_context = kServiceContext;
  int64_t scheduling_hash = kSchedulingHash;
  EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
      std::string(), &message, &service_context, &scheduling_hash));
  EXPECT_TRUE(message.empty());
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Try to decode a garbage notification.  It should leave all its
// arguments untouched and return false.
TEST_F(PushClientChannelTest, DecodeGarbage) {
  std::string data = "garbage";
  std::string message = kMessage;
  std::string service_context = kServiceContext;
  int64_t scheduling_hash = kSchedulingHash;
  EXPECT_FALSE(PushClientChannel::DecodeMessageForTest(
      data, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Simulate an incoming notification. It should be decoded properly
// by the channel.
TEST_F(PushClientChannelTest, OnIncomingMessage) {
  notifier::Notification notification;
  notification.data =
      PushClientChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);
  fake_push_client_->SimulateIncomingNotification(notification);

  EXPECT_EQ(kServiceContext,
            push_client_channel_.GetServiceContextForTest());
  EXPECT_EQ(kSchedulingHash,
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_EQ(kMessage, last_message_);
}

// Simulate an incoming notification with no receiver. It should be dropped by
// the channel.
TEST_F(PushClientChannelTest, OnIncomingMessageNoReceiver) {
  push_client_channel_.SetMessageReceiver(nullptr);

  notifier::Notification notification;
  notification.data = PushClientChannel::EncodeMessageForTest(
      kMessage, kServiceContext, kSchedulingHash);
  fake_push_client_->SimulateIncomingNotification(notification);

  EXPECT_TRUE(push_client_channel_.GetServiceContextForTest().empty());
  EXPECT_EQ(static_cast<int64_t>(0),
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_TRUE(last_message_.empty());
}

// Simulate an incoming garbage notification. It should be dropped by
// the channel.
TEST_F(PushClientChannelTest, OnIncomingMessageGarbage) {
  notifier::Notification notification;
  notification.data = "garbage";
  fake_push_client_->SimulateIncomingNotification(notification);
  EXPECT_TRUE(push_client_channel_.GetServiceContextForTest().empty());
  EXPECT_EQ(static_cast<int64_t>(0),
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_TRUE(last_message_.empty());
}

// Send a message, simulate an incoming message with context, and then
// send the same message again.  The first sent message should not
// have any context, but the second sent message should have the
// context from the incoming emssage.
TEST_F(PushClientChannelTest, PersistedMessageState) {
  push_client_channel_.SendMessage(kMessage);
  ASSERT_EQ(1u, fake_push_client_->sent_notifications().size());
  {
    std::string message;
    std::string service_context;
    int64_t scheduling_hash = 0LL;
    EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
        fake_push_client_->sent_notifications()[0].data,
        &message,
        &service_context,
        &scheduling_hash));
    EXPECT_EQ(kMessage, message);
    EXPECT_TRUE(service_context.empty());
    EXPECT_EQ(0LL, scheduling_hash);
  }

  notifier::Notification notification;
  notification.data = PushClientChannel::EncodeMessageForTest(
      kMessage, kServiceContext, kSchedulingHash);
  fake_push_client_->SimulateIncomingNotification(notification);

  push_client_channel_.SendMessage(kMessage);
  ASSERT_EQ(2u, fake_push_client_->sent_notifications().size());
  {
    std::string message;
    std::string service_context;
    int64_t scheduling_hash = 0LL;
    EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
        fake_push_client_->sent_notifications()[1].data,
        &message,
        &service_context,
        &scheduling_hash));
    EXPECT_EQ(kMessage, message);
    EXPECT_EQ(kServiceContext, service_context);
    EXPECT_EQ(kSchedulingHash, scheduling_hash);
  }
}

}  // namespace
}  // namespace syncer
