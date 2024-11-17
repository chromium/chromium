// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_notification_blocker.h"

#include <memory>

#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

using message_center::Notification;

namespace ash::boca {
namespace {

class OnTaskNotificationBlockerTest : public ::testing::Test {
 protected:
  Notification CreateFakeNotificationWithId(
      const std::string& notification_id) {
    Notification notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                              notification_id, u"TestTitle", u"TestMessage",
                              ui::ImageModel(), u"TestSource", GURL(),
                              message_center::NotifierId(),
                              message_center::RichNotificationData(), nullptr);
    return notification;
  }

  OnTaskNotificationBlocker notification_blocker_{/*message_center=*/nullptr};
};

TEST_F(OnTaskNotificationBlockerTest, ShouldNotBlockAllowlistedNotifications) {
  auto notification =
      CreateFakeNotificationWithId(kOnTaskEnterLockedModeNotificationId);
  EXPECT_TRUE(notification_blocker_.ShouldShowNotification(notification));
}

TEST_F(OnTaskNotificationBlockerTest, ShouldBlockNonAllowlistedNotifications) {
  auto notification = CreateFakeNotificationWithId("RandomNotificationId");
  EXPECT_FALSE(notification_blocker_.ShouldShowNotification(notification));
}

TEST_F(OnTaskNotificationBlockerTest,
       ShouldNotBlockAllowlistedNotificationPopups) {
  auto notification =
      CreateFakeNotificationWithId(kOnTaskEnterLockedModeNotificationId);
  EXPECT_TRUE(
      notification_blocker_.ShouldShowNotificationAsPopup(notification));
}

TEST_F(OnTaskNotificationBlockerTest,
       ShouldBlockNonAllowlistedNotificationPopups) {
  auto notification = CreateFakeNotificationWithId("RandomNotificationId");
  EXPECT_FALSE(
      notification_blocker_.ShouldShowNotificationAsPopup(notification));
}

}  // namespace
}  // namespace ash::boca
