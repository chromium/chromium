// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/site_engagement/content/site_engagement_service.h"

// This class provides data for the "Review Notification Permissions" dialog.
// This module shows the domains that send a lot of notification, but have low
// engagement.
class NotificationPermissionsReviewService : public SafetyHubService,
                                             public content_settings::Observer {
 public:
  explicit NotificationPermissionsReviewService(
      HostContentSettingsMap* hcsm,
      site_engagement::SiteEngagementService* engagement_service);

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

  // SafetyHubService implementation.
  // Returns a weak pointer to the service.
  base::WeakPtr<SafetyHubService> GetAsWeakRef() override;

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
  base::Value::List PopulateNotificationPermissionReviewData();

  // Returns the list of all notification permissions that should be reviewed.
  std::unique_ptr<NotificationPermissionsReviewResult>
  GetNotificationPermissions();

  // Sets the notification permission for the given origin.
  void SetNotificationPermissionsForOrigin(std::string origin,
                                           ContentSetting setting);

 private:
  // SafetyHubService implementation

  // Initializes the latest result to the notifications that should be reviewed.
  std::unique_ptr<SafetyHubResult> InitializeLatestResultImpl() override;

  // Returns the interval at which the repeated updates will be run.
  base::TimeDelta GetRepeatedUpdateInterval() override;

  // For the notification permission review service, there is not background
  // task. Instead, all operations happen on the UI thread.
  base::OnceCallback<std::unique_ptr<SafetyHubResult>()> GetBackgroundTask()
      override;

  // A boilerplate function that returns an empty result.
  static std::unique_ptr<SafetyHubResult> UpdateOnBackgroundThread();

  // Gathers all the sites that sent a lot of notifications, and that the user
  // should review.
  std::unique_ptr<SafetyHubResult> UpdateOnUIThread(
      std::unique_ptr<SafetyHubResult> result) override;

  // Returns |true| when the URL and notification count combination meets the
  // criteria for adding the origin to the review list.
  bool ShouldAddToNotificationPermissionReviewList(GURL url,
                                                   int notification_count);

  // Whether notifications from disruptive sites are revoked. In that case, the
  // notification permission module should be hidden.
  bool IsDisruptiveNotificationRevocationEnabled();

  // Used to determine how often the user engaged with websites.
  raw_ptr<site_engagement::SiteEngagementService> engagement_service_;

  // Used to update the notification permissions per URL.
  const scoped_refptr<HostContentSettingsMap> hcsm_;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  base::WeakPtrFactory<NotificationPermissionsReviewService> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
