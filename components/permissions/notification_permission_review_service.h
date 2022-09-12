// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
#define COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_

#include <vector>

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace permissions {

struct NotificationPermissions {
  std::string origin;
  int notification_count;

  NotificationPermissions(std::string origin, int notification_count);
  ~NotificationPermissions();
};

// Manages a list of sites that send a high volume of notifications with low
// engagement. The list may be displayed for user review in the Notifications
// settings page.
class NotificationPermissionsReviewService : public KeyedService {
 public:
  explicit NotificationPermissionsReviewService(HostContentSettingsMap* hcsm);

  NotificationPermissionsReviewService(
      const NotificationPermissionsReviewService&) = delete;
  NotificationPermissionsReviewService& operator=(
      const NotificationPermissionsReviewService&) = delete;

  ~NotificationPermissionsReviewService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns a list of sites that send a high volume of notifications.
  std::vector<NotificationPermissions> GetNotificationSiteListForReview();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
