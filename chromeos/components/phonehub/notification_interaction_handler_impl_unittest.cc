// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_interaction_handler_impl.h"

#include <memory>

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

  void HandleNotificationClick(int64_t notification_id) override {
    notification_id_ = notification_id;
  }

 private:
  int64_t notification_id_ = 0;
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

  NotificationInteractionHandler& handler() { return *interaction_handler_; }

 private:
  FakeClickHandler fake_click_handler_;

  std::unique_ptr<NotificationInteractionHandlerImpl> interaction_handler_;
};

TEST_F(NotificationInteractionHandlerImplTest,
       NotifyNotificationsClickHandler) {
  const int64_t expected_id = 599600;

  handler().HandleNotificationClicked(expected_id);
  EXPECT_EQ(expected_id, GetNotificationId());
}

}  // namespace phonehub
}  // namespace chromeos
