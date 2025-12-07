// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "base/auto_reset.h"
#include "base/containers/map_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safety_check/safety_check.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/safety_hub/notification_wrapper_android.h"
#endif

namespace {

using RevocationState =
    DisruptiveNotificationPermissionsManager::RevocationState;

constexpr char kRevokedStatusDictKeyStr[] = "revoked_status";
constexpr char kAcknowledgedStr[] = "acknowledged";
constexpr char kIgnoreStr[] = "ignore";
constexpr char kIgnoreInsideSafetyHubStr[] = "ignore_inside_sh";
constexpr char kIgnoreOutsideSafetyHubStr[] = "ignore_outside_sh";
constexpr char kRevokeStr[] = "revoke";
constexpr char kProposedStr[] = "proposed";
constexpr char kSiteEngagementStr[] = "site_engagement";
constexpr char kDailyNotificationCountStr[] = "daily_notification_count";
constexpr char kHasReportedProposalStr[] = "has_reported_proposal";
constexpr char kHasReportedFalsePositiveStr[] = "has_reported_false_positive";
constexpr char kTimestampStr[] = "timestamp";
constexpr char kVersionStr[] = "version";
constexpr char kPageVisitCountStr[] = "page_visit";
constexpr char kNotificationClickCountStr[] = "notification_click_count";

constexpr char kRevocationResultHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevocationResult";

const base::TimeDelta kIgnoreExpirationOutsideSafetyHub = base::Days(90);
const base::TimeDelta kIgnoreExpirationInsideSafetyHub = base::Days(365);

std::optional<RevocationState> GetRevocationState(
    const base::Value::Dict& dict) {
  const std::string* revocation_state =
      dict.FindString(kRevokedStatusDictKeyStr);
  if (!revocation_state) {
    return std::nullopt;
  } else if (*revocation_state == kProposedStr) {
    return RevocationState::kProposed;
  } else if (*revocation_state == kAcknowledgedStr) {
    return RevocationState::kAcknowledged;
  } else if (*revocation_state == kRevokeStr) {
    return RevocationState::kRevoked;
  } else if (*revocation_state == kIgnoreInsideSafetyHubStr ||
             *revocation_state == kIgnoreStr) {
    return RevocationState::kIgnoreInsideSH;
  } else if (*revocation_state == kIgnoreOutsideSafetyHubStr) {
    return RevocationState::kIgnoreOutsideSH;
  } else {
    return std::nullopt;
  }
}

bool IsSiteEngagementScoreLow(double site_engagement_score) {
  return site_engagement_score <=
         features::kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore
             .Get();
}

bool IsDailyNotificationCountHigh(int daily_notification_count) {
  return daily_notification_count >=
         features::
             kSafetyHubDisruptiveNotificationRevocationMinNotificationCount
                 .Get();
}

bool IsNotificationDisruptive(double site_engagement_score,
                              int daily_notification_count) {
  return IsSiteEngagementScoreLow(site_engagement_score) &&
         IsDailyNotificationCountHigh(daily_notification_count);
}

std::string_view GetRevocationStateString(RevocationState revocation_state) {
  switch (revocation_state) {
    case RevocationState::kRevoked:
      return "Revoked";
    case RevocationState::kAcknowledged:
      return "Acknowledged";
    case RevocationState::kIgnoreInsideSH:
    case RevocationState::kIgnoreOutsideSH:
      return "Regranted";
    case RevocationState::kProposed:
      return "Proposed";
  }
}

}  // namespace

DisruptiveNotificationPermissionsManager::RevocationEntry::RevocationEntry(
    RevocationState revocation_state,
    double site_engagement,
    int daily_notification_count,
    base::Time timestamp)
    : revocation_state(revocation_state),
      site_engagement(site_engagement),
      daily_notification_count(daily_notification_count),
      timestamp(timestamp) {}
DisruptiveNotificationPermissionsManager::RevocationEntry::RevocationEntry(
    const DisruptiveNotificationPermissionsManager::RevocationEntry& other) =
    default;
DisruptiveNotificationPermissionsManager::RevocationEntry&
DisruptiveNotificationPermissionsManager::RevocationEntry::operator=(
    const DisruptiveNotificationPermissionsManager::RevocationEntry& other) =
    default;
