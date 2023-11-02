// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_EMPTY_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_EMPTY_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_

#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

// Dummy AnnouncementNotificationService that does nothing.
class EmptyAnnouncementNotificationService
    : public AnnouncementNotificationService {
 public:
  EmptyAnnouncementNotificationService();

  EmptyAnnouncementNotificationService(
      const EmptyAnnouncementNotificationService&) = delete;
  EmptyAnnouncementNotificationService& operator=(
      const EmptyAnnouncementNotificationService&) = delete;

  ~EmptyAnnouncementNotificationService() override;

 private:
  // AnnouncementNotificationService overrides.
  void MaybeShowNotification() override;
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_EMPTY_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
