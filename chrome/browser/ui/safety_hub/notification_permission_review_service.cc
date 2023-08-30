// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"

#include <map>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
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
constexpr char kDisplayedKey[] = "display_count";
constexpr char kOrigin[] = "origin";
// The daily average is calculated over the past this many days.
constexpr int kDays = 7;

int ExtractNotificationCount(ContentSettingPatternSource item,
                             std::string date) {
  if (!item.setting_value.is_dict()) {
    return 0;
  }

  base::Value::Dict* bucket = item.setting_value.GetDict().FindDict(date);
  if (!bucket) {
    return 0;
  }
  return bucket->FindInt(kDisplayedKey).value_or(0);
}

int GetDailyAverageNotificationCount(ContentSettingPatternSource item) {
  // Calculate daily average count for the past week.
  base::Time date = base::Time::Now();
  int notification_count_total = 0;

  for (int day = 0; day < kDays; ++day) {
    notification_count_total += ExtractNotificationCount(
        item, permissions::NotificationsEngagementService::GetBucketLabel(
                  date - base::Days(day)));
  }

  return std::ceil(notification_count_total / kDays);
}

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

std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
GetNotificationCountMapPerPatternPair(
    scoped_refptr<HostContentSettingsMap> hcsm) {
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      result;
  for (auto& item : hcsm->GetSettingsForOneType(
           ContentSettingsType::NOTIFICATION_INTERACTIONS)) {
    result[std::pair{item.primary_pattern, item.secondary_pattern}] =
        GetDailyAverageNotificationCount(item);
  }

  return result;
}

bool ShouldAddToNotificationPermissionReviewList(
    site_engagement::SiteEngagementService* service,
    GURL url,
    int notification_count) {
  // The notification permission should be added to the list if one of the
  // criteria below holds:
  // - Site engagement level is NONE OR MINIMAL and average daily notification
  // count is more than 0.
  // - Site engamment level is LOW and average daily notification count is
  // more than 3. Otherwise, the notification permission should not be added
  // to review list.
  double score = service->GetScore(url);
  int low_engagement_notification_limit =
      features::kSafetyCheckNotificationPermissionsLowEnagementLimit.Get();
  bool is_low_engagement =
      !site_engagement::SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::MEDIUM) &&
      notification_count > low_engagement_notification_limit;
  int min_engagement_notification_limit =
      features::kSafetyCheckNotificationPermissionsMinEnagementLimit.Get();
  bool is_minimal_engagement =
      !site_engagement::SiteEngagementService::IsEngagementAtLeast(
          score, blink::mojom::EngagementLevel::LOW) &&
      notification_count > min_engagement_notification_limit;

  return is_minimal_engagement || is_low_engagement;
}

}  // namespace

NotificationPermissions::NotificationPermissions(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    int notification_count)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      notification_count(notification_count) {}
NotificationPermissions::~NotificationPermissions() = default;

NotificationPermissionsReviewService::NotificationPermissionsReviewService(
    HostContentSettingsMap* hcsm)
    : hcsm_(hcsm) {
  content_settings_observation_.Observe(hcsm);
}

NotificationPermissionsReviewService::~NotificationPermissionsReviewService() =
    default;

void NotificationPermissionsReviewService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (!content_type_set.Contains(ContentSettingsType::NOTIFICATIONS)) {
    return;
  }
  // Sites on the notification permission review blocklist are sites where the
  // notification permission is ALLOW and the user has indicated the site should
  // not be suggested again in the module for revocation. A change in the
  // notification permission for such a site (e.g. by the user or by
  // resetting permissions) is considered to be a signal that the site should
  // not longer be ignored, in case the permission is allowed again in the
  // future. Setting ContentSetting to ALLOW when it already is ALLOW will not
  // trigger this function.
  RemovePatternFromNotificationPermissionReviewBlocklist(primary_pattern,
                                                         secondary_pattern);
}

void NotificationPermissionsReviewService::Shutdown() {}

std::vector<NotificationPermissions>
NotificationPermissionsReviewService::GetNotificationSiteListForReview() {
  // Get blocklisted pattern pairs that should not be shown in the review list.
  std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
      ignored_patterns_set = GetIgnoredPatternPairs(hcsm_);

  // Get daily average notification count of pattern pairs.
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      notification_count_map = GetNotificationCountMapPerPatternPair(hcsm_);

  // Get the permissions with notification counts that needs to be reviewed.
  // This list will be filtered based on notification count and site engagement
  // score in the PopulateNotificationPermissionReviewData function.
  std::vector<NotificationPermissions> notification_permissions_list;
  for (auto& item :
       hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS)) {
    std::pair pair(item.primary_pattern, item.secondary_pattern);

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

    int notification_count = notification_count_map[pair];
    notification_permissions_list.emplace_back(
        item.primary_pattern, item.secondary_pattern, notification_count);
  }

  return notification_permissions_list;
}

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

base::Value::List
NotificationPermissionsReviewService::PopulateNotificationPermissionReviewData(
    Profile* profile) {
  base::Value::List result;
  if (!base::FeatureList::IsEnabled(
          features::kSafetyCheckNotificationPermissions)) {
    return result;
  }

  auto notification_permissions = GetNotificationSiteListForReview();

  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile);

  // Sort notification permissions by their priority for surfacing to the user.
  auto notification_permission_ordering =
      [](const NotificationPermissions& left,
         const NotificationPermissions& right) {
        return left.notification_count > right.notification_count;
      };
  std::sort(notification_permissions.begin(), notification_permissions.end(),
            notification_permission_ordering);

  for (const auto& notification_permission : notification_permissions) {
    // Converting primary pattern to GURL should always be valid, since
    // Notification Permission Review list only contains single origins. Those
    // are filtered in
    // NotificationPermissionsReviewService::GetNotificationSiteListForReview.
    GURL url = GURL(notification_permission.primary_pattern.ToString());
    DCHECK(url.is_valid());
    if (!ShouldAddToNotificationPermissionReviewList(
            engagement_service, url,
            notification_permission.notification_count)) {
      continue;
    }

    base::Value::Dict permission;
    permission.Set(kOrigin, notification_permission.primary_pattern.ToString());
    std::string notification_info_string = l10n_util::GetPluralStringFUTF8(
        IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_COUNT_LABEL,
        notification_permission.notification_count);
    permission.Set(kSafetyHubNotificationInfoString, notification_info_string);
    result.Append(std::move(permission));
  }

  return result;
}
