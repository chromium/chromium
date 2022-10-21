// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_

#include "base/memory/raw_ptr.h"
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

  AnnouncementNotificationDelegate(const AnnouncementNotificationDelegate&) =
      delete;
  AnnouncementNotificationDelegate& operator=(
      const AnnouncementNotificationDelegate&) = delete;

  ~AnnouncementNotificationDelegate() override;

 private:
  // AnnouncementNotificationService::Delegate implementation.
  void ShowNotification() override;
  bool IsFirstRun() override;

  // Used to show the notification.
  raw_ptr<NotificationDisplayService, DanglingUntriaged> display_service_;
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_H_