DisruptiveNotificationPermissionsManager::RevocationEntry::~RevocationEntry() =
    default;

bool DisruptiveNotificationPermissionsManager::RevocationEntry::operator==(
    const RevocationEntry& other) const = default;

DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    ContentSettingHelper(HostContentSettingsMap& hcsm)
    : hcsm_(hcsm) {}

std::vector<
    std::pair<GURL, DisruptiveNotificationPermissionsManager::RevocationEntry>>
DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    GetAllEntries() {
  std::vector<std::pair<GURL, RevocationEntry>> entries;
  for (const auto& item : hcsm_->GetSettingsForOneType(
           ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS)) {
    const GURL& url = item.primary_pattern.ToRepresentativeUrl();
    std::optional<RevocationEntry> revocation_entry =
        ToRevocationEntry(item.setting_value, item.metadata);
    if (revocation_entry) {
      entries.push_back(
          std::make_pair(url, std::move(revocation_entry).value()));
    }
  }
  return entries;
}

std::optional<DisruptiveNotificationPermissionsManager::RevocationEntry>
DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    GetRevocationEntry(const GURL& url) {
  content_settings::SettingInfo info;
  base::Value stored_value = hcsm_->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, &info);
  return ToRevocationEntry(stored_value, info.metadata);
}

std::optional<DisruptiveNotificationPermissionsManager::RevocationEntry>
DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    ToRevocationEntry(const base::Value& value,
                      const content_settings::RuleMetaData& metadata) {
  if (value.is_none() || !value.is_dict()) {
    return std::nullopt;
  }
  const base::Value::Dict& dict = value.GetDict();

  std::optional<RevocationState> revocation_state = GetRevocationState(dict);
  if (!revocation_state) {
    return std::nullopt;
  }

  if (*revocation_state == RevocationState::kProposed &&
      dict.FindInt(kVersionStr).value_or(-1) !=
          features::kSafetyHubDisruptiveNotificationRevocationExperimentVersion
              .Get()) {
    // This is a proposed revocation created for a different version of a
    // revocation experiment. It is outdated, so let's ignore it.
    return std::nullopt;
  }

  RevocationEntry entry(
      /*revocation_state=*/*revocation_state,
      /*site_engagement=*/dict.FindDouble(kSiteEngagementStr).value_or(0),
      /*daily_notification_count=*/
      dict.FindInt(kDailyNotificationCountStr).value_or(0),
      /*timestamp=*/
      base::ValueToTime(dict.Find(kTimestampStr)).value_or(base::Time()));
  entry.has_reported_proposal =
      dict.FindBool(kHasReportedProposalStr).value_or(false);
  entry.has_reported_false_positive =
      dict.FindBool(kHasReportedFalsePositiveStr).value_or(false);
  entry.page_visit_count = dict.FindInt(kPageVisitCountStr).value_or(0);
  entry.notification_click_count =
      dict.FindInt(kNotificationClickCountStr).value_or(0);
  entry.lifetime = metadata.lifetime();

  return entry;
}

