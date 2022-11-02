// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/notification_permission_review_service.h"

#include <map>
#include <set>

#include "base/containers/contains.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/notifications_engagement_service.h"

namespace permissions {

constexpr char kExcludedKey[] = "exempted";
constexpr char kDisplayedKey[] = "display_count";
// The daily average is calculated over the past this many days.
constexpr int kDays = 7;

namespace {

int ExtractNotificationCount(ContentSettingPatternSource item,
                             std::string date) {
  if (!item.setting_value.is_dict())
    return 0;

  base::Value::Dict* bucket = item.setting_value.GetDict().FindDict(date);
  if (!bucket)
    return 0;
  return bucket->FindInt(kDisplayedKey).value_or(0);
}

int GetDailyAverageNotificationCount(ContentSettingPatternSource item) {
  // Calculate daily average count for the past week.
  base::Time date = base::Time::Now();
  int notification_count_total = 0;

  for (int day = 0; day < kDays; ++day) {
    notification_count_total += ExtractNotificationCount(
        item,
        NotificationsEngagementService::GetBucketLabel(date - base::Days(day)));
  }

  return std::ceil(notification_count_total / kDays);
}

std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
GetIgnoredPatternPairs(scoped_refptr<HostContentSettingsMap> hcsm) {
  std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>> result;

  ContentSettingsForOneType ignored_patterns;
  hcsm->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, &ignored_patterns);

  for (auto& item : ignored_patterns) {
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
  ContentSettingsForOneType notification_count_list;
  hcsm->GetSettingsForOneType(ContentSettingsType::NOTIFICATION_INTERACTIONS,
                              &notification_count_list);

  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      result;
  for (auto& item : notification_count_list) {
    result[std::pair{item.primary_pattern, item.secondary_pattern}] =
        GetDailyAverageNotificationCount(item);
  }

  return result;
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
    : hcsm_(hcsm) {}

NotificationPermissionsReviewService::~NotificationPermissionsReviewService() =
    default;

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
  // score in SiteSettingsHandler#PopulateNotificationPermissionReviewData
  // function.
  ContentSettingsForOneType notifications;
  hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS,
                               &notifications);

  std::vector<NotificationPermissions> notification_permissions_list;
  for (auto& item : notifications) {
    std::pair pair(item.primary_pattern, item.secondary_pattern);

    // Blocklisted permissions should not be in the review list.
    if (base::Contains(ignored_patterns_set, pair))
      continue;

    // Only granted permissions should be in the review list.
    if (item.GetContentSetting() != CONTENT_SETTING_ALLOW)
      continue;

    // Only URLs that belong to a single origin should be in the review list.
    if (!content_settings::PatternAppliesToSingleOrigin(item.primary_pattern,
                                                        item.secondary_pattern))
      continue;

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

}  // namespace permissions
