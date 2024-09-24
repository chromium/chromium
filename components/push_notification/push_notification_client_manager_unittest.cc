// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_notification/push_notification_client_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestMessage[] = "This is a test message";
const char kNotificationTypeIdKey[] = "type_id";
const char kNotificationPayloadKey[] = "payload";

class FakePushNotificationClient
    : public push_notification::PushNotificationClient {
 public:
  explicit FakePushNotificationClient(
      const push_notification::ClientId& client_id)
      : push_notification::PushNotificationClient(client_id) {}
  FakePushNotificationClient(const FakePushNotificationClient&) = delete;
  FakePushNotificationClient& operator=(const FakePushNotificationClient&) =
      delete;
  ~FakePushNotificationClient() override = default;

  void OnMessageReceived(
      base::flat_map<std::string, std::string> message_data) override {
    last_received_message_ = message_data.at(kNotificationPayloadKey);
  }

  const std::string& GetMostRecentMessageReceived() {
    return last_received_message_;
  }

 private:
  std::string last_received_message_;
};

}  // namespace

namespace push_notification {

class PushNotificationClientManagerTest : public testing::Test {
 public:
  PushNotificationClientManagerTest() = default;
  ~PushNotificationClientManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    push_notification_client_manager_ =
        std::make_unique<PushNotificationClientManager>();
  }

  std::unique_ptr<PushNotificationClientManager>
      push_notification_client_manager_;
};

TEST_F(PushNotificationClientManagerTest, AddClient) {
  auto fake_push_notification_client =
      std::make_unique<FakePushNotificationClient>(
          push_notification::ClientId::kNearbyPresence);
  push_notification_client_manager_->AddPushNotificationClient(
      fake_push_notification_client.get());

  EXPECT_EQ(
      1u,
      push_notification_client_manager_->GetPushNotificationClients().size());
}

TEST_F(PushNotificationClientManagerTest, AddThenRemoveClient) {
  auto fake_push_notification_client =
      std::make_unique<FakePushNotificationClient>(
          push_notification::ClientId::kNearbyPresence);
  push_notification_client_manager_->AddPushNotificationClient(
      fake_push_notification_client.get());
  EXPECT_EQ(
      1u,
      push_notification_client_manager_->GetPushNotificationClients().size());
  push_notification_client_manager_->RemovePushNotificationClient(
      push_notification::ClientId::kNearbyPresence);
  EXPECT_EQ(
      0u,
      push_notification_client_manager_->GetPushNotificationClients().size());
}

TEST_F(PushNotificationClientManagerTest, PassPushNotificationMessageToClient) {
  auto fake_push_notification_client =
      std::make_unique<FakePushNotificationClient>(
          push_notification::ClientId::kNearbyPresence);

  push_notification_client_manager_->AddPushNotificationClient(
      fake_push_notification_client.get());
  EXPECT_EQ(
      1u,
      push_notification_client_manager_->GetPushNotificationClients().size());

  PushNotificationClientManager::PushNotificationMessage test_incoming_message;
  test_incoming_message.data.insert_or_assign(
      kNotificationTypeIdKey, push_notification::kNearbyPresenceClientId);
  test_incoming_message.data.insert_or_assign(kNotificationPayloadKey,
                                              kTestMessage);

  push_notification_client_manager_->NotifyPushNotificationClientOfMessage(
      std::move(test_incoming_message));
  EXPECT_EQ(kTestMessage,
            fake_push_notification_client->GetMostRecentMessageReceived());
  ;
}

TEST_F(PushNotificationClientManagerTest,
       PassPushNotificationMessageBeforeAddingClient) {
  PushNotificationClientManager::PushNotificationMessage test_incoming_message;
  test_incoming_message.data.insert_or_assign(
      kNotificationTypeIdKey, push_notification::kNearbyPresenceClientId);
  test_incoming_message.data.insert_or_assign(kNotificationPayloadKey,
                                              kTestMessage);

  push_notification_client_manager_->NotifyPushNotificationClientOfMessage(
      std::move(test_incoming_message));

  auto fake_push_notification_client =
      std::make_unique<FakePushNotificationClient>(
          push_notification::ClientId::kNearbyPresence);

  push_notification_client_manager_->AddPushNotificationClient(
      fake_push_notification_client.get());
  EXPECT_EQ(
      1u,
      push_notification_client_manager_->GetPushNotificationClients().size());
  EXPECT_EQ(kTestMessage,
            fake_push_notification_client->GetMostRecentMessageReceived());
}

}  // namespace push_notification
