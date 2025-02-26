// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

class GURL;

namespace site_engagement {
class SiteEngagementService;
}  // namespace site_engagement

// This class keeps track of disruptive notification permissions by checking the
// average daily notification counts and site engagement score.
class DisruptiveNotificationPermissionsManager {
 public:
  explicit DisruptiveNotificationPermissionsManager(
      scoped_refptr<HostContentSettingsMap> hcsm,
      site_engagement::SiteEngagementService* site_engagement_service);

  DisruptiveNotificationPermissionsManager(
      const DisruptiveNotificationPermissionsManager&) = delete;
  DisruptiveNotificationPermissionsManager& operator=(
      const DisruptiveNotificationPermissionsManager&) = delete;

  ~DisruptiveNotificationPermissionsManager();

  // Revokes notification permissions for disruptive sites and records
  // the revoked websites in the content setting.
  void RevokeDisruptiveNotifications();

 private:
  // Whether the notification is disruptive based on the site engagement score
  // for the URL and the daily average notification count.
  bool IsNotificationDisruptive(const GURL& url, int daily_notification_count);

  // Stores the URL in REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS content
  // setting with |constraints|. The content setting value is a dictionary.
  // "revoked_status" key value depends on whether the revocation will actually
  // be performed or only proposed as part of shadow run.
  void StoreRevokedDisruptiveNotificationPermission(
      const GURL& url,
      const content_settings::ContentSettingConstraints& constraints);

  scoped_refptr<HostContentSettingsMap> hcsm_;

  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
