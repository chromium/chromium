// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_enforcelist.h"

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"

namespace {

using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

// Key in the HTTPS_ENFORCED website setting dictionary to indicate whether
// HTTPS-First Mode is enabled on the site.
const char kEnabledKey[] = "enabled";

// Key in the HTTPS_ENFORCED website setting dictionary to store the
// timestamp when HTTPS-First Mode is enabled on the site.
const char kAdditionTimestamp[] = "added_time";

// All SSL decisions are per host (and are shared arcoss schemes), so this
// canonicalizes all hosts into a secure scheme GURL to use with content
// settings. The returned GURL will be the passed in host with an empty path and
// https:// as the scheme.
GURL GetSecureGURLForHost(const std::string& host) {
  std::string url = "https://" + host;
  return GURL(url);
}

}  // namespace

namespace security_interstitials {

HttpsOnlyModeEnforcelist::HttpsOnlyModeEnforcelist(
    HostContentSettingsMap* host_content_settings_map,
    base::Clock* clock)
    : host_content_settings_map_(host_content_settings_map), clock_(clock) {}

HttpsOnlyModeEnforcelist::~HttpsOnlyModeEnforcelist() = default;

void HttpsOnlyModeEnforcelist::EnforceForHost(const std::string& host,
                                              bool is_nondefault_storage) {
  if (is_nondefault_storage) {
    // Decisions for non-default storage partitions are stored in memory only.
    enforce_https_hosts_for_non_default_storage_partitions_.insert(host);
    return;
  }
  DCHECK(!IsEnforcedForUrl(GURL("http://" + host), is_nondefault_storage));

  // We want to count how many HTTPS-enforced hosts accumulate over time, so
  // use a dictionary here.
  GURL url = GetSecureGURLForHost(host);
  base::Value::Dict dict;
  dict.Set(kEnabledKey, true);
  dict.Set(kAdditionTimestamp, base::TimeToValue(clock_->Now()));

  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::HTTPS_ENFORCED,
      base::Value(std::move(dict)));

  // Record metrics when the list of enforced hosts changes.
  RecordSiteEngagementHeuristicState(SiteEngagementHeuristicState::kEnabled);
  RecordMetrics(is_nondefault_storage);
}

void HttpsOnlyModeEnforcelist::UnenforceForHost(const std::string& host,
                                                bool is_nondefault_storage) {
  if (is_nondefault_storage) {
    // Decisions for non-default storage partitions are stored in memory only.
    enforce_https_hosts_for_non_default_storage_partitions_.erase(host);
    return;
  }
  DCHECK(IsEnforcedForUrl(GURL("http://" + host), is_nondefault_storage));

  // We want to count how many HTTPS-enforced hosts accumulate over time, so
  // don't remove the value, just set it to false.
  GURL url = GetSecureGURLForHost(host);
  base::Value value = host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::HTTPS_ENFORCED, nullptr);
  DCHECK(value.is_dict());

  base::Value::Dict& dict = value.GetDict();
  dict.Set(kEnabledKey, false);

  // Record the duration HTTPS was enforced on this host.
  auto* addition_timestamp_string = dict.Find(kAdditionTimestamp);
  auto addition_timestamp = base::ValueToTime(addition_timestamp_string);
  if (addition_timestamp.has_value()) {
    base::TimeDelta duration = clock_->Now() - addition_timestamp.value();
    https_only_mode::RecordSiteEngagementHeuristicEnforcementDuration(duration);
  }

  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::HTTPS_ENFORCED,
      base::Value(std::move(dict)));

  // Record metrics when the list of enforced hosts changes.
  RecordSiteEngagementHeuristicState(SiteEngagementHeuristicState::kDisabled);
  RecordMetrics(is_nondefault_storage);
}

bool HttpsOnlyModeEnforcelist::IsEnforcedForUrl(
    const GURL& url,
    bool is_nondefault_storage) const {
  // HTTPS-First Mode is never auto-enabled for URLs with non-default ports.
  if (!url.port().empty()) {
    return false;
  }
  if (is_nondefault_storage) {
    return base::Contains(
        enforce_https_hosts_for_non_default_storage_partitions_, url.host());
  }

  GURL secure_url = GetSecureGURLForHost(url.host());
  const base::Value value = host_content_settings_map_->GetWebsiteSetting(
      secure_url, secure_url, ContentSettingsType::HTTPS_ENFORCED, nullptr);
  if (!value.is_dict()) {
    return false;
  }
  const auto& dict = value.GetDict();
  return dict.FindBool(kEnabledKey).value_or(false);
}

std::set<GURL> HttpsOnlyModeEnforcelist::GetHosts(
    bool is_nondefault_storage) const {
  std::set<GURL> urls;
  if (is_nondefault_storage) {
    for (const std::string& host :
         enforce_https_hosts_for_non_default_storage_partitions_) {
      urls.insert(GURL("https://" + host));
    }
    return urls;
  }

  for (const ContentSettingPatternSource& rule :
       host_content_settings_map_->GetSettingsForOneType(
           ContentSettingsType::HTTPS_ENFORCED)) {
    GURL url(rule.primary_pattern.ToString());
    if (!url.is_empty()) {
      urls.insert(url);
    }
  }
  return urls;
}

void HttpsOnlyModeEnforcelist::RevokeEnforcements(const std::string& host) {
  GURL url = GetSecureGURLForHost(host);
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::HTTPS_ENFORCED, base::Value());
  // Decisions for non-default storage partitions are stored separately in
  // memory; delete those as well.
  enforce_https_hosts_for_non_default_storage_partitions_.erase(host);
}

void HttpsOnlyModeEnforcelist::Clear(
    base::Time delete_begin,
    base::Time delete_end,
    const HostContentSettingsMap::PatternSourcePredicate& pattern_filter) {
  // This clears accumulated hosts as well.
  host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::HTTPS_ENFORCED, delete_begin, delete_end,
      pattern_filter);
}

void HttpsOnlyModeEnforcelist::ClearEnforcements(base::Time delete_begin,
                                                 base::Time delete_end) {
  Clear(delete_begin, delete_end,
        HostContentSettingsMap::PatternSourcePredicate());
  enforce_https_hosts_for_non_default_storage_partitions_.clear();
}

void HttpsOnlyModeEnforcelist::RecordMetrics(bool is_nondefault_storage) {
  if (is_nondefault_storage) {
    // Don't record metrics for non-default storage.
    return;
  }

  ContentSettingsForOneType output =
      host_content_settings_map_->GetSettingsForOneType(
          ContentSettingsType::HTTPS_ENFORCED);
  size_t accumulated_host_count = output.size();
  size_t current_host_count = base::ranges::count_if(
      output, [](const ContentSettingPatternSource setting) {
        if (!setting.setting_value.is_dict()) {
          return false;
        }
        const auto& dict = setting.setting_value.GetDict();
        return dict.FindBool(kEnabledKey).value_or(false);
      });
  https_only_mode::RecordSiteEngagementHeuristicCurrentHostCounts(
      current_host_count, accumulated_host_count);
}

void HttpsOnlyModeEnforcelist::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace security_interstitials
