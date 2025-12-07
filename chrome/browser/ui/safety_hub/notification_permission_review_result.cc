// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_result.h"

#include <set>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

NotificationPermissions::NotificationPermissions(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    int notification_count)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      notification_count(notification_count) {}
NotificationPermissions::~NotificationPermissions() = default;

NotificationPermissionsReviewResult::NotificationPermissionsReviewResult() =
    default;
NotificationPermissionsReviewResult::~NotificationPermissionsReviewResult() =
    default;

NotificationPermissionsReviewResult::NotificationPermissionsReviewResult(
    const NotificationPermissionsReviewResult&) = default;

void NotificationPermissionsReviewResult::AddNotificationPermission(
    const NotificationPermissions& notification_permission) {
  notification_permissions_.push_back(std::move(notification_permission));
}

base::Value::List
NotificationPermissionsReviewResult::GetSortedListValueForUI() {
  base::Value::List result;

// Setting up the list for UI is done on the Android side.
#if !BUILDFLAG(IS_ANDROID)
  const auto sorted_notification_permissions =
      GetSortedNotificationPermissions();

  // Each entry is a dictionary with origin as key and notification count as
  // value.
  for (const auto& notification_permission : sorted_notification_permissions) {
    base::Value::Dict permission;
    permission.Set(kSafetyHubOriginKey,
                   notification_permission.primary_pattern.ToString());
    std::string notification_info_string = l10n_util::GetPluralStringFUTF8(
        IDS_SETTINGS_SAFETY_CHECK_REVIEW_NOTIFICATION_PERMISSIONS_COUNT_LABEL,
        notification_permission.notification_count);
    permission.Set(kSafetyHubNotificationInfoString, notification_info_string);
    result.Append(std::move(permission));
  }
#endif
  return result;
}

std::vector<NotificationPermissions>
NotificationPermissionsReviewResult::GetSortedNotificationPermissions() {
  // Sort notification permissions by their priority for surfacing to the user.
  auto notification_permission_ordering = [](const auto& left,
                                             const auto& right) {
    return left.notification_count > right.notification_count;
  };
  std::sort(notification_permissions_.begin(), notification_permissions_.end(),
            notification_permission_ordering);

  std::vector<NotificationPermissions> result(notification_permissions_);
  return result;
}

std::set<ContentSettingsPattern>
NotificationPermissionsReviewResult::GetOrigins() const {
  std::set<ContentSettingsPattern> origins;
  for (const auto& permission : notification_permissions_) {
    origins.insert(permission.primary_pattern);
  }
  return origins;
}

std::unique_ptr<SafetyHubResult> NotificationPermissionsReviewResult::Clone()
    const {
  return std::make_unique<NotificationPermissionsReviewResult>(*this);
}

base::Value::Dict NotificationPermissionsReviewResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List notification_permissions;
  for (const auto& permission : notification_permissions_) {
    base::Value::Dict permission_dict;
    permission_dict.Set(kSafetyHubOriginKey,
                        permission.primary_pattern.ToString());
    notification_permissions.Append(std::move(permission_dict));
  }
  result.Set(kSafetyHubNotificationPermissionsReviewResultKey,
             std::move(notification_permissions));
  return result;
}

bool NotificationPermissionsReviewResult::IsTriggerForMenuNotification() const {
  return !notification_permissions_.empty();
}

bool NotificationPermissionsReviewResult::WarrantsNewMenuNotification(
    const base::Value::Dict& previous_result_dict) const {
  std::set<ContentSettingsPattern> old_origins;
  for (const base::Value& permission : *previous_result_dict.FindList(
           kSafetyHubNotificationPermissionsReviewResultKey)) {
    const base::Value::Dict& notification_permission = permission.GetDict();
    old_origins.insert(ContentSettingsPattern::FromString(
        *notification_permission.FindString(kSafetyHubOriginKey)));
  }
  std::set<ContentSettingsPattern> new_origins = GetOrigins();
  return !std::ranges::includes(old_origins, new_origins);
}

std::u16string NotificationPermissionsReviewResult::GetNotificationString()
    const {
  if (notification_permissions_.empty()) {
    return std::u16string();
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_REVIEW_NOTIFICATION_PERMISSIONS_MENU_NOTIFICATION,
      GetOrigins().size());
}

int NotificationPermissionsReviewResult::GetNotificationCommandId() const {
  return IDC_OPEN_SAFETY_HUB;
}
