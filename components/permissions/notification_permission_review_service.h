// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
#define COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_

#include <vector>

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"

namespace permissions {

struct NotificationPermissions {
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  int notification_count;

  NotificationPermissions(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          int notification_count);
  ~NotificationPermissions();
};

// This class provides data for "Review Notification Permissions" module in site
// settings notification page. This module shows the domains that send a lot of
// notification, but have low engagement.
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

  // Returns a list containing the sites that send a lot of notifications.
  std::vector<NotificationPermissions> GetNotificationSiteListForReview();

  // Add given pattern pair to the blocklist for the "Review notification
  // permission" feature. The patterns in blocklist will not be suggested to be
  // reviewed by the user again.
  void AddPatternToNotificationPermissionReviewBlocklist(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

  // Removes given origin from the blocklist for the "Review notification
  // permission" feature. The pattern may be suggested again for review.
  void RemovePatternFromNotificationPermissionReviewBlocklist(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

 private:
  // Used to update the notification permissions per URL.
  const scoped_refptr<HostContentSettingsMap> hcsm_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
