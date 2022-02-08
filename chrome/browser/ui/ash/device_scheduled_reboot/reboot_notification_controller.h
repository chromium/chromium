// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_REBOOT_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_REBOOT_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace base {
class Time;
}

using ButtonClickCallback =
    message_center::HandleNotificationClickDelegate::ButtonClickCallback;

// This class is responsible for creating and managing notifications about the
// reboot when DeviceScheduledRebootPolicy is set.
class RebootNotificationController {
 public:
  RebootNotificationController();
  RebootNotificationController(const RebootNotificationController&) = delete;
  RebootNotificationController& operator=(const RebootNotificationController&) =
      delete;
  ~RebootNotificationController();

  // Only show notification if the user is in session and kiosk session is not
  // in progress.
  void MaybeShowPendingRebootNotification(
      const base::Time& reboot_time,
      ButtonClickCallback reboot_callback) const;

 private:
  void ShowNotification(
      const std::string& id,
      const std::u16string& title,
      const std::u16string& message,
      const message_center::RichNotificationData& data,
      scoped_refptr<message_center::NotificationDelegate> delegate) const;
};

#endif  // CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_REBOOT_NOTIFICATION_CONTROLLER_H_
