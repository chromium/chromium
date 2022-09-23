// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/notification_permission_review_service.h"

#include <set>

#include "base/containers/contains.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "url/gurl.h"

namespace permissions {
constexpr char kExcludedKey[] = "exempted";

NotificationPermissions::NotificationPermissions(const url::Origin& origin,
                                                 int notification_count)
    : origin(origin), notification_count(notification_count) {}
NotificationPermissions::~NotificationPermissions() = default;

NotificationPermissionsReviewService::NotificationPermissionsReviewService(
    HostContentSettingsMap* hcsm)
    : hcsm_(hcsm) {}

NotificationPermissionsReviewService::~NotificationPermissionsReviewService() =
    default;

void NotificationPermissionsReviewService::Shutdown() {}

std::vector<NotificationPermissions>
NotificationPermissionsReviewService::GetNotificationSiteListForReview() {
  // TODO(crbug.com/1345920): Apply filters by checking site engagement score
  // and notification count.
  ContentSettingsForOneType ignored_patterns;
  hcsm_->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW, &ignored_patterns);

  std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
      ignored_patterns_set;
  for (auto& item : ignored_patterns) {
    const base::Value& stored_value = item.setting_value;
    bool is_ignored =
        stored_value.is_dict() &&
        stored_value.GetDict().FindBool(kExcludedKey).value_or(false);
    if (is_ignored) {
      ignored_patterns_set.insert(
          {std::move(item.primary_pattern), std::move(item.secondary_pattern)});
    }
  }

  ContentSettingsForOneType permissions;
  hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS,
                               &permissions);
  std::vector<NotificationPermissions> notification_permissions_list;
  for (const auto& item : permissions) {
    if (item.GetContentSetting() != CONTENT_SETTING_ALLOW)
      continue;

    if (!content_settings::PatternAppliesToSingleOrigin(item.primary_pattern,
                                                        item.secondary_pattern))
      continue;

    if (base::Contains(ignored_patterns_set,
                       std::pair{item.primary_pattern, item.secondary_pattern}))
      continue;
    // TODO(crbug.com/1345920): Get notification count from
    // NotificationEngagementService.
    notification_permissions_list.emplace_back(
        url::Origin::Create(GURL(item.primary_pattern.ToString())), 0);
  }

  return notification_permissions_list;
}

void NotificationPermissionsReviewService::
    AddOriginToNotificationPermissionReviewBlocklist(
        const url::Origin& origin) {
  base::Value::Dict permission_dict;
  permission_dict.Set(kExcludedKey, base::Value(true));

  hcsm_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), GURL(),
      ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW,
      base::Value(std::move(permission_dict)));
}

}  // namespace permissions
