// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
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
constexpr char kDisplayedKey[] = "display_count";
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

}  // namespace

NotificationPermissions::NotificationPermissions(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    int notification_count)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      notification_count(notification_count) {}
NotificationPermissions::~NotificationPermissions() = default;

NotificationPermissionsReviewService::NotificationPermissionsResult::
    NotificationPermissionsResult() = default;
NotificationPermissionsReviewService::NotificationPermissionsResult::
    ~NotificationPermissionsResult() = default;

NotificationPermissionsReviewService::NotificationPermissionsResult::
    NotificationPermissionsResult(const NotificationPermissionsResult&) =
        default;

void NotificationPermissionsReviewService::NotificationPermissionsResult::
    AddNotificationPermission(
        const NotificationPermissions& notification_permission) {
  notification_permissions_.push_back(std::move(notification_permission));
}

base::Value::List NotificationPermissionsReviewService::
    NotificationPermissionsResult::GetSortedListValueForUI() {
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

std::vector<NotificationPermissions> NotificationPermissionsReviewService::
    NotificationPermissionsResult::GetSortedNotificationPermissions() {
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

std::set<ContentSettingsPattern> NotificationPermissionsReviewService::
    NotificationPermissionsResult::GetOrigins() const {
  std::set<ContentSettingsPattern> origins;
  for (NotificationPermissions permission : notification_permissions_) {
    origins.insert(permission.primary_pattern);
  }
  return origins;
}

std::unique_ptr<SafetyHubService::Result>
NotificationPermissionsReviewService::NotificationPermissionsResult::Clone()
    const {
  return std::make_unique<NotificationPermissionsResult>(*this);
}

base::Value::Dict NotificationPermissionsReviewService::
    NotificationPermissionsResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List notification_permissions;
  for (NotificationPermissions permission : notification_permissions_) {
    base::Value::Dict permission_dict;
    permission_dict.Set(kSafetyHubOriginKey,
                        permission.primary_pattern.ToString());
    notification_permissions.Append(std::move(permission_dict));
  }
  result.Set(kSafetyHubNotificationPermissionsResultKey,
             std::move(notification_permissions));
  return result;
}

bool NotificationPermissionsReviewService::NotificationPermissionsResult::
    IsTriggerForMenuNotification() const {
  return !notification_permissions_.empty();
}

bool NotificationPermissionsReviewService::NotificationPermissionsResult::
    WarrantsNewMenuNotification(
        const base::Value::Dict& previous_result_dict) const {
  std::set<ContentSettingsPattern> old_origins;
  for (const base::Value& permission : *previous_result_dict.FindList(
           kSafetyHubNotificationPermissionsResultKey)) {
    const base::Value::Dict& notification_permission = permission.GetDict();
    old_origins.insert(ContentSettingsPattern::FromString(
        *notification_permission.FindString(kSafetyHubOriginKey)));
  }
  std::set<ContentSettingsPattern> new_origins = GetOrigins();
  return !base::ranges::includes(old_origins, new_origins);
}

std::u16string NotificationPermissionsReviewService::
    NotificationPermissionsResult::GetNotificationString() const {
  if (notification_permissions_.empty()) {
    return std::u16string();
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_REVIEW_NOTIFICATION_PERMISSIONS_MENU_NOTIFICATION,
      GetOrigins().size());
}

int NotificationPermissionsReviewService::NotificationPermissionsResult::
    GetNotificationCommandId() const {
  return IDC_OPEN_SAFETY_HUB;
}

NotificationPermissionsReviewService::NotificationPermissionsReviewService(
    HostContentSettingsMap* hcsm,
    site_engagement::SiteEngagementService* engagement_service)
    : engagement_service_(engagement_service), hcsm_(hcsm) {
  content_settings_observation_.Observe(hcsm);

  if (!base::FeatureList::IsEnabled(features::kSafetyHub)) {
    return;
  }

  // TODO(crbug.com/40267370): Because there is only an UI thread for this
  // service, calling both |StartRepeatedUpdates()| and
  // |InitializeLatestResult()| will result in the result being calculated twice
  // when the service starts. When redesigning SafetyHubService, that should be
  // avoided.
  StartRepeatedUpdates();
  InitializeLatestResult();
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
    SetLatestResult(
        UpdateOnUIThread(std::make_unique<NotificationPermissionsResult>()));
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
    SetLatestResult(
        UpdateOnUIThread(std::make_unique<NotificationPermissionsResult>()));
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

std::unique_ptr<SafetyHubService::Result>
NotificationPermissionsReviewService::UpdateOnUIThread(
    std::unique_ptr<SafetyHubService::Result> interim_result) {
  // Get blocklisted pattern pairs that should not be shown in the review list.
  std::set<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
      ignored_patterns_set = GetIgnoredPatternPairs(hcsm_);

  // Get daily average notification count of pattern pairs.
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      notification_count_map = GetNotificationCountMapPerPatternPair(hcsm_);

  // Get the permissions with notification counts that needs to be reviewed.
  // This list is filtered based on notification count and site engagement
  // score.
  auto result = std::make_unique<NotificationPermissionsResult>();
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

std::unique_ptr<NotificationPermissionsReviewService::Result>
NotificationPermissionsReviewService::GetNotificationPermissions() {
  // Return the cached result, which is kept in sync with the values on disk
  // (i.e. HCSM), when available. Otherwise, re-calculate the result.
  return GetCachedResult().value_or(
      UpdateOnUIThread(std::make_unique<NotificationPermissionsResult>()));
}

base::Value::List NotificationPermissionsReviewService::
    PopulateNotificationPermissionReviewData() {
  return (static_cast<NotificationPermissionsResult*>(
              GetNotificationPermissions().get()))
      ->GetSortedListValueForUI();
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

base::OnceCallback<std::unique_ptr<SafetyHubService::Result>()>
NotificationPermissionsReviewService::GetBackgroundTask() {
  return base::BindOnce(&UpdateOnBackgroundThread);
}

// static
std::unique_ptr<SafetyHubService::Result>
NotificationPermissionsReviewService::UpdateOnBackgroundThread() {
  // Return an empty result.
  return std::make_unique<NotificationPermissionsResult>();
}

std::unique_ptr<SafetyHubService::Result>
NotificationPermissionsReviewService::InitializeLatestResultImpl() {
  return UpdateOnUIThread(std::make_unique<NotificationPermissionsResult>());
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
