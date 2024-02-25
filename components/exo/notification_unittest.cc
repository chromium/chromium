// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace exo {
namespace {

using NotificationTest = test::ExoTestBase;

void Close(int* close_call_count, bool by_user) {
  (*close_call_count)++;
}

void Click(int* click_call_count,
           std::optional<int>* passed_button_index,
           const std::optional<int>& button_index) {
  (*click_call_count)++;
  *passed_button_index = button_index;
}

TEST_F(NotificationTest, CloseCallback) {
  auto* message_center = message_center::MessageCenter::Get();

  // Clear all notifications.
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);

  // Params for test notification.
  const std::string title = "TEST title";
  const std::string message = "TEST message";
  const std::string display_source = "TEST display_source";
  const std::string notification_id = "exo-notification.test";
  const std::string notifier_id = "exo-notification-test";
  const std::vector<std::string> buttons = {"TEST button0", "TEST button1"};

  // For the close callback.
  int close_call_count = 0;

  Notification notification(
      title, message, display_source, notification_id, notifier_id, buttons,
      base::BindRepeating(&Close, base::Unretained(&close_call_count)),
      base::DoNothing());

  EXPECT_EQ(close_call_count, 0);

  EXPECT_NE(nullptr,
            message_center->FindVisibleNotificationById(notification_id));

  // Closes notification.
  notification.Close();

  EXPECT_EQ(nullptr,
            message_center->FindVisibleNotificationById(notification_id));

  // Expected to be called once
  EXPECT_EQ(close_call_count, 1);

  // Clear all notifications.
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
}

TEST_F(NotificationTest, ClickCallback) {
  auto* message_center = message_center::MessageCenter::Get();

  // Clear all notifications.
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);

  // Params for test notification.
  const std::string title = "TEST title";
  const std::string message = "TEST message";
  const std::string display_source = "TEST display_source";
  const std::string notification_id = "exo-notification.test";
  const std::string notifier_id = "exo-notification-test";
  const std::vector<std::string> buttons = {"TEST button0", "TEST button1"};

  // For the click callback.
  int click_call_count = 0;
  std::optional<int> passed_button_index;

  Notification notification(
      title, message, display_source, notification_id, notifier_id, buttons,
      base::DoNothing(),
      base::BindRepeating(&Click, base::Unretained(&click_call_count),
                          base::Unretained(&passed_button_index)));

  EXPECT_EQ(click_call_count, 0);

  EXPECT_NE(nullptr,
            message_center->FindVisibleNotificationById(notification_id));

  // Clicks on notification.
  message_center->ClickOnNotification(notification_id);

  // Expected to be called once without button index
  EXPECT_EQ(click_call_count, 1);
  EXPECT_EQ(passed_button_index, std::nullopt);

  // Clicks on button.
  message_center->ClickOnNotificationButton(notification_id, 0);

  // Expected to be called once with button index
  EXPECT_EQ(click_call_count, 2);
  EXPECT_EQ(*passed_button_index, 0);

  // Clear all notifications.
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
}

}  // namespace
}  // namespace exo
