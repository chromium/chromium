// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"

#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"

using ::testing::StrictMock;

namespace ash::boca {
namespace {
class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(void, LaunchApp, (), (override));
};

// TODO(crbug.com/376592382): Move this to shared library after to be reused by
// onTask and other components. Mock message center as we don't have the
// necessary dependencies to verify real notification.
class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  // message_center::FakeMessageCenter:
  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    notifications_.emplace(notification->id(), *notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    if (!notifications_.contains(id)) {
      return;
    }
    notifications_.erase(id);
    for (auto& observer : observer_list()) {
      observer.OnNotificationRemoved(id, by_user);
    }
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notifications_.contains(id)) {
      return &notifications_.at(id);
    }
    return nullptr;
  }

  void ClickOnNotification(const std::string& id) override {
    auto* notification = &notifications_.at(id);
    EXPECT_TRUE(notification);
    for (auto& observer : observer_list()) {
      observer.OnNotificationClicked(id, std::nullopt, std::nullopt);
    }
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    auto* notification = &notifications_.at(id);
    EXPECT_TRUE(notification);
    EXPECT_EQ(id, notification->id());
    notification->delegate()->Click(button_index, std::nullopt);

    for (auto& observer : observer_list()) {
      observer.OnNotificationClicked(id, button_index, std::nullopt);
    }
  }

 private:
  std::map<std::string, message_center::Notification> notifications_;
};

class BocaNotificationHandlerTest : public testing::Test {
 protected:
  BocaNotificationHandlerTest() = default;
  TestMessageCenter test_message_center_;
  BocaNotificationHandler handler_;
  StrictMock<MockBocaAppClient> boca_app_client_;
};

TEST_F(BocaNotificationHandlerTest,
       HandleSessionStartShouldCreateNotification) {
  handler_.HandleSessionStartedNotification(&test_message_center_);
  auto* notification = test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId);
  EXPECT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BOCA_CONNECTED_TO_CLASS_NOTIFICATION_MESSAGE),
            notification->message());
}

TEST_F(BocaNotificationHandlerTest, HandleClickButtonShouldOpenApp) {
  handler_.HandleSessionStartedNotification(&test_message_center_);
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId));
  EXPECT_CALL(boca_app_client_, LaunchApp()).Times(1);
  test_message_center_.ClickOnNotificationButton(
      /*id=*/handler_.kSessionNotificationId,
      /*button_index=*/0);
}

TEST_F(BocaNotificationHandlerTest,
       HandleSessionStartShouldRemoveNotification) {
  handler_.HandleSessionEndedNotification(&test_message_center_);
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId));
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kCaptionNotificationId));
}

TEST_F(BocaNotificationHandlerTest,
       HandleCaptionOnShouldAddCaptionNotification) {
  handler_.HandleSessionStartedNotification(&test_message_center_);

  handler_.HandleCaptionNotification(&test_message_center_,
                                     /*is_local_caption_enabled=*/true,
                                     /*is_session_caption_enabled=*/true);
  auto* notification = test_message_center_.FindVisibleNotificationById(
      handler_.kCaptionNotificationId);
  EXPECT_TRUE(notification);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BOCA_MICROPHONE_IN_USE_NOTIFICATION_MESSAGE),
            notification->message());
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId));
}
TEST_F(BocaNotificationHandlerTest, HandleCaptionOffShouldRemoveNotification) {
  handler_.HandleSessionStartedNotification(&test_message_center_);

  handler_.HandleCaptionNotification(&test_message_center_, false, false);
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kCaptionNotificationId));
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId));
}

TEST_F(BocaNotificationHandlerTest,
       HandleSessionEndShouldRemoveSessionCaptionNotification) {
  handler_.HandleSessionStartedNotification(&test_message_center_);

  handler_.HandleCaptionNotification(&test_message_center_,
                                     /*is_local_caption_enabled=*/false,
                                     /*is_session_caption_enabled=*/true);
  handler_.HandleSessionEndedNotification(&test_message_center_);
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kCaptionNotificationId));
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId));
}

TEST_F(BocaNotificationHandlerTest,
       HandleSessionEndShouldNotRemoveLocalCaptionNotification) {
  handler_.HandleSessionStartedNotification(&test_message_center_);

  handler_.HandleCaptionNotification(&test_message_center_,
                                     /*is_local_caption_enabled=*/true,
                                     /*is_session_caption_enabled=*/true);
  handler_.HandleSessionEndedNotification(&test_message_center_);
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      handler_.kCaptionNotificationId));
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      handler_.kSessionNotificationId));
}
}  // namespace
}  // namespace ash::boca