void DisruptiveNotificationPermissionsManager::ContentSettingHelper::
    PersistRevocationEntry(const GURL& url, const RevocationEntry& entry) {
  CHECK(url.is_valid());

  std::string_view revocation_state_string;
  base::TimeDelta lifetime;
  base::Value::Dict dict;
  switch (entry.revocation_state) {
    case RevocationState::kProposed:
      revocation_state_string = kProposedStr;
      dict.Set(
          kVersionStr,
          features::kSafetyHubDisruptiveNotificationRevocationExperimentVersion
              .Get());
      lifetime =
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
      break;
    case RevocationState::kRevoked:
      revocation_state_string = kRevokeStr;
      lifetime =
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
      break;
    case RevocationState::kIgnoreInsideSH:
      revocation_state_string = kIgnoreInsideSafetyHubStr;
      lifetime = kIgnoreExpirationInsideSafetyHub;
      break;
    case RevocationState::kIgnoreOutsideSH:
      revocation_state_string = kIgnoreOutsideSafetyHubStr;
      lifetime = kIgnoreExpirationOutsideSafetyHub;
      break;
    case RevocationState::kAcknowledged:
      revocation_state_string = kAcknowledgedStr;
      lifetime =
          safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
      break;
  }
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
  dict.Set(kPageVisitCountStr, entry.page_visit_count);
  dict.Set(kNotificationClickCountStr, entry.notification_click_count);
  content_settings::ContentSettingConstraints constraints(entry.timestamp);
  constraints.set_lifetime(lifetime);
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

DisruptiveNotificationPermissionsManager::
    DisruptiveNotificationPermissionsManager(
        scoped_refptr<HostContentSettingsMap> hcsm,
        site_engagement::SiteEngagementService* site_engagement_service,
        RevokedPermissionsOSNotificationDisplayManager*
            revoked_permissions_notification_display_manager)
    : hcsm_(std::move(hcsm)),
      site_engagement_service_(site_engagement_service),
      revoked_permissions_notification_display_manager_(
          revoked_permissions_notification_display_manager) {
  // TODO(crbug.com/435407894): Remove the migration logic after most of the
  // exceptions were migrated.
  for (auto& [url, revocation_entry] :
       ContentSettingHelper(*hcsm_).GetAllEntries()) {
    if (revocation_entry.revocation_state != RevocationState::kIgnoreInsideSH &&
        revocation_entry.revocation_state !=
            RevocationState::kIgnoreOutsideSH) {
      continue;
    }

    if (revocation_entry.lifetime != base::TimeDelta()) {
      continue;
    }

    // Migrate ignore entries that don't have expiration set. Resaving the entry
    // will update the lifetime.
    ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, revocation_entry);
  }
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

    // Do not revoke if user has ignored abusive revocation.
    if (safety_hub_util::IsAbusiveNotificationRevocationIgnored(hcsm_.get(),
                                                                url)) {
      base::UmaHistogramEnumeration(
          kRevocationResultHistogram,
          RevocationResult::kAbusiveRevocationIgnored);
      continue;
    }

    auto it = notification_count_map.find(
        std::make_pair(item.primary_pattern, item.secondary_pattern));
    const int notification_count =
        it != notification_count_map.end() ? it->second : 0;
    const double site_engagement_score =
        site_engagement_service_->GetScore(url);
    const bool is_disruptive =
        IsNotificationDisruptive(site_engagement_score, notification_count);

    // Now check if we already have a revocation entry for this url and process
    // it.
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
    if (revocation_entry) {
      switch (revocation_entry->revocation_state) {
        case RevocationState::kRevoked:
          // This should never happen, because the content setting is granted.
          // We are in an inconsistent state, so let's clean this up.
          ContentSettingHelper(*hcsm_).DeleteRevocationEntry(url);
          break;
        case RevocationState::kIgnoreInsideSH:
        case RevocationState::kIgnoreOutsideSH:
          base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                        RevocationResult::kIgnore);
          // Ignore this entry, we should not revoke permissions for this url
          // again.
          continue;
        case RevocationState::kProposed:
          if (!is_disruptive) {
            // Not disruptive anymore, clean up proposed revocation.
            base::UmaHistogramCustomCounts(
                base::StrCat(
                    {"Settings.SafetyHub.DisruptiveNotificationRevocations."
                     "NotDisruptiveAnymore.DaysSinceProposedRevocation"}),
                (clock_->Now() - revocation_entry->timestamp).InDays(), 1, 30,
                30);
            if (!IsSiteEngagementScoreLow(site_engagement_score)) {
              base::UmaHistogramCounts100(
                  "Settings.SafetyHub.DisruptiveNotificationRevocations."
                  "NotDisruptiveAnymore.SiteEngagementIncreased",
                  site_engagement_service_->GetScore(url));
            }
            if (!IsDailyNotificationCountHigh(notification_count)) {
              base::UmaHistogramCounts100(
                  "Settings.SafetyHub.DisruptiveNotificationRevocations."
                  "NotDisruptiveAnymore.NotificationCountDecreased",
                  notification_count);
            }

            ContentSettingHelper(*hcsm_).DeleteRevocationEntry(url);
          } else {
            if (!features::kSafetyHubDisruptiveNotificationRevocationShadowRun
                     .Get() &&
                CanRevokeNotifications(url, *revocation_entry)) {
              RevokeNotifications(url, *revocation_entry);
              revoked_anything = true;
            } else {
              base::UmaHistogramEnumeration(
                  kRevocationResultHistogram,
                  RevocationResult::kAlreadyInProposedRevokeList);
            }
            continue;
          }
          break;
        case RevocationState::kAcknowledged:
          // Was revoked in the past, but the user regranted. We can revoke
          // again.
          break;
      }
    }

    if (!is_disruptive) {
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kNotDisruptive);
      continue;
    }

    // Only can revoke notification permissions if ASK is the default setting.
    if (default_notification_setting != CONTENT_SETTING_ASK) {
      base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                    RevocationResult::kNoRevokeDefaultBlock);
      continue;
    }

    RevocationEntry entry(
        /*revocation_state=*/RevocationState::kProposed,
        /*site_engagement=*/site_engagement_service_->GetScore(url),
        /*daily_notification_count=*/notification_count,
        /*timestamp=*/clock_->Now());
    ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, entry);
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

  if (revoked_anything && revoked_permissions_notification_display_manager_) {
    revoked_permissions_notification_display_manager_->DisplayNotification();
  }

  ReportDailyRunMetrics();
}

