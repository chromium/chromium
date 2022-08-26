// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/review_notification_permissions_helper.h"

#include "chrome/browser/profiles/profile.h"

namespace site_settings {

NotificationPermissions::NotificationPermissions()
    : origin(""), notification_count(0) {}
NotificationPermissions::NotificationPermissions(
    const NotificationPermissions& other) = default;
NotificationPermissions::NotificationPermissions(
    NotificationPermissions&& other) = default;
NotificationPermissions::NotificationPermissions(std::string origin,
                                                 int notification_count)
    : origin(origin), notification_count(notification_count) {}
NotificationPermissions::~NotificationPermissions() = default;

std::vector<NotificationPermissions> GetReviewNotificationPermissions(
    Profile* profile) {
  // TODO(crbug.com/1345920): Use real notification permission data instead of
  // dummy data.
  std::vector<NotificationPermissions> dummy_list = {
      NotificationPermissions("www.example1.com", 8),
      NotificationPermissions("www.example2.com", 5),
      NotificationPermissions("www.example3.com", 1)};
  return dummy_list;
}

}  // namespace site_settings
