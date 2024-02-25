// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/notification_manager_impl.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/phonehub/fake_message_sender.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

const char16_t kAppName[] = u"Test App";
const char kPackageName[] = "com.google.testapp";
const int64_t kUserId = 1;

const char16_t kTitle[] = u"Test notification";
const char16_t kTextContent[] = u"This is a test notification";

enum class NotificationState { kAdded, kUpdated, kRemoved };

Notification CreateNotification(int64_t id) {
  return phonehub::Notification(
      id,
      phonehub::Notification::AppMetadata(
          kAppName, kPackageName,
          /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
          /*icon_color=*/std::nullopt,
          /*icon_is_monochrome=*/true, kUserId,
          proto::AppStreamabilityStatus::STREAMABLE),
      base::Time::Now(), Notification::Importance::kDefault,
      Notification::Category::kConversation,
      {{Notification::ActionType::kInlineReply, /*action_id=*/0}},
      Notification::InteractionBehavior::kNone, kTitle, kTextContent);
}

class FakeObserver : public NotificationManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  std::optional<NotificationState> GetState(int64_t notification_id) const {
    const auto it = id_to_state_map_.find(notification_id);
    if (it == id_to_state_map_.end())
      return std::nullopt;
    return it->second;
  }

 private:
  // NotificationManager::Observer:
  void OnNotificationsAdded(
      const base::flat_set<int64_t>& notification_ids) override {
    for (int64_t id : notification_ids)
      id_to_state_map_[id] = NotificationState::kAdded;
  }

  void OnNotificationsUpdated(
      const base::flat_set<int64_t>& notification_ids) override {
    for (int64_t id : notification_ids)
      id_to_state_map_[id] = NotificationState::kUpdated;
  }

  void OnNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids) override {
    for (int64_t id : notification_ids)
      id_to_state_map_[id] = NotificationState::kRemoved;
  }

  base::flat_map<int64_t, NotificationState> id_to_state_map_;
};

}  // namespace

class NotificationManagerImplTest : public testing::Test {
 protected:
  NotificationManagerImplTest() = default;
  NotificationManagerImplTest(const NotificationManagerImplTest&) = delete;
  NotificationManagerImplTest& operator=(const NotificationManagerImplTest&) =
      delete;
  ~NotificationManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<NotificationManagerImpl>(
        &fake_message_sender_, &fake_user_action_recorder_,
        &fake_multidevice_setup_client_);
    manager_->AddObserver(&fake_observer_);
  }

  void TearDown() override { manager_->RemoveObserver(&fake_observer_); }

  NotificationManager& manager() { return *manager_; }
  FakeMessageSender& fake_message_sender() { return fake_message_sender_; }

  void SetNotificationsInternal(
      const base::flat_set<Notification>& notifications) {
    manager_->SetNotificationsInternal(notifications);
  }

  void ClearNotificationsInternal() { manager_->ClearNotificationsInternal(); }

  size_t GetNumNotifications() {
    return manager_->id_to_notification_map_.size();
  }

  std::optional<NotificationState> GetNotificationState(
      int64_t notification_id) {
    return fake_observer_.GetState(notification_id);
  }

  void SetNotificationFeatureStatus(FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(
        Feature::kPhoneHubNotifications, feature_state);
  }

  FakeUserActionRecorder fake_user_action_recorder_;

 private:
  FakeObserver fake_observer_;

  FakeMessageSender fake_message_sender_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;

  std::unique_ptr<NotificationManager> manager_;
};

TEST_F(NotificationManagerImplTest, SetAndClearNotificationsInternal) {
  EXPECT_EQ(0u, GetNumNotifications());
  const int64_t expected_id1 = 0;
  const int64_t expected_id2 = 1;

  SetNotificationsInternal(base::flat_set<Notification>{
      CreateNotification(expected_id1), CreateNotification(expected_id2)});
  EXPECT_EQ(2u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id2));

  ClearNotificationsInternal();
  EXPECT_EQ(0u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kRemoved, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kRemoved, GetNotificationState(expected_id2));
}

TEST_F(NotificationManagerImplTest, GetNotification) {
  EXPECT_EQ(0u, GetNumNotifications());
  const int64_t expected_id1 = 0;

  SetNotificationsInternal(
      base::flat_set<Notification>{CreateNotification(expected_id1)});
  EXPECT_EQ(1u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));

  // Call GetNotification() on an existent notification id. Expect a non-null
  // pointer.
  EXPECT_TRUE(manager().GetNotification(expected_id1));

  // Call GetNotification() on a non-existent notification id. Expect a null
  // pointer.
  EXPECT_FALSE(manager().GetNotification(/*notification_id=*/4));

  // Remove |expected_id1| and expect that GetNotification() returns a null
  // pointer.
  manager().DismissNotification(expected_id1);
  EXPECT_EQ(1u, fake_message_sender().GetDismissNotificationRequestCallCount());
  EXPECT_EQ(expected_id1,
            fake_message_sender().GetRecentDismissNotificationRequest());
  EXPECT_FALSE(manager().GetNotification(expected_id1));
}

