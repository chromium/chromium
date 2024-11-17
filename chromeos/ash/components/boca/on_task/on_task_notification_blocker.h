// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_NOTIFICATION_BLOCKER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_NOTIFICATION_BLOCKER_H_

#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"

namespace message_center {
class MessageCenter;
class Notification;
}  // namespace message_center

namespace ash::boca {

// Notification blocker implementation for OnTask that blocks all notifications
// except for OnTask related ones. Especially required to prevent surfacing
// notifications that allow users to exit locked mode.
class OnTaskNotificationBlocker : public message_center::NotificationBlocker {
 public:
  explicit OnTaskNotificationBlocker(
      message_center::MessageCenter* message_center);
  OnTaskNotificationBlocker(const OnTaskNotificationBlocker&) = delete;
  OnTaskNotificationBlocker& operator=(const OnTaskNotificationBlocker&) =
      delete;
  ~OnTaskNotificationBlocker() override;

  // NotificationBlocker:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override;
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ON_TASK_NOTIFICATION_BLOCKER_H_
