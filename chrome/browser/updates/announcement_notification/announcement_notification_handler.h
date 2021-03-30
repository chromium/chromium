// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/notifications/notification_handler.h"  // nogncheck

class Profile;

// Notification handler for showing announcement notification on all platforms.
class AnnouncementNotificationHandler : public NotificationHandler {
 public:
  AnnouncementNotificationHandler();
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
               const base::Optional<int>& action_index,
               const base::Optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;

  void OpenAnnouncement(Profile* profile);

  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationHandler);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_HANDLER_H_
