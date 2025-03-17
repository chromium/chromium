// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "base/containers/map_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/notifications_engagement_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

namespace {

constexpr char kRevocationResultHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevocationResult";

base::Value UpdateContentSettingValue(
    scoped_refptr<HostContentSettingsMap> hcsm,
    const GURL& url,
    const content_settings::SettingInfo& info,
    base::Value::Dict dict) {
  CHECK(url.is_valid());
  auto constraint = content_settings::ContentSettingConstraints(
      info.metadata.expiration() - info.metadata.lifetime());
  constraint.set_lifetime(info.metadata.lifetime());
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)), constraint);

  return hcsm->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
}

void RemoveContentSettingValue(scoped_refptr<HostContentSettingsMap> hcsm,
                               const GURL& url) {
  CHECK(url.is_valid());
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, {});
}

}  // namespace

DisruptiveNotificationPermissionsManager::
    DisruptiveNotificationPermissionsManager(
        scoped_refptr<HostContentSettingsMap> hcsm,
        site_engagement::SiteEngagementService* site_engagement_service)
    : hcsm_(std::move(hcsm)),
      site_engagement_service_(site_engagement_service) {}

DisruptiveNotificationPermissionsManager::
    ~DisruptiveNotificationPermissionsManager() = default;

void DisruptiveNotificationPermissionsManager::RevokeDisruptiveNotifications() {
  int revoked_sites_count = 0;
  ContentSetting default_notification_setting =
      hcsm_->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS);

  // Get daily average notification count of pattern pairs.
  std::map<std::pair<ContentSettingsPattern, ContentSettingsPattern>, int>
      notification_count_map = permissions::NotificationsEngagementService::
          GetNotificationCountMapPerPatternPair(hcsm_.get());

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

    // Check if a content setting already exists.
    content_settings::SettingInfo info;
    base::Value stored_value = hcsm_->GetWebsiteSetting(
        url, url,
        ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
        &info);
    if (!stored_value.is_none()) {
      CHECK(stored_value.is_dict());
      base::Value::Dict dict = std::move(stored_value).TakeDict();
      auto recorded_score = dict.FindDouble(safety_hub::kSiteEngagementStr);
      if (!recorded_score.has_value()) {
        continue;
      }
      const double new_score = site_engagement_service_->GetScore(url);
      if (recorded_score.value() < new_score) {
        dict.Set(safety_hub::kRevokedStatusDictKeyStr,
                 safety_hub::kFalsePositiveStr);
        UpdateContentSettingValue(hcsm_, url, info, std::move(dict));
        UMA_HISTOGRAM_COUNTS_100(
            "Settings.SafetyHub.DisruptiveNotificationRevocations."
            "FalsePositive.SiteEngagement",
            new_score);
        base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                      RevocationResult::kFalsePositive);
      } else {
        base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                      RevocationResult::kAlreadyInRevokeList);
      }
      continue;
    }
    auto it = notification_count_map.find(
        std::make_pair(item.primary_pattern, item.secondary_pattern));
    int notification_count =
        it != notification_count_map.end() ? it->second : 0;
    if (!IsNotificationDisruptive(url, notification_count)) {
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

    content_settings::ContentSettingConstraints default_constraint(
        clock_->Now());
    default_constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
    StoreRevokedDisruptiveNotificationPermission(url, default_constraint,
                                                 notification_count);
    UMA_HISTOGRAM_COUNTS_100(
        "Settings.SafetyHub.DisruptiveNotificationRevocations.Proposed."
        "NotificationCount",
        notification_count);
    base::UmaHistogramEnumeration(kRevocationResultHistogram,
                                  RevocationResult::kRevoke);
    revoked_sites_count++;
  }
  UMA_HISTOGRAM_COUNTS_100(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "RevokedWebsitesCount",
      revoked_sites_count);
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
        const content_settings::ContentSettingConstraints& constraints,
        int daily_notification_count) {
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
  dict.Set(safety_hub::kSiteEngagementStr,
           site_engagement_service_->GetScore(url));
  dict.Set(safety_hub::kDailyNotificationCountStr, daily_notification_count);
  dict.Set(safety_hub::kTimestampStr, base::TimeToValue(clock_->Now()));
  hcsm_->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)), constraints);
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
  content_settings::SettingInfo info;
  base::Value stored_value = hcsm->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, &info);
  if (stored_value.is_none()) {
    return;
  }
  CHECK(stored_value.is_dict());
  base::Value::Dict dict = std::move(stored_value).TakeDict();
  const bool has_reported_metrics =
      dict.FindBool(safety_hub::kHasReportedMetricsStr).value_or(false);
  if (!has_reported_metrics) {
    ukm::builders::SafetyHub_DisruptiveNotificationRevocations_Proposed(
        source_id)
        .SetDailyAverageVolume(
            dict.FindInt(safety_hub::kDailyNotificationCountStr).value_or(0))
        .SetSiteEngagement(
            dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0))
        .Record(ukm::UkmRecorder::Get());
    // Update the stored content setting value.
    dict.Set(safety_hub::kHasReportedMetricsStr, true);
    stored_value = UpdateContentSettingValue(hcsm, url, info, std::move(dict));
    dict = std::move(stored_value).TakeDict();
  }

  const std::string* revoked_status =
      dict.FindString(safety_hub::kRevokedStatusDictKeyStr);
  if (revoked_status && *revoked_status == safety_hub::kFalsePositiveStr) {
    const base::Value* stored_timestamp = dict.Find(safety_hub::kTimestampStr);
    base::TimeDelta delta_since_revocation =
        base::Time::Now() -
        base::ValueToTime(stored_timestamp).value_or(base::Time::Now());
    ukm::builders::SafetyHub_DisruptiveNotificationRevocations_FalsePositive(
        source_id)
        .SetDaysSinceRevocation(delta_since_revocation.InDays())
        .SetNewSiteEngagement(
            site_engagement::SiteEngagementService::Get(profile)->GetScore(url))
        .SetOldSiteEngagement(
            dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0))
        .Record(ukm::UkmRecorder::Get());
    // Remove the false positive value to avoid repeated triggers.
    RemoveContentSettingValue(hcsm, url);
  }
}
