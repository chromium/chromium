// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kExcludedKey[] = "exempted";

// Engagement limits notification permissions module.
const int kMinEngagementNotificationLimit = 0;
const int kLowEngagementNotificationLimit = 4;

std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
GetIgnoredPatternPairs(scoped_refptr<HostContentSettingsMap> hcsm) {
  std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>> result;

  for (auto& item : hcsm->GetSettingsForOneType(
           ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW)) {
    const base::Value& stored_value = item.setting_value;
    bool is_ignored =
        stored_value.is_dict() &&
        stored_value.GetDict().FindBool(kExcludedKey).value_or(false);
    if (is_ignored) {
      result.insert(
          {std::move(item.primary_pattern), std::move(item.secondary_pattern)});
    }
  }

  return result;
}

}  // namespace

NotificationPermissionsReviewService::NotificationPermissionsReviewService(
    HostContentSettingsMap* hcsm,
    site_engagement::SiteEngagementService* engagement_service)
    : engagement_service_(engagement_service), hcsm_(hcsm) {
  content_settings_observation_.Observe(hcsm);

  // Disruptive notification revocation overlaps with the notification review
  // module. Disable this module when the disruptive revocation is running.
  if (!IsDisruptiveNotificationRevocationEnabled()) {
    // TODO(crbug.com/40267370): Because there is only a UI thread for this
    // service, calling both |StartRepeatedUpdates()| and
    // |InitializeLatestResult()| will result in the result being calculated
    // twice when the service starts. When redesigning SafetyHubService, that
    // should be avoided.
    StartRepeatedUpdates();
    InitializeLatestResult();
  }
}

NotificationPermissionsReviewService::~NotificationPermissionsReviewService() =
    default;

void NotificationPermissionsReviewService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(
          ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW)) {
    // In order to keep the latest result updated with the actual list that
    // should be reviewed, the latest result should be updated here. This is
    // triggered whenever an update is made to the ignore list. For other
    // updates on notification permissions,
    SetLatestResult(UpdateOnUIThread(
        std::make_unique<NotificationPermissionsReviewResult>()));
    return;
  }
  if (content_type_set.Contains(ContentSettingsType::NOTIFICATIONS)) {
    // Sites on the notification permission review blocklist are sites where the
    // notification permission is ALLOW and the user has indicated the site
    // should not be suggested again in the module for revocation. A change in
    // the notification permission for such a site (e.g. by the user or by
    // resetting permissions) is considered to be a signal that the site should
    // not longer be ignored, in case the permission is allowed again in the
    // future. Setting ContentSetting to ALLOW when it already is ALLOW will not
    // trigger this function.
    RemovePatternFromNotificationPermissionReviewBlocklist(primary_pattern,
                                                           secondary_pattern);
    // Update the result since the permission might have been revoked without
    // being on the ignore list and therefore wouldn't cause another
    // OnContentSettingChanged() event.
    SetLatestResult(UpdateOnUIThread(
        std::make_unique<NotificationPermissionsReviewResult>()));
    return;
  }
}

void NotificationPermissionsReviewService::Shutdown() {}

void NotificationPermissionsReviewService::
    AddPatternToNotificationPermissionReviewBlocklist(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern) {
  base::Value::Dict permission_dict;
  permission_dict.Set(kExcludedKey, base::Value(true));

  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW,
      base::Value(std::move(permission_dict)));
}

void NotificationPermissionsReviewService::
    RemovePatternFromNotificationPermissionReviewBlocklist(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern) {
  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, {});
}