TEST_F(NotificationManagerImplTest, DismissNotifications) {
  EXPECT_EQ(0u, GetNumNotifications());
  const int64_t expected_id1 = 0;
  const int64_t expected_id2 = 1;

  SetNotificationsInternal(base::flat_set<Notification>{
      CreateNotification(expected_id1), CreateNotification(expected_id2)});
  EXPECT_EQ(2u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id2));

  manager().DismissNotification(expected_id2);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_notification_dismissals());
  EXPECT_EQ(1u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kRemoved, GetNotificationState(expected_id2));
  EXPECT_EQ(1u, fake_message_sender().GetDismissNotificationRequestCallCount());
  EXPECT_EQ(expected_id2,
            fake_message_sender().GetRecentDismissNotificationRequest());

  // Dismiss the same notification again, verify nothing happens.
  manager().DismissNotification(expected_id2);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_notification_dismissals());
  EXPECT_EQ(1u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kRemoved, GetNotificationState(expected_id2));
  EXPECT_EQ(1u, fake_message_sender().GetDismissNotificationRequestCallCount());
  EXPECT_EQ(expected_id2,
            fake_message_sender().GetRecentDismissNotificationRequest());
}

TEST_F(NotificationManagerImplTest, UpdatedNotification) {
  EXPECT_EQ(0u, GetNumNotifications());
  const int64_t expected_id1 = 0;
  const int64_t expected_id2 = 1;

  SetNotificationsInternal(base::flat_set<Notification>{
      CreateNotification(expected_id1), CreateNotification(expected_id2)});
  EXPECT_EQ(2u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id2));

  // Simulate updating a notification.
  SetNotificationsInternal(
      base::flat_set<Notification>{CreateNotification(expected_id1)});
  EXPECT_EQ(2u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kUpdated, GetNotificationState(expected_id1));
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id2));
}

TEST_F(NotificationManagerImplTest, SendInlineReply) {
  EXPECT_EQ(0u, GetNumNotifications());
  const int64_t expected_id1 = 0;

  SetNotificationsInternal(
      base::flat_set<Notification>{CreateNotification(expected_id1)});
  EXPECT_EQ(1u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));

  // Simulate sending an inline reply to a notification.
  const std::u16string& expected_reply(u"test reply");
  manager().SendInlineReply(expected_id1, expected_reply);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_notification_replies());
  EXPECT_EQ(1u, GetNumNotifications());
  EXPECT_EQ(NotificationState::kAdded, GetNotificationState(expected_id1));
  EXPECT_EQ(1u,
            fake_message_sender().GetNotificationInlineReplyRequestCallCount());
  std::pair<int64_t, std::u16string> pair =
      fake_message_sender().GetRecentNotificationInlineReplyRequest();
  EXPECT_EQ(expected_id1, pair.first);
  EXPECT_EQ(expected_reply, pair.second);

  // Simulate sending an inline reply to a non-existent notification. Expect
  // that no new reply calls were called and that the most recent reply is the
  // same as the previous inline reply call.
  manager().SendInlineReply(/*notification_id=*/5, /*reply=*/std::u16string());
  EXPECT_EQ(1u, fake_user_action_recorder_.num_notification_replies());
  EXPECT_EQ(1u,
            fake_message_sender().GetNotificationInlineReplyRequestCallCount());
  pair = fake_message_sender().GetRecentNotificationInlineReplyRequest();
  EXPECT_EQ(expected_id1, pair.first);
  EXPECT_EQ(expected_reply, pair.second);
}

TEST_F(NotificationManagerImplTest, ClearNotificationsOnFeatureStatusChanged) {
  // Simulate enabling notification feature state.
  SetNotificationFeatureStatus(FeatureState::kEnabledByUser);

  // Set an internal notification.
  SetNotificationsInternal(
      base::flat_set<Notification>{CreateNotification(/*notification_id=*/1)});
  EXPECT_EQ(1u, GetNumNotifications());

  // Change notification feature state to disabled, expect internal
  // notifications to be cleared.
  SetNotificationFeatureStatus(FeatureState::kDisabledByUser);
  EXPECT_EQ(0u, GetNumNotifications());
}
}  // namespace phonehub
}  // namespace ash
