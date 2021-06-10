// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_interaction_handler_impl.h"

#include <memory>

#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_click_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeClickHandler : public NotificationClickHandler {
 public:
  FakeClickHandler() = default;
  ~FakeClickHandler() override = default;

  int64_t get_notification_id() const { return notification_id_; }

  std::u16string get_visible_app_name() { return visible_app_name; }

  std::string get_package_name() { return package_name; }

  void HandleNotificationClick(
      int64_t notification_id,
      const Notification::AppMetadata& app_metadata) override {
    notification_id_ = notification_id;
    visible_app_name = app_metadata.visible_app_name;
    package_name = app_metadata.package_name;
  }

 private:
  int64_t notification_id_ = 0;
  std::u16string visible_app_name;
  std::string package_name;
};

}  // namespace

class NotificationInteractionHandlerImplTest : public testing::Test {
 protected:
  NotificationInteractionHandlerImplTest() = default;
  NotificationInteractionHandlerImplTest(
      const NotificationInteractionHandlerImplTest&) = delete;
  NotificationInteractionHandlerImplTest& operator=(
      const NotificationInteractionHandlerImplTest&) = delete;
  ~NotificationInteractionHandlerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    interaction_handler_ =
        std::make_unique<NotificationInteractionHandlerImpl>();
    interaction_handler_->AddNotificationClickHandler(&fake_click_handler_);
  }

  void TearDown() override {
    interaction_handler_->RemoveNotificationClickHandler(&fake_click_handler_);
  }

  int64_t GetNotificationId() const {
    return fake_click_handler_.get_notification_id();
  }

  std::u16string GetVisibleAppName() {
    return fake_click_handler_.get_visible_app_name();
  }

  std::string GetPackageName() {
    return fake_click_handler_.get_package_name();
  }

  NotificationInteractionHandler& handler() { return *interaction_handler_; }

 private:
  FakeClickHandler fake_click_handler_;

  std::unique_ptr<NotificationInteractionHandlerImpl> interaction_handler_;
};

TEST_F(NotificationInteractionHandlerImplTest,
       NotifyNotificationsClickHandler) {
  const int64_t expected_id = 599600;
  const char16_t expected_app_visible_name[] = u"Fake App";
  const char expected_package_name[] = "com.fakeapp";
  auto expected_app_metadata = Notification::AppMetadata(
      expected_app_visible_name, expected_package_name, gfx::Image());

  handler().HandleNotificationClicked(expected_id, expected_app_metadata);

  EXPECT_EQ(expected_id, GetNotificationId());
  EXPECT_EQ(expected_app_visible_name, GetVisibleAppName());
  EXPECT_EQ(expected_package_name, GetPackageName());
}

}  // namespace phonehub
}  // namespace chromeos
