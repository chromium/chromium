// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "base/auto_reset.h"
#include "base/containers/map_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/safety_hub/notification_wrapper_android.h"
#endif

namespace {

constexpr char kRevokedStatusDictKeyStr[] = "revoked_status";
constexpr char kIgnoreStr[] = "ignore";
constexpr char kRevokeStr[] = "revoke";
constexpr char kProposedStr[] = "proposed";
constexpr char kSiteEngagementStr[] = "site_engagement";
constexpr char kDailyNotificationCountStr[] = "daily_notification_count";
constexpr char kHasReportedProposalStr[] = "has_reported_proposal";
constexpr char kHasReportedFalsePositiveStr[] = "has_reported_false_positive";
constexpr char kTimestampStr[] = "timestamp";

constexpr char kRevocationResultHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevocationResult";

DisruptiveNotificationPermissionsManager::RevocationState GetRevocationState(
    const base::Value::Dict& dict) {
  const std::string* revocation_state =
      dict.FindString(kRevokedStatusDictKeyStr);
  if (!revocation_state) {
    return DisruptiveNotificationPermissionsManager::RevocationState::kNone;
  } else if (*revocation_state == kProposedStr) {
    return DisruptiveNotificationPermissionsManager::RevocationState::kProposed;
  } else if (*revocation_state == kRevokeStr) {
    return DisruptiveNotificationPermissionsManager::RevocationState::kRevoked;
  } else if (*revocation_state == kIgnoreStr) {
    return DisruptiveNotificationPermissionsManager::RevocationState::kIgnore;
  } else {
    return DisruptiveNotificationPermissionsManager::RevocationState::kUnknown;
  }
}

}  // namespace

DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    ContentSettingHelper(HostContentSettingsMap& hcsm)
    : hcsm_(hcsm) {}

std::optional<DisruptiveNotificationPermissionsManager::RevocationEntry>
DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    GetRevocationEntry(const GURL& url) {
  content_settings::SettingInfo info;
  base::Value stored_value = hcsm_->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, &info);
  if (stored_value.is_none() || !stored_value.is_dict()) {
    return std::nullopt;
  }
  base::Value::Dict dict = std::move(stored_value).TakeDict();

  return RevocationEntry{
      .revocation_state = GetRevocationState(dict),
      .site_engagement = dict.FindDouble(kSiteEngagementStr).value_or(0),
      .daily_notification_count =
          dict.FindInt(kDailyNotificationCountStr).value_or(0),
      .timestamp =
          base::ValueToTime(dict.Find(kTimestampStr)).value_or(base::Time()),
      .has_reported_proposal =
          dict.FindBool(kHasReportedProposalStr).value_or(false),
      .has_reported_false_positive =
          dict.FindBool(kHasReportedFalsePositiveStr).value_or(false),
      .created_at = info.metadata.expiration() - info.metadata.lifetime(),
      .lifetime = info.metadata.lifetime(),
  };
}

void DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    PersistRevocationEntry(const GURL& url, const RevocationEntry& entry) {
  CHECK(url.is_valid());

  std::string_view revocation_state_string;
  switch (entry.revocation_state) {
    case DisruptiveNotificationPermissionsManager::RevocationState::kNone:
    case DisruptiveNotificationPermissionsManager::RevocationState::kUnknown:
      // Invalid entry, we won't persist it.
      return;
    case DisruptiveNotificationPermissionsManager::RevocationState::kProposed:
      revocation_state_string = kProposedStr;
      break;
    case DisruptiveNotificationPermissionsManager::RevocationState::kRevoked:
      revocation_state_string = kRevokeStr;
      break;
    case DisruptiveNotificationPermissionsManager::RevocationState::kIgnore:
      revocation_state_string = kIgnoreStr;
      break;
  }
  base::Value::Dict dict;
  dict.Set(kRevokedStatusDictKeyStr, revocation_state_string);
  dict.Set(kSiteEngagementStr, entry.site_engagement);
  dict.Set(kDailyNotificationCountStr, entry.daily_notification_count);
  dict.Set(kTimestampStr, base::TimeToValue(entry.timestamp));
  if (entry.has_reported_proposal) {
    dict.Set(kHasReportedProposalStr, entry.has_reported_proposal);
  }
  if (entry.has_reported_false_positive) {
    dict.Set(kHasReportedFalsePositiveStr, entry.has_reported_false_positive);
  }
  content_settings::ContentSettingConstraints constraints(entry.created_at);
  constraints.set_lifetime(entry.lifetime);
  hcsm_->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)), constraints);
}

void DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    DeleteRevocationEntry(const GURL& url) {
  hcsm_->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, {});
}

DisruptiveNotificationPermissionsManager::SafetyHubNotificationWrapper::
    ~SafetyHubNotificationWrapper() = default;

DisruptiveNotificationPermissionsManager::
    DisruptiveNotificationPermissionsManager(
        scoped_refptr<HostContentSettingsMap> hcsm,
        site_engagement::SiteEngagementService* site_engagement_service)
    : hcsm_(std::move(hcsm)),
      site_engagement_service_(site_engagement_service)
#if BUILDFLAG(IS_ANDROID)
      ,
      notification_wrapper_(std::make_unique<NotificationWrapperAndroid>())
#endif
{
  content_settings_observation_.Observe(hcsm_.get());
}

DisruptiveNotificationPermissionsManager::
    ~DisruptiveNotificationPermissionsManager() = default;

void DisruptiveNotificationPermissionsManager::RevokeDisruptiveNotifications() {
  base::AutoReset<bool> is_revocation_running(&is_revocation_running_, true);

  int proposed_revoked_sites_count = 0;
  ContentSetting default_notification_setting =
      hcsm_->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS);

  // Get daily average notification count of pattern pairs.
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      notification_count_map = permissions::NotificationsEngagementService::
          GetNotificationCountMapPerPatternPair(hcsm_.get());

  bool revoked_anything = false;
  for (const auto& item :
       hcsm_->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS)) {
    // Skip default content setting.
    if (item.primary_pattern == ContentSettingsPattern::Wildcard() &&
        item.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      continue;
    }

    // Only granted permissions can be revoked.
    if (item.GetContentSetting() != CONTENT_SETTING_ALLOW) {
      base::UmaHistogramEnumeration(
          kRevocationResultHistogram,
          RevocationResult::kNotAllowedContentSetting);
      continue;
    }

    // Invalid primary pattern cannot be revoked.
    if (!item.primary_pattern.IsValid()) {
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kInvalidContentSetting);
      continue;
    }

    // Only URLs that belong to a single origin can be revoked.
    if (!content_settings::PatternAppliesToSingleOrigin(
            item.primary_pattern, item.secondary_pattern)) {
      base::UmaHistogramEnumeration(
          kRevocationResultHistogram,
          RevocationResult::kNotSiteScopedContentSetting);
      continue;
    }

    // Only user controlled permissions can be revoked.
    if (content_settings::GetSettingSourceFromProviderType(item.source) !=
        content_settings::SettingSource::kUser) {
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kManagedContentSetting);
      continue;
    }

    // Converting primary pattern to GURL should always be valid, since
    // revocation should only contain single origins.
    GURL url = GURL(item.primary_pattern.ToString());
    CHECK(url.is_valid());

    auto it = notification_count_map.find(
        std::make_pair(item.primary_pattern, item.secondary_pattern));
    int notification_count =
        it != notification_count_map.end() ? it->second : 0;
    if (!IsNotificationDisruptive(url, notification_count)) {
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kNotDisruptive);
      continue;
    }

    // At this point we know that the url is allowed to send notifications and
    // is classified as sending disruptive notifications. Now check if we
    // already have a revocation entry for this url and process it.
    //
    // Note that proposed revocations from previous runs will not actually be
    // revoked if they are not anymore classified are disruptive.
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
    if (revocation_entry) {
      revoked_anything |=
          HandleExistingValueAndMaybeRevoke(url, *revocation_entry);
      continue;
    }

    // Only can revoke notification permissions if ASK is the default setting.
    if (default_notification_setting != CONTENT_SETTING_ASK) {
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kNoRevokeDefaultBlock);
      continue;
    }

    ContentSettingHelper(*hcsm_).PersistRevocationEntry(
        url, RevocationEntry{
                 .revocation_state = RevocationState::kProposed,
                 .site_engagement = site_engagement_service_->GetScore(url),
                 .daily_notification_count = notification_count,
                 .timestamp = clock_->Now(),
             });
    base::UmaHistogramCounts100(
        "Settings.SafetyHub.DisruptiveNotificationRevocations.Proposed."
        "NotificationCount",
        notification_count);
    base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                  RevocationResult::kProposedRevoke);
    proposed_revoked_sites_count++;
  }
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "RevokedWebsitesCount",
      proposed_revoked_sites_count);

  if (revoked_anything) {
    DisplayNotification();
  }
}

