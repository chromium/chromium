// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

class NotificationDisplayService;

// Id of the announcement notification.
constexpr char kAnnouncementNotificationId[] = "announcement_notification";

// Default delegate for AnnouncementNotificationService that works on
// non-Android platforms.
class AnnouncementNotificationDelegate
    : public AnnouncementNotificationService::Delegate {
 public:
  explicit AnnouncementNotificationDelegate(
      NotificationDisplayService* display_service);
  ~AnnouncementNotificationDelegate() override;

 private:
  // AnnouncementNotificationService::Delegate implementation.
  void ShowNotification() override;
  bool IsFirstRun() override;

  // Used to show the notification.
  NotificationDisplayService* display_service_;

  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationDelegate);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_
