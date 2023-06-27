// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_allowlist.h"

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"

namespace {

// Key for the expiration time of a decision in the per-site HTTP allowlist
// content settings dictionary.
const char kHTTPAllowlistExpirationTimeKey[] = "decision_expiration_time";

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

HttpsOnlyModeAllowlist::HttpsOnlyModeAllowlist(
    HostContentSettingsMap* host_content_settings_map,
    base::Clock* clock,
    base::TimeDelta expiration_timeout)
    : host_content_settings_map_(host_content_settings_map),
      clock_(clock),
      expiration_timeout_(expiration_timeout) {}

HttpsOnlyModeAllowlist::~HttpsOnlyModeAllowlist() = default;

void HttpsOnlyModeAllowlist::AllowHttpForHost(const std::string& host,
                                              bool is_nondefault_storage) {
  if (is_nondefault_storage) {
    // Decisions for non-default storage partitions are stored in memory only.
    allowed_http_hosts_for_non_default_storage_partitions_.insert(host);
    return;
  }

  // Store when the HTTP allowlist entry for this host should expire. This value
  // must be stored inside a dictionary as content settings don't support
  // directly storing a string value.
  GURL url = GetSecureGURLForHost(host);
  base::Time expiration_time = clock_->Now() + expiration_timeout_;
  base::Value::Dict dict;
  dict.Set(kHTTPAllowlistExpirationTimeKey, base::TimeToValue(expiration_time));
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::HTTP_ALLOWED,
      base::Value(std::move(dict)));
}

bool HttpsOnlyModeAllowlist::IsHttpAllowedForAnyHost(
    bool is_nondefault_storage) const {
  if (is_nondefault_storage) {
    return !allowed_http_hosts_for_non_default_storage_partitions_.empty();
  }

  ContentSettingsForOneType content_settings_list =
      host_content_settings_map_->GetSettingsForOneType(
          ContentSettingsType::HTTP_ALLOWED);
  return !content_settings_list.empty();
}

bool HttpsOnlyModeAllowlist::IsHttpAllowedForHost(
    const std::string& host,
    bool is_nondefault_storage) const {
  if (is_nondefault_storage) {
    return base::Contains(
        allowed_http_hosts_for_non_default_storage_partitions_, host);
  }

  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  const base::Value value = host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::HTTP_ALLOWED, nullptr);
  if (!value.is_dict()) {
    return false;
  }

  const base::Value* decision_expiration_value =
      value.GetDict().Find(kHTTPAllowlistExpirationTimeKey);
  auto decision_expiration = base::ValueToTime(decision_expiration_value);
  if (decision_expiration <= clock_->Now()) {
    // Allowlist entry has expired.
    return false;
  }

  return true;
}

void HttpsOnlyModeAllowlist::RevokeUserAllowExceptions(
    const std::string& host) {
  GURL url = GetSecureGURLForHost(host);
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::HTTP_ALLOWED, base::Value());
  // Decisions for non-default storage partitions are stored separately in
  // memory; delete those as well.
  allowed_http_hosts_for_non_default_storage_partitions_.erase(host);
}

void HttpsOnlyModeAllowlist::Clear(
    base::Time delete_begin,
    base::Time delete_end,
    const HostContentSettingsMap::PatternSourcePredicate& pattern_filter) {
  host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::HTTP_ALLOWED, delete_begin, delete_end,
      pattern_filter);
}

void HttpsOnlyModeAllowlist::ClearAllowlist(base::Time delete_begin,
                                            base::Time delete_end) {
  Clear(delete_begin, delete_end,
        HostContentSettingsMap::PatternSourcePredicate());
  allowed_http_hosts_for_non_default_storage_partitions_.clear();
}

void HttpsOnlyModeAllowlist::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

}  // namespace security_interstitials