bool DisruptiveNotificationPermissionsManager::
    HandleExistingValueAndMaybeRevoke(const GURL& url,
                                      const RevocationEntry& revocation_entry) {
  switch (revocation_entry.revocation_state) {
    case RevocationState::kNone:
    case RevocationState::kUnknown:
    case RevocationState::kRevoked:
      // kNone and kUnknown mean that this is an invalid entry, while kRevoked
      // should never happen, because the content setting is granted. In any of
      // these three cases we are in an inconsistent state, so let's clean this
      // up.
      ContentSettingHelper(*hcsm_).DeleteRevocationEntry(url);
      return false;
    case RevocationState::kIgnore:
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kIgnore);
      return false;
    case RevocationState::kProposed:
      if (!features::kSafetyHubDisruptiveNotificationRevocationShadowRun
               .Get() &&
          CanRevokeNotifications(url, revocation_entry)) {
        RevokeNotifications(url, revocation_entry);
        return true;
      } else {
        base::UmaHistogramEnumeration(
            kRevocationResultHistogram,
            RevocationResult::kAlreadyInProposedRevokeList);
        return false;
      }
  }
}

bool DisruptiveNotificationPermissionsManager::CanRevokeNotifications(
    const GURL& url,
    const RevocationEntry& revocation_entry) {
  CHECK_EQ(revocation_entry.revocation_state, RevocationState::kProposed);
  const int days_since_proposed_revocation =
      (clock_->Now() - revocation_entry.timestamp).InDays();

  return revocation_entry.has_reported_proposal ||
         days_since_proposed_revocation >=
             features::
                 kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays
                     .Get();
}

void DisruptiveNotificationPermissionsManager::RevokeNotifications(
    const GURL& url,
    RevocationEntry revocation_entry) {
  const base::TimeDelta delta_since_proposed_revocation =
      clock_->Now() - revocation_entry.timestamp;
  revocation_entry.revocation_state = RevocationState::kRevoked;
  revocation_entry.created_at = clock_->Now();
  revocation_entry.lifetime = safety_hub_util::GetCleanUpThreshold();
  ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, revocation_entry);
  UpdateNotificationPermission(url, ContentSetting::CONTENT_SETTING_DEFAULT);
  base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                RevocationResult::kRevoke);
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "Revoke.DaysSinceProposedRevocation",
      delta_since_proposed_revocation.InDays());
  base::UmaHistogramBoolean(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "HasReportedMetricsBeforeRevocation",
      revocation_entry.has_reported_proposal);
}

void DisruptiveNotificationPermissionsManager::DisplayNotification() {
  if (notification_wrapper_) {
    notification_wrapper_->DisplayNotification(
        GetRevokedNotifications().size());
  }
}

void DisruptiveNotificationPermissionsManager::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.ContainsAllTypes() ||
      content_type_set.GetType() ==
          ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS) {
    UpdateNotificationCount();
  }
  if (!IsRunning() && !is_changing_notification_permission_ &&
      !content_type_set.ContainsAllTypes() &&
      content_type_set.GetType() == ContentSettingsType::NOTIFICATIONS &&
      content_settings::PatternAppliesToSingleOrigin(primary_pattern,
                                                     secondary_pattern)) {
    GURL url = primary_pattern.ToRepresentativeUrl();
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
    // If the user explicitly regranted notification permission and there is a
    // revocation entry for this url, don't revoke for this url anymore in the
    // future.
    if (revocation_entry &&
        revocation_entry->revocation_state == RevocationState::kRevoked &&
        hcsm_->GetContentSetting(url, url,
                                 ContentSettingsType::NOTIFICATIONS) ==
            ContentSetting::CONTENT_SETTING_ALLOW) {
      OnPermissionRegranted(url, *revocation_entry);
    }
  }
}

void DisruptiveNotificationPermissionsManager::UpdateNotificationCount() {
  // If revocation is currently running there is no point in updating, since
  // the notification will be re-displayed when the revocation completes.
  if (!features::kSafetyHubDisruptiveNotificationRevocationShadowRun.Get() &&
      notification_wrapper_ && !is_revocation_running_) {
    notification_wrapper_->UpdateNotification(GetRevokedNotifications().size());
  }
}

