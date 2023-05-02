// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_enforcelist.h"

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/values.h"

namespace {

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
    HostContentSettingsMap* host_content_settings_map)
    : host_content_settings_map_(host_content_settings_map) {}

HttpsOnlyModeEnforcelist::~HttpsOnlyModeEnforcelist() = default;

void HttpsOnlyModeEnforcelist::EnforceForHost(const std::string& host,
                                              bool is_nondefault_storage) {
  if (is_nondefault_storage) {
    // Decisions for non-default storage partitions are stored in memory only.
    enforce_https_hosts_for_non_default_storage_partitions_.insert(host);
    return;
  }

  // We don't store when the HTTP enforcelist entry for this host should expire,
  // but we still make this a dict to be future-proof.
  GURL url = GetSecureGURLForHost(host);
  auto dict = std::make_unique<base::Value>(base::Value::Type::DICT);
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::HTTPS_ENFORCED,
      base::Value::FromUniquePtrValue(std::move(dict)));
}

bool HttpsOnlyModeEnforcelist::IsEnforcedForHost(
    const std::string& host,
    bool is_nondefault_storage) const {
  if (is_nondefault_storage) {
    return base::Contains(
        enforce_https_hosts_for_non_default_storage_partitions_, host);
  }

  GURL url = GetSecureGURLForHost(host);
  const base::Value value = host_content_settings_map_->GetWebsiteSetting(
      url, url, ContentSettingsType::HTTPS_ENFORCED, nullptr);
  if (!value.is_dict()) {
    return false;
  }
  return true;
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

}  // namespace security_interstitials