void DisruptiveNotificationPermissionsManager::ReportDailyRunMetrics() {
  base::Time now = clock_->Now();
  for (const auto& [url, revocation_entry] :
       ContentSettingHelper(*hcsm_).GetAllEntries()) {
    if (now - revocation_entry.timestamp >
        safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold()) {
      // Since ignored entries don't expire while revoked do, report entries
      // only for a limited amount of time in order to ensure that the
      // distribution makes sense.
      continue;
    }

    std::string_view revocation_state =
        GetRevocationStateString(revocation_entry.revocation_state);

    std::string_view site_engagement;
    double score = site_engagement_service_->GetScore(url);
    if (score == 0.0) {
      site_engagement = "0";
    } else if (score <= 1) {
      site_engagement = "1";
    } else if (score <= 2) {
      site_engagement = "2";
    } else if (score <= 3) {
      site_engagement = "3";
    } else if (score <= 4) {
      site_engagement = "4";
    } else if (score <= 5) {
      site_engagement = "5";
    } else if (score <= 7) {
      site_engagement = "6-7";
    } else if (score <= 10) {
      site_engagement = "8-10";
    } else if (score <= 15) {
      site_engagement = "11-15";
    } else {
      site_engagement = ">15";
    }
    base::UmaHistogramCustomCounts(
        base::StrCat({"Settings.SafetyHub.DisruptiveNotificationRevocations."
                      "DailyDistribution.",
                      revocation_state, ".SiteEngagement", site_engagement,
                      ".DaysSinceRevocation"}),
        (now - revocation_entry.timestamp).InDays(), 1, 30, 30);
  }
}

bool DisruptiveNotificationPermissionsManager::CanRevokeNotifications(
    const GURL& url,
    const RevocationEntry& revocation_entry) {
  CHECK_EQ(revocation_entry.revocation_state, RevocationState::kProposed);
  const base::TimeDelta time_since_proposed_revocation =
      clock_->Now() - revocation_entry.timestamp;

  return time_since_proposed_revocation >=
             features::
                 kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed
                     .Get() &&
         (revocation_entry.has_reported_proposal ||
          time_since_proposed_revocation.InDays() >=
              features::
                  kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays
                      .Get());
}

void DisruptiveNotificationPermissionsManager::RevokeNotifications(
    const GURL& url,
    RevocationEntry revocation_entry) {
  const base::TimeDelta delta_since_proposed_revocation =
      clock_->Now() - revocation_entry.timestamp;
  revocation_entry.revocation_state = RevocationState::kRevoked;
  // Reset timestamp so that it reflects the revocation time.
  revocation_entry.timestamp = clock_->Now();
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
  safe_browsing::SafeBrowsingMetricsCollector::
      LogSafeBrowsingNotificationRevocationSourceHistogram(
          safe_browsing::NotificationRevocationSource::
              kDisruptiveAutoRevocation);
}