ContentSettingsForOneType
DisruptiveNotificationPermissionsManager::GetRevokedNotifications() {
  ContentSettingsForOneType result;
  ContentSettingsForOneType revoked_permissions = hcsm_->GetSettingsForOneType(
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  // Filter only revoked values, skipping proposed, ignore or false positive
  // values.
  for (const auto& revoked_permission : revoked_permissions) {
    const GURL& url = revoked_permission.primary_pattern.ToRepresentativeUrl();
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
    if (revocation_entry &&
        revocation_entry->revocation_state == RevocationState::kRevoked) {
      result.emplace_back(revoked_permission);
    }
  }
  return result;
}

bool DisruptiveNotificationPermissionsManager::IsRunning() {
  return is_revocation_running_;
}

void DisruptiveNotificationPermissionsManager::RegrantPermissionForUrl(
    const GURL& url) {
  // If the user decides to regrant permissions for `url`, check if it has
  // revoked disruptive notification permissions. If so, allow notification
  // permissions and ignore the `url` from future auto-revocation.
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
  if (!revocation_entry ||
      revocation_entry->revocation_state != RevocationState::kRevoked) {
    return;
  }

  UpdateNotificationPermission(url, ContentSetting::CONTENT_SETTING_ALLOW);
  OnPermissionRegranted(url, *revocation_entry);
}

void DisruptiveNotificationPermissionsManager::OnPermissionRegranted(
    const GURL& url,
    RevocationEntry revocation_entry) {
  revocation_entry.revocation_state = RevocationState::kIgnore;
  // Clear the lifetime so that this won't expire.
  revocation_entry.lifetime = base::TimeDelta();
  ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, revocation_entry);

  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "DaysSinceProposedRevocation",
      (clock_->Now() - revocation_entry.timestamp).InDays());
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "NewSiteEngagement",
      site_engagement_service_->GetScore(url));
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "PreviousNotificationCount",
      revocation_entry.daily_notification_count);
}

void DisruptiveNotificationPermissionsManager::UndoRegrantPermissionForUrl(
    const GURL& url,
    std::set<ContentSettingsType> permission_types,
    content_settings::ContentSettingConstraints constraints) {
  // The user has decided to undo the regranted permission revocation for
  // `url`. Only update the `NOTIFICATIONS` and
  // `REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS` settings if the url had
  // revoked notification permissions.
  if (!permission_types.contains(ContentSettingsType::NOTIFICATIONS)) {
    return;
  }

  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
  if (!revocation_entry ||
      revocation_entry->revocation_state != RevocationState::kIgnore) {
    return;
  }

  UpdateNotificationPermission(url, ContentSetting::CONTENT_SETTING_DEFAULT);
  revocation_entry->revocation_state = RevocationState::kRevoked;
  revocation_entry->created_at =
      constraints.expiration() - constraints.lifetime();
  revocation_entry->lifetime = constraints.lifetime();
  ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, *revocation_entry);
}

void DisruptiveNotificationPermissionsManager::ClearRevokedPermissionsList() {
  ContentSettingsForOneType revoked_permissions = hcsm_->GetSettingsForOneType(
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  for (const auto& revoked_permission : revoked_permissions) {
    const GURL& url = revoked_permission.primary_pattern.ToRepresentativeUrl();
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
    if (revocation_entry &&
        revocation_entry->revocation_state == RevocationState::kRevoked) {
      DeleteRevokedPermissionContentSetting(
          revoked_permission.primary_pattern,
          revoked_permission.secondary_pattern);
    }
  }
}

void DisruptiveNotificationPermissionsManager::
    DeleteRevokedPermissionContentSetting(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern) {
  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, {});
}

void DisruptiveNotificationPermissionsManager::RestoreDeletedRevokedPermission(
    const ContentSettingsPattern& primary_pattern,
    content_settings::ContentSettingConstraints constraints) {
  GURL url = primary_pattern.ToRepresentativeUrl();
  base::Value engagement_as_value = hcsm_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS);
  if (engagement_as_value.is_none() || !engagement_as_value.is_dict()) {
    return;
  }

  ContentSettingHelper(*hcsm_).PersistRevocationEntry(
      url,
      RevocationEntry{
          .revocation_state = RevocationState::kRevoked,
          .site_engagement = site_engagement_service_->GetScore(url),
          .daily_notification_count = permissions::
              NotificationsEngagementService::GetDailyAverageNotificationCount(
                  engagement_as_value.GetDict()),
          .timestamp = clock_->Now(),
          .created_at = constraints.expiration() - constraints.lifetime(),
          .lifetime = constraints.lifetime(),
      });
}

