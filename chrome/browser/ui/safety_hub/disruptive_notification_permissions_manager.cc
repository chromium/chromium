// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "base/containers/map_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "url/gurl.h"

DisruptiveNotificationPermissionsManager::
    DisruptiveNotificationPermissionsManager(
        scoped_refptr<HostContentSettingsMap> hcsm,
        site_engagement::SiteEngagementService* site_engagement_service)
    : hcsm_(std::move(hcsm)),
      site_engagement_service_(site_engagement_service) {}

DisruptiveNotificationPermissionsManager::
    ~DisruptiveNotificationPermissionsManager() = default;

void DisruptiveNotificationPermissionsManager::RevokeDisruptiveNotifications() {
  ContentSetting default_notification_setting =
      hcsm_->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS);
  // Only can revoke notification permissions if ASK is the default setting.
  if (default_notification_setting != CONTENT_SETTING_ASK) {
    return;
  }

  // Get daily average notification count of pattern pairs.
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      notification_count_map = permissions::NotificationsEngagementService::
          GetNotificationCountMapPerPatternPair(hcsm_.get());

  for (const auto& item :
       hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS)) {
    // Only granted permissions can be revoked.
    if (item.GetContentSetting() != CONTENT_SETTING_ALLOW) {
      continue;
    }

    // Invalid primary pattern cannot be revoked.
    if (!item.primary_pattern.IsValid()) {
      continue;
    }

    // Only URLs that belong to a single origin can be revoked.
    if (!content_settings::PatternAppliesToSingleOrigin(
            item.primary_pattern, item.secondary_pattern)) {
      continue;
    }

    // Only user controlled permissions can be revoked.
    if (content_settings::GetSettingSourceFromProviderType(item.source) !=
        content_settings::SettingSource::kUser) {
      continue;
    }

    // Converting primary pattern to GURL should always be valid, since
    // revocation should only contain single origins.
    GURL url = GURL(item.primary_pattern.ToString());
    CHECK(url.is_valid());

    // Check if a content setting already exists.
    base::Value stored_value(hcsm_->GetWebsiteSetting(
        url, url,
        ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS));
    if (!stored_value.is_none()) {
      CHECK(stored_value.is_dict());
      continue;
    }

    auto* notification_count = base::FindOrNull(
        notification_count_map,
        std::make_pair(item.primary_pattern, item.secondary_pattern));
    if (notification_count &&
        !IsNotificationDisruptive(url, *notification_count)) {
      continue;
    }

    content_settings::ContentSettingConstraints default_constraint(
        clock_->Now());
    default_constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
    StoreRevokedDisruptiveNotificationPermission(url, default_constraint);
  }
}

bool DisruptiveNotificationPermissionsManager::IsNotificationDisruptive(
    const GURL& url,
    int daily_notification_count) {
  const bool low_site_engagement_score =
      site_engagement_service_->GetScore(url) <=
      safe_browsing::
          kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore.Get();
  const bool high_daily_notification_count =
      daily_notification_count >=
      safe_browsing::
          kSafetyHubDisruptiveNotificationRevocationMinNotificationCount.Get();
  return low_site_engagement_score && high_daily_notification_count;
}

void DisruptiveNotificationPermissionsManager::
    StoreRevokedDisruptiveNotificationPermission(
        const GURL& url,
        const content_settings::ContentSettingConstraints& constraints) {
  // The url should be valid as it is checked that the pattern represents a
  // single origin.
  CHECK(url.is_valid());

  base::Value::Dict dict;
  if (safe_browsing::kSafetyHubDisruptiveNotificationRevocationShadowRun
          .Get()) {
    dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kProposedStr);
  } else {
    // TODO(crbug.com/397363276): Set ignore or revoke value.
  }
  hcsm_->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)), constraints);
}
