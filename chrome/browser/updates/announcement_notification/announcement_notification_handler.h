// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_HANDLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "chrome/browser/notifications/notification_handler.h"  // nogncheck

class Profile;

// Notification handler for showing announcement notification on all platforms.
class AnnouncementNotificationHandler : public NotificationHandler {
 public:
  AnnouncementNotificationHandler();

  AnnouncementNotificationHandler(const AnnouncementNotificationHandler&) =
      delete;
  AnnouncementNotificationHandler& operator=(
      const AnnouncementNotificationHandler&) = delete;

  ~AnnouncementNotificationHandler() override;

 private:
  void OnShow(Profile* profile, const std::string& notification_id) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;

  void OpenAnnouncement(Profile* profile);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_HANDLER_H_