bool DisruptiveNotificationPermissionsManager::IsNotificationDisruptive(
    const GURL& url,
    int daily_notification_count) {
  const bool low_site_engagement_score =
      site_engagement_service_->GetScore(url) <=
      features::kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore
          .Get();
  const bool high_daily_notification_count =
      daily_notification_count >=
      features::kSafetyHubDisruptiveNotificationRevocationMinNotificationCount
          .Get();
  return low_site_engagement_score && high_daily_notification_count;
}

// static
void DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
    Profile* profile,
    const GURL& url,
    FalsePositiveReason reason,
    ukm::SourceId source_id) {
  if (!profile) {
    return;
  }
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  if (!hcsm || !url.is_valid()) {
    return;
  }

  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm).GetRevocationEntry(url);
  if (!revocation_entry ||
      (revocation_entry->revocation_state != RevocationState::kProposed &&
       revocation_entry->revocation_state != RevocationState::kRevoked) ||
      revocation_entry->has_reported_false_positive) {
    return;
  }

  base::TimeDelta delta_since_proposed_revocation =
      base::Time::Now() - revocation_entry->timestamp;
  const int days_since_proposed_revocation =
      delta_since_proposed_revocation.InDays();

  const int min_days =
      features::
          kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown
              .Get();
  const int max_days =
      features::kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod
          .Get();
  if (days_since_proposed_revocation < min_days ||
      days_since_proposed_revocation > max_days) {
    return;
  }

  const double old_site_engagement_score = revocation_entry->site_engagement;
  const double new_site_engagement_score =
      site_engagement::SiteEngagementService::Get(profile)->GetScore(url);
  if (new_site_engagement_score - old_site_engagement_score <
      features::
          kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta
              .Get()) {
    return;
  }

  ukm::builders::SafetyHub_DisruptiveNotificationRevocations_FalsePositive(
      source_id)
      .SetDaysSinceRevocation(days_since_proposed_revocation)
      .SetReason(static_cast<int>(reason))
      .SetRevocationState(static_cast<int>(revocation_entry->revocation_state))
      .SetNewSiteEngagement(new_site_engagement_score)
      .SetOldSiteEngagement(old_site_engagement_score)
      .SetDailyAverageVolume(revocation_entry->daily_notification_count)
      .Record(ukm::UkmRecorder::Get());
  revocation_entry->has_reported_false_positive = true;
  ContentSettingHelper(*hcsm).PersistRevocationEntry(url, *revocation_entry);

  base::UmaHistogramEnumeration(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.FalsePositive",
      reason);  // kPageVisit or kNotificationClick
}

// static
void DisruptiveNotificationPermissionsManager::LogMetrics(
    Profile* profile,
    const GURL& url,
    ukm::SourceId source_id) {
  if (!profile) {
    return;
  }
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  if (!hcsm || !url.is_valid()) {
    return;
  }

  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm).GetRevocationEntry(url);
  if (!revocation_entry ||
      revocation_entry->revocation_state != RevocationState::kProposed ||
      revocation_entry->has_reported_proposal) {
    return;
  }
  ukm::builders::SafetyHub_DisruptiveNotificationRevocations_Proposed(source_id)
      .SetDailyAverageVolume(revocation_entry->daily_notification_count)
      .SetSiteEngagement(revocation_entry->site_engagement)
      .Record(ukm::UkmRecorder::Get());

  // Update the stored content setting value.
  revocation_entry->has_reported_proposal = true;
  ContentSettingHelper(*hcsm).PersistRevocationEntry(url, *revocation_entry);
}

// static
bool DisruptiveNotificationPermissionsManager::
    IsUrlRevokedDisruptiveNotification(HostContentSettingsMap* hcsm,
                                       const GURL& url) {
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm).GetRevocationEntry(url);
  return revocation_entry &&
         revocation_entry->revocation_state == RevocationState::kRevoked;
}

void DisruptiveNotificationPermissionsManager::UpdateNotificationPermission(
    const GURL& url,
    ContentSetting setting_value) {
  base::AutoReset<bool> is_changing_notification_permission(
      &is_changing_notification_permission_, true);

  hcsm_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      setting_value);
}

void DisruptiveNotificationPermissionsManager::SetClockForTesting(
    base::Clock* clock) {
  clock_ = clock;
}

void DisruptiveNotificationPermissionsManager::SetNotificationWrapperForTesting(
    std::unique_ptr<SafetyHubNotificationWrapper> wrapper) {
  notification_wrapper_ = std::move(wrapper);
}
