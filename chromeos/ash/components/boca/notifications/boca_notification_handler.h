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
  inline static constexpr char kSessionNotificationId[] =
      "school-tools-session";
  inline static constexpr char kCaptionNotificationId[] =
      "school-tools-caption";

  // Handle session started notification.
  void HandleSessionStartedNotification(
      message_center::MessageCenter* message_center);
  // Handle session ended notification.
  void HandleSessionEndedNotification(
      message_center::MessageCenter* message_center);
  // Handle caption notification.
  void HandleCaptionNotification(message_center::MessageCenter* message_center,
                                 bool is_local_caption_enabled,
                                 bool is_session_caption_enabled);

 private:
  bool is_local_caption_enabled_ = false;
  bool is_session_caption_enabled_ = false;
  bool is_session_started_ = false;
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_NOTIFICATIONS_BOCA_NOTIFICATION_HANDLER_H_
