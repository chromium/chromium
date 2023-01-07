// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_ANDROID_H_

#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

class AnnouncementNotificationDelegateAndroid
    : public AnnouncementNotificationService::Delegate {
 public:
  AnnouncementNotificationDelegateAndroid();

  AnnouncementNotificationDelegateAndroid(
      const AnnouncementNotificationDelegateAndroid&) = delete;
  AnnouncementNotificationDelegateAndroid& operator=(
      const AnnouncementNotificationDelegateAndroid&) = delete;

  ~AnnouncementNotificationDelegateAndroid() override;

 private:
  // AnnouncementNotificationService::Delegate implementation.
  void ShowNotification() override;
  bool IsFirstRun() override;
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_DELEGATE_ANDROID_H_
