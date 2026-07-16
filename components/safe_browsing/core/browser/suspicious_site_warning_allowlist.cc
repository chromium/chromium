// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"

#include <utility>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace safe_browsing {

namespace {

// Given a hostname, returns its GURL with the http scheme (e.g.,
// "http://www.foo.com"). This matches the convention used by real-time URL
// check caching in VerdictCacheManager (GetHostNameWithHTTPScheme).
GURL GetHostNameWithHTTPScheme(const std::string& host) {
  std::string result(url::kHttpScheme);
  result.append(url::kStandardSchemeSeparator).append(host);
  return GURL(result);
}

}  // namespace

SuspiciousSiteWarningAllowlist::SuspiciousSiteWarningAllowlist(
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : SuspiciousSiteWarningAllowlist(std::move(host_content_settings_map),
                                     kDefaultExpirationTimeout) {}

SuspiciousSiteWarningAllowlist::SuspiciousSiteWarningAllowlist(
    scoped_refptr<HostContentSettingsMap> host_content_settings_map,
    base::TimeDelta expiration_timeout)
    : host_content_settings_map_(std::move(host_content_settings_map)),
      expiration_timeout_(expiration_timeout) {}

SuspiciousSiteWarningAllowlist::~SuspiciousSiteWarningAllowlist() = default;

void SuspiciousSiteWarningAllowlist::AllowSiteForHost(const std::string& host) {
  if (!host_content_settings_map_) {
    return;
  }

  GURL url = GetHostNameWithHTTPScheme(host);
  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(expiration_timeout_);

  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::SUSPICIOUS_SITE_WARNING_DATA,
      base::Value(base::DictValue()), constraints);
}

bool SuspiciousSiteWarningAllowlist::IsSiteAllowedForHost(
    const std::string& host) const {
  if (!host_content_settings_map_) {
    return false;
  }

  GURL url = GetHostNameWithHTTPScheme(host);
  const base::Value value = host_content_settings_map_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::SUSPICIOUS_SITE_WARNING_DATA, nullptr);

  return value.is_dict();
}

void SuspiciousSiteWarningAllowlist::RevokeUserAllowException(
    const std::string& host) {
  if (!host_content_settings_map_) {
    return;
  }

  GURL url = GetHostNameWithHTTPScheme(host);
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::SUSPICIOUS_SITE_WARNING_DATA,
      base::Value());
}

void SuspiciousSiteWarningAllowlist::Clear(
    base::Time delete_begin,
    base::Time delete_end,
    const HostContentSettingsMap::PatternSourcePredicate& pattern_filter) {
  if (!host_content_settings_map_) {
    return;
  }
  host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::SUSPICIOUS_SITE_WARNING_DATA, delete_begin,
      delete_end, pattern_filter);
}

void SuspiciousSiteWarningAllowlist::Clear(base::Time delete_begin,
                                           base::Time delete_end) {
  Clear(delete_begin, delete_end,
        HostContentSettingsMap::PatternSourcePredicate());
}

}  // namespace safe_browsing
