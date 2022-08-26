// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_REVIEW_NOTIFICATION_PERMISSIONS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_REVIEW_NOTIFICATION_PERMISSIONS_HELPER_H_

#include <string>
#include <vector>

class Profile;

namespace site_settings {

struct NotificationPermissions {
  std::string origin;
  int notification_count;

  NotificationPermissions();
  NotificationPermissions(const NotificationPermissions& other);
  NotificationPermissions& operator=(const NotificationPermissions& other) =
      default;
  NotificationPermissions(NotificationPermissions&& other);
  NotificationPermissions(std::string origin, int notification_count);
  ~NotificationPermissions();
};

// Returns a list containing the sites that send a lot of notifications.
std::vector<NotificationPermissions> GetReviewNotificationPermissions(
    Profile* profile);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_REVIEW_NOTIFICATION_PERMISSIONS_HELPER_H_
