// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_notification_blocker.h"

#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotificationBlocker;
using message_center::NotifierType;

namespace ash::boca {

OnTaskNotificationBlocker::OnTaskNotificationBlocker(
    MessageCenter* message_center)
    : NotificationBlocker(message_center) {}

OnTaskNotificationBlocker::~OnTaskNotificationBlocker() = default;

bool OnTaskNotificationBlocker::ShouldShowNotification(
    const Notification& notification) const {
  return GetAllowlistedNotificationIdsForLockedMode().contains(
      notification.id());
}

bool OnTaskNotificationBlocker::ShouldShowNotificationAsPopup(
    const Notification& notification) const {
  return GetAllowlistedNotificationIdsForLockedMode().contains(
      notification.id());
}

}  // namespace ash::boca