void DisruptiveNotificationPermissionsManager::OnPermissionChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  if (content_settings::PatternAppliesToSingleOrigin(primary_pattern,
                                                     secondary_pattern)) {
    GURL url = primary_pattern.ToRepresentativeUrl();
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm_).GetRevocationEntry(url);
    // If the user explicitly regranted notification permission and there is a
    // revocation entry for this url, don't revoke for this url anymore in the
    // future. The entry has to be retained for us to remember this.
    if (revocation_entry &&
        revocation_entry->revocation_state == RevocationState::kRevoked &&
        hcsm_->GetContentSetting(url, url,
                                 ContentSettingsType::NOTIFICATIONS) ==
            ContentSetting::CONTENT_SETTING_ALLOW) {
      OnPermissionRegranted(url, *revocation_entry,
                            /*regranted_in_safety_hub=*/false);
      return;
    }
  }

  // Otherwise, the user is manually changing notification permissions for these
  // patterns, so let's clean up any revocation entries that we have for them.
  hcsm_->SetWebsiteSettingCustomScope(
      primary_pattern, secondary_pattern,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, {});
}

bool DisruptiveNotificationPermissionsManager::IsChangingContentSettings() {
  return is_revocation_running_ || is_changing_notification_permission_;
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
  OnPermissionRegranted(url, *revocation_entry,
                        /*regranted_in_safety_hub=*/true);
}

void DisruptiveNotificationPermissionsManager::OnPermissionRegranted(
    const GURL& url,
    RevocationEntry revocation_entry,
    bool regranted_in_safety_hub) {
  revocation_entry.revocation_state = regranted_in_safety_hub
                                          ? RevocationState::kIgnoreInsideSH
                                          : RevocationState::kIgnoreOutsideSH;
  ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, revocation_entry);

  std::string uma_metric_prefix = base::StrCat(
      {"Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant.",
       regranted_in_safety_hub ? "InSafetyHub" : "OutsideSafetyHub", "."});
  base::UmaHistogramCounts100(
      base::StrCat({uma_metric_prefix, "DaysSinceProposedRevocation"}),
      (clock_->Now() - revocation_entry.timestamp).InDays());
  base::UmaHistogramCounts100(
      base::StrCat({uma_metric_prefix, "NewSiteEngagement"}),
      site_engagement_service_->GetScore(url));
  base::UmaHistogramCounts100(
      base::StrCat({uma_metric_prefix, "PreviousNotificationCount"}),
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
      (revocation_entry->revocation_state != RevocationState::kIgnoreInsideSH &&
       revocation_entry->revocation_state !=
           RevocationState::kIgnoreOutsideSH)) {
    return;
  }

  UpdateNotificationPermission(url, ContentSetting::CONTENT_SETTING_DEFAULT);
  revocation_entry->revocation_state = RevocationState::kRevoked;
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
      revocation_entry->revocation_state = RevocationState::kAcknowledged;
      ContentSettingHelper(*hcsm_).PersistRevocationEntry(url,
                                                          *revocation_entry);
    }
  }
}