std::unique_ptr<SafetyHubResult>
NotificationPermissionsReviewService::UpdateOnUIThread(
    std::unique_ptr<SafetyHubResult> interim_result) {
  // Get blocklisted pattern pairs that should not be shown in the review list.
  std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
      ignored_patterns_set = GetIgnoredPatternPairs(hcsm_);

  // Get daily average notification count of pattern pairs.
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      notification_count_map = permissions::NotificationsEngagementService::
          GetNotificationCountMapPerPatternPair(hcsm_.get());

  // Get the permissions with notification counts that needs to be reviewed.
  // This list is filtered based on notification count and site engagement
  // score.
  auto result = std::make_unique<NotificationPermissionsReviewResult>();
  for (auto& item :
       hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS)) {
    std::pair pair(item.primary_pattern, item.secondary_pattern);

    // Invalid primary pattern should not be in the review list.
    if (!item.primary_pattern.IsValid()) {
      continue;
    }

    // Blocklisted permissions should not be in the review list.
    if (base::Contains(ignored_patterns_set, pair)) {
      continue;
    }

    // Only granted permissions should be in the review list.
    if (item.GetContentSetting() != CONTENT_SETTING_ALLOW) {
      continue;
    }

    // Only URLs that belong to a single origin should be in the review list.
    if (!content_settings::PatternAppliesToSingleOrigin(
            item.primary_pattern, item.secondary_pattern)) {
      continue;
    }

    // Converting primary pattern to GURL should always be valid, since
    // Notification Permission Review list only contains single origins.
    GURL url = GURL(item.primary_pattern.ToString());
    DCHECK(url.is_valid());

    int notification_count = notification_count_map[pair];
    if (!ShouldAddToNotificationPermissionReviewList(url, notification_count)) {
      continue;
    }

    NotificationPermissions notification_permission(
        item.primary_pattern, item.secondary_pattern, notification_count);

    result->AddNotificationPermission(notification_permission);
  }

  return result;
}

std::unique_ptr<NotificationPermissionsReviewResult>
NotificationPermissionsReviewService::GetNotificationPermissions() {
  if (IsDisruptiveNotificationRevocationEnabled()) {
    return std::make_unique<NotificationPermissionsReviewResult>();
  }
  // Return the cached result, which is kept in sync with the values on disk
  // (i.e. HCSM), when available. Otherwise, re-calculate the result.
  auto result = GetCachedResult().value_or(UpdateOnUIThread(
      std::make_unique<NotificationPermissionsReviewResult>()));
  return base::WrapUnique<NotificationPermissionsReviewResult>(
      static_cast<NotificationPermissionsReviewResult*>(result.release()));
}

base::Value::List NotificationPermissionsReviewService::
    PopulateNotificationPermissionReviewData() {
  return (GetNotificationPermissions().get())->GetSortedListValueForUI();
}

void NotificationPermissionsReviewService::SetNotificationPermissionsForOrigin(
    std::string origin,
    ContentSetting setting) {
  hcsm_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(origin),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      setting);
}

base::TimeDelta
NotificationPermissionsReviewService::GetRepeatedUpdateInterval() {
  return base::Days(1);
}

base::OnceCallback<std::unique_ptr<SafetyHubResult>()>
NotificationPermissionsReviewService::GetBackgroundTask() {
  return base::BindOnce(&UpdateOnBackgroundThread);
}

// static
std::unique_ptr<SafetyHubResult>
NotificationPermissionsReviewService::UpdateOnBackgroundThread() {
  // Return an empty result.
  return std::make_unique<NotificationPermissionsReviewResult>();
}

std::unique_ptr<SafetyHubResult>
NotificationPermissionsReviewService::InitializeLatestResultImpl() {
  return UpdateOnUIThread(
      std::make_unique<NotificationPermissionsReviewResult>());
}

base::WeakPtr<SafetyHubService>
NotificationPermissionsReviewService::GetAsWeakRef() {
  return weak_factory_.GetWeakPtr();
}

bool NotificationPermissionsReviewService::
    ShouldAddToNotificationPermissionReviewList(GURL url,
                                                int notification_count) {
  // The notification permission should be added to the list if one of the
  // criteria below holds:
  // - Site engagement level is NONE OR MINIMAL and average daily notification
  // count is more than 0.
  // - Site engamment level is LOW and average daily notification count is
  // more than 3. Otherwise, the notification permission should not be added
  // to review list.
  double score = engagement_service_->GetScore(url);
  bool is_low_engagement =
      !site_engagement::SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::MEDIUM) &&
      notification_count > kLowEngagementNotificationLimit;
  bool is_minimal_engagement =
      !site_engagement::SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::LOW) &&
      notification_count > kMinEngagementNotificationLimit;

  return is_minimal_engagement || is_low_engagement;
}

bool NotificationPermissionsReviewService::
    IsDisruptiveNotificationRevocationEnabled() {
  return base::FeatureList::IsEnabled(
             features::kSafetyHubDisruptiveNotificationRevocation) &&
         !features::kSafetyHubDisruptiveNotificationRevocationShadowRun.Get();
}
