// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"

#include "chromeos/ash/components/boca/boca_app_client.h"
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
    EXPECT_FALSE(notification_);
    notification_ = std::move(notification);
  }

  void UpdateNotification(
      const std::string& id,
      std::unique_ptr<message_center::Notification> new_notification) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(notification_->id(), id);
    EXPECT_EQ(new_notification->id(), id);
    notification_ = std::move(new_notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(notification_->id(), id);
    notification_.reset();
    for (auto& observer : observer_list()) {
      observer.OnNotificationRemoved(id, by_user);
    }
  }

  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override {
    if (notification_) {
      EXPECT_EQ(notification_->id(), id);
      return notification_.get();
    }
    return nullptr;
  }

  void ClickOnNotification(const std::string& id) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());
    for (auto& observer : observer_list()) {
      observer.OnNotificationClicked(id, std::nullopt, std::nullopt);
    }
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    EXPECT_TRUE(notification_);
    EXPECT_EQ(id, notification_->id());
    notification_->delegate()->Click(button_index, std::nullopt);

    for (auto& observer : observer_list()) {
      observer.OnNotificationClicked(id, button_index, std::nullopt);
    }
  }

 private:
  std::unique_ptr<message_center::Notification> notification_;
};

class BocaNotificationHandlerTest : public testing::Test {
 protected:
  BocaNotificationHandlerTest() = default;
  TestMessageCenter test_message_center_;
  StrictMock<MockBocaAppClient> boca_app_client_;
};

TEST_F(BocaNotificationHandlerTest,
       HandleSessionStartShouldCreateNotification) {
  BocaNotificationHandler::HandleSessionStartedNotification(
      &test_message_center_);
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      BocaNotificationHandler::kNotificationId));
}

TEST_F(BocaNotificationHandlerTest, HandleClickButtonShouldOpenApp) {
  BocaNotificationHandler::HandleSessionStartedNotification(
      &test_message_center_);
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      BocaNotificationHandler::kNotificationId));
  EXPECT_CALL(boca_app_client_, LaunchApp()).Times(1);
  test_message_center_.ClickOnNotificationButton(
      /*id=*/BocaNotificationHandler::kNotificationId, /*button_index=*/0);
}

TEST_F(BocaNotificationHandlerTest,
       HandleSessionStartShouldRemoveNotification) {
  BocaNotificationHandler::HandleSessionEndedNotification(
      &test_message_center_);
  EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
      BocaNotificationHandler::kNotificationId));
}

TEST_F(BocaNotificationHandlerTest, HandleCaptionOnShouldUpdateNotification) {
  BocaNotificationHandler::HandleSessionStartedNotification(
      &test_message_center_);

  BocaNotificationHandler::HandleCaptionNotification(
      &test_message_center_, /*is_caption_enabled=*/true);
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      BocaNotificationHandler::kNotificationId));
}
TEST_F(BocaNotificationHandlerTest, HandleCaptionOffShouldUpdateNotification) {
  BocaNotificationHandler::HandleSessionStartedNotification(
      &test_message_center_);

  BocaNotificationHandler::HandleCaptionNotification(
      &test_message_center_, /*is_caption_enabled=*/false);
  EXPECT_TRUE(test_message_center_.FindVisibleNotificationById(
      BocaNotificationHandler::kNotificationId));
}

}  // namespace
}  // namespace ash::boca