void DisruptiveNotificationPermissionsManager::RestoreDeletedRevokedPermission(
    const ContentSettingsPattern& primary_pattern,
    content_settings::ContentSettingConstraints constraints) {
  GURL url = primary_pattern.ToRepresentativeUrl();
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm_).GetRevocationEntry(url);

  // If the user is restoring an acknowledged revoked permission then there
  // should be a corresponding acknowledged entry.
  if (!revocation_entry ||
      revocation_entry->revocation_state != RevocationState::kAcknowledged) {
    return;
  }

  revocation_entry->revocation_state = RevocationState::kRevoked;
  ContentSettingHelper(*hcsm_).PersistRevocationEntry(url, *revocation_entry);
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
       revocation_entry->revocation_state != RevocationState::kRevoked)) {
    return;
  }

  base::TimeDelta delta_since_proposed_revocation =
      base::Time::Now() - revocation_entry->timestamp;
  const int days_since_proposed_revocation =
      delta_since_proposed_revocation.InDays();

  const double old_site_engagement_score = revocation_entry->site_engagement;
  const double new_site_engagement_score =
      site_engagement::SiteEngagementService::Get(profile)->GetScore(url);

  // Report that an interaction with a revoked or proposed site has happened.
  ukm::builders::
      SafetyHub_DisruptiveNotificationRevocations_FalsePositiveInteraction(
          source_id)
          .SetDaysSinceRevocation(days_since_proposed_revocation)
          .SetReason(static_cast<int>(reason))
          .SetRevocationState(
              static_cast<int>(revocation_entry->revocation_state))
          .SetNewSiteEngagement(new_site_engagement_score)
          .SetOldSiteEngagement(old_site_engagement_score)
          .SetDailyAverageVolume(revocation_entry->daily_notification_count)
          .Record(ukm::UkmRecorder::Get());

  std::string_view revocation_state =
      GetRevocationStateString(revocation_entry->revocation_state);
  base::UmaHistogramEnumeration(
      base::StrCat({"Settings.SafetyHub.DisruptiveNotificationRevocations.",
                    revocation_state, ".FalsePositiveInteraction"}),
      reason);

  switch (reason) {
    case FalsePositiveReason::kPageVisit:
      revocation_entry->page_visit_count++;
      break;
    case FalsePositiveReason::kPersistentNotificationClick:
    case FalsePositiveReason::kNonPersistentNotificationClick:
      revocation_entry->notification_click_count++;
      break;
  }
  ContentSettingHelper(*hcsm).PersistRevocationEntry(url, *revocation_entry);

  // Only report false positive revocations for already revoked permissions.
  // Proposals for revocation permissions will be cleaned up in the main check
  // if the site is not disruptive anymore.
  if (revocation_entry->revocation_state != RevocationState::kRevoked ||
      revocation_entry->has_reported_false_positive) {
    return;
  }

  // Don't report any false positive revocations after the threshold.
  const int max_days =
      features::kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod
          .Get();
  if (days_since_proposed_revocation > max_days) {
    return;
  }

  // Only report false positive revocations after the min cooldown period has
  // passed.
  const int min_days =
      features::
          kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown
              .Get();
  if (days_since_proposed_revocation < min_days) {
    return;
  }

  if (new_site_engagement_score - old_site_engagement_score <
      features::
          kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta
              .Get()) {
    return;
  }

  // Report as false positive only after the minimum required observation period
  // has passed and the SES increased by the required amount.
  ukm::builders::
      SafetyHub_DisruptiveNotificationRevocations_FalsePositiveRevocation(
          source_id)
          .SetDaysSinceRevocation(days_since_proposed_revocation)
          .SetRevocationState(
              static_cast<int>(revocation_entry->revocation_state))
          .SetPageVisitCount(revocation_entry->page_visit_count)
          .SetNotificationClickCount(revocation_entry->notification_click_count)
          .SetNewSiteEngagement(new_site_engagement_score)
          .SetOldSiteEngagement(old_site_engagement_score)
          .SetDailyAverageVolume(revocation_entry->daily_notification_count)
          .Record(ukm::UkmRecorder::Get());

  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositive.SiteEngagement",
      new_site_engagement_score);
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositive.PageVisitCount",
      revocation_entry->page_visit_count);
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositive.NotificationClickCount",
      revocation_entry->notification_click_count);
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositive.DaysSinceProposedRevocation",
      days_since_proposed_revocation);
  base::UmaHistogramCounts100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositive.DailyAverageVolume",
      revocation_entry->daily_notification_count);

  revocation_entry->has_reported_false_positive = true;
  ContentSettingHelper(*hcsm).PersistRevocationEntry(url, *revocation_entry);
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

// Static
ContentSettingsForOneType
DisruptiveNotificationPermissionsManager::GetRevokedNotifications(
    HostContentSettingsMap* hcsm) {
  ContentSettingsForOneType result;
  ContentSettingsForOneType revoked_permissions = hcsm->GetSettingsForOneType(
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  // Filter only revoked values, skipping proposed, ignore or false positive
  // values.
  for (const auto& revoked_permission : revoked_permissions) {
    const GURL& url = revoked_permission.primary_pattern.ToRepresentativeUrl();
    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm).GetRevocationEntry(url);
    if (revocation_entry &&
        revocation_entry->revocation_state == RevocationState::kRevoked) {
      result.emplace_back(revoked_permission);
    }
  }
  return result;
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

// static
bool DisruptiveNotificationPermissionsManager::
    IsUrlIgnoredForRevokedDisruptiveNotification(HostContentSettingsMap* hcsm,
                                                 const GURL& url) {
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm).GetRevocationEntry(url);

  if (!revocation_entry) {
    return false;
  }
  return revocation_entry->revocation_state ==
             RevocationState::kIgnoreInsideSH ||
         revocation_entry->revocation_state ==
             RevocationState::kIgnoreOutsideSH;
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

