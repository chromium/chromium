// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_NOTIFICATIONS_BOCA_NOTIFICATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_NOTIFICATIONS_BOCA_NOTIFICATION_HANDLER_H_

#include "ui/message_center/message_center.h"

namespace ash::boca {
// Handles Class hub notifications.
class BocaNotificationHandler {
 public:
  inline static constexpr char kNotificationId[] = "school-tools";
  // Handle session started notification.
  static void HandleSessionStartedNotification(
      message_center::MessageCenter* message_center);
  // Handle session ended notification.
  static void HandleSessionEndedNotification(
      message_center::MessageCenter* message_center);
  // Handle caption notification.
  static void HandleCaptionNotification(
      message_center::MessageCenter* message_center,
      bool is_caption_enabled);
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_NOTIFICATIONS_BOCA_NOTIFICATION_HANDLER_H_
