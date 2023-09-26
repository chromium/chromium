// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_NOTIFICATION_PERMISSION_REVIEW_SERVICE_H_

#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"

constexpr char kSafetyHubNotificationInfoString[] = "notificationInfoString";
constexpr char kSafetyHubNotificationCount[] = "notificationCount";
constexpr char kSafetyHubNotificationPermissionsResultKey[] =
    "notificationPermissions";

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
// This module shows the domains that send a lot of notification, but have low
// engagement.
class NotificationPermissionsReviewService : public KeyedService,
                                             public content_settings::Observer {
 public:
  // The result of the periodic update contains the sites that sent a large
  // number of notifications, along with the number of notifications that they
  // sent. The sites that are added to the review blocklist should not be added
  // here.
  class NotificationPermissionsResult : public SafetyHubService::Result {
   public:
    NotificationPermissionsResult();

    explicit NotificationPermissionsResult(const base::Value::Dict& dict);

    NotificationPermissionsResult(const NotificationPermissionsResult&);
    NotificationPermissionsResult& operator=(
        const NotificationPermissionsResult&) = default;

    ~NotificationPermissionsResult() override;

    // TODO(crbug.com/1443466): Make methods private if they are not required to
    // be public.

    void AddNotificationPermission(ContentSettingsPattern origin,
                                   int notification_count);

    std::vector<std::pair<ContentSettingsPattern, int>>
    GetNotificationPermissions() const;

    std::set<ContentSettingsPattern> GetOrigins() const;

    // SafetyHubService::Result implementation

    std::unique_ptr<SafetyHubService::Result> Clone() const override;

    base::Value::Dict ToDictValue() const override;

    bool IsTriggerForMenuNotification() const override;

    bool WarrantsNewMenuNotification(
        const Result& previousResult) const override;

    std::u16string GetNotificationString() const override;

    int GetNotificationCommandId() const override;

   private:
    std::vector<std::pair<ContentSettingsPattern, int>>
        notification_permissions_;
  };

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

  // TODO(crbug.com/1443466): This will be a SafetyHubService implementation.
  std::unique_ptr<SafetyHubService::Result> GetResultFromDictValue(
      const base::Value::Dict& dict);

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
