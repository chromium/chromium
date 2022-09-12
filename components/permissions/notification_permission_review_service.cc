// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/notification_permission_review_service.h"

namespace permissions {

NotificationPermissions::NotificationPermissions(std::string origin,
                                                 int notification_count)
    : origin(origin), notification_count(notification_count) {}
NotificationPermissions::~NotificationPermissions() = default;

// TODO(crbug.com/1345920): Use HostContentSettingsMap to fetch notification
// permissions data.
NotificationPermissionsReviewService::NotificationPermissionsReviewService(
    HostContentSettingsMap* hcsm) {}

NotificationPermissionsReviewService::~NotificationPermissionsReviewService() =
    default;

void NotificationPermissionsReviewService::Shutdown() {}

std::vector<NotificationPermissions>
NotificationPermissionsReviewService::GetNotificationSiteListForReview() {
  // TODO(crbug.com/1345920): Use notification permission data to be reviewed
  // instead of empty list.
  return {};
}

}  // namespace permissions
