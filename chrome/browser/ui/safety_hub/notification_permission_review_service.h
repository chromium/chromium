// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_

#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"

constexpr char kSafetyHubNotificationInfoString[] = "notificationInfoString";

struct NotificationPermissions {
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
  int notification_count;

  NotificationPermissions(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          int notification_count);
  ~NotificationPermissions();
};

// This class provides data for the "Review Notification Permissions" dialog.
/// This module shows the domains that send a lot of notification, but have low
// engagement.
class NotificationPermissionsReviewService : public KeyedService,
                                             public content_settings::Observer {
 public:
  explicit NotificationPermissionsReviewService(HostContentSettingsMap* hcsm);

  NotificationPermissionsReviewService(
      const NotificationPermissionsReviewService&) = delete;
  NotificationPermissionsReviewService& operator=(
      const NotificationPermissionsReviewService&) = delete;

  ~NotificationPermissionsReviewService() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

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

  // Returns a sorted list with the notification count for each domain to be
  // shown on the 'Review Notification Permissions' dialog. Those domains send a
  // lot of notifications, but have low site engagement.
  base::Value::List PopulateNotificationPermissionReviewData(Profile* profile);

 private:
  // Used to update the notification permissions per URL.
  const scoped_refptr<HostContentSettingsMap> hcsm_;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
