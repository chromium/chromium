// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"

#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/features.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "url/gurl.h"

namespace content_settings {

// static
bool CookieSettingsBase::IsThirdPartyRequest(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) {
  net::StaticCookiePolicy policy(
      net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);
  return policy.CanAccessCookies(url, site_for_cookies) != net::OK;
}

// static
GURL CookieSettingsBase::GetFirstPartyURL(
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* top_frame_origin) {
  return top_frame_origin != nullptr ? top_frame_origin->GetURL()
                                     : site_for_cookies.RepresentativeUrl();
}

bool CookieSettingsBase::ShouldDeleteCookieOnExit(
    const ContentSettingsForOneType& cookie_settings,
    const std::string& domain,
    bool is_https) const {
  GURL origin = net::cookie_util::CookieOriginToURL(domain, is_https);
  ContentSetting setting = GetCookieSetting(origin, origin, nullptr);
  DCHECK(IsValidSetting(setting));
  if (setting == CONTENT_SETTING_ALLOW)
    return false;
  // Non-secure cookies are readable by secure sites. We need to check for
  // https pattern if http is not allowed. The section below is independent
  // of the scheme so we can just retry from here.
  if (!is_https)
    return ShouldDeleteCookieOnExit(cookie_settings, domain, true);
  // Check if there is a more precise rule that "domain matches" this cookie.
  bool matches_session_only_rule = false;
  for (const auto& entry : cookie_settings) {
    // While we don't know on which top-frame-origin a cookie was set, we still
    // use exceptions that only specify a secondary pattern to handle cookies
    // that match this pattern.
    const std::string& host = entry.primary_pattern.MatchesAllHosts()
                                  ? entry.secondary_pattern.GetHost()
                                  : entry.primary_pattern.GetHost();
    if (net::cookie_util::IsDomainMatch(domain, host)) {
      if (entry.GetContentSetting() == CONTENT_SETTING_ALLOW) {
        return false;
      } else if (entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY) {
        matches_session_only_rule = true;
      }
    }
  }
  return setting == CONTENT_SETTING_SESSION_ONLY || matches_session_only_rule;
}

ContentSetting CookieSettingsBase::GetCookieSetting(
    const GURL& url,
    const GURL& first_party_url,
    content_settings::SettingSource* source) const {
  return GetCookieSettingInternal(
      url, first_party_url,
      IsThirdPartyRequest(url, net::SiteForCookies::FromUrl(first_party_url)),
      source);
}

bool CookieSettingsBase::IsFullCookieAccessAllowed(
    const GURL& url,
    const GURL& first_party_url) const {
#if !BUILDFLAG(IS_IOS)
  // IOS uses this method with an empty |first_party_url| but we don't have
  // content settings on IOS, so it does not matter.
  DCHECK(!first_party_url.is_empty() || url.is_empty()) << url;
#endif
  return IsAllowed(GetCookieSetting(url, first_party_url, nullptr));
}

bool CookieSettingsBase::IsFullCookieAccessAllowed(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) const {
  ContentSetting setting = GetCookieSettingInternal(
      url,
      GetFirstPartyURL(site_for_cookies,
                       base::OptionalOrNullptr(top_frame_origin)),
      IsThirdPartyRequest(url, site_for_cookies), nullptr);
  return IsAllowed(setting);
}

bool CookieSettingsBase::IsCookieSessionOnly(const GURL& origin) const {
  ContentSetting setting = GetCookieSetting(origin, origin, nullptr);
  DCHECK(IsValidSetting(setting));
  return setting == CONTENT_SETTING_SESSION_ONLY;
}

net::CookieAccessSemantics
CookieSettingsBase::GetCookieAccessSemanticsForDomain(
    const std::string& cookie_domain) const {
  ContentSetting setting = GetSettingForLegacyCookieAccess(cookie_domain);
  DCHECK(IsValidSettingForLegacyAccess(setting));
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return net::CookieAccessSemantics::LEGACY;
    case CONTENT_SETTING_BLOCK:
      return net::CookieAccessSemantics::NONLEGACY;
    default:
      NOTREACHED();
  }
  return net::CookieAccessSemantics::UNKNOWN;
}

// static
bool CookieSettingsBase::IsValidSetting(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY ||
          setting == CONTENT_SETTING_BLOCK);
}

// static
bool CookieSettingsBase::IsAllowed(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY);
}

// static
bool CookieSettingsBase::IsValidSettingForLegacyAccess(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW || setting == CONTENT_SETTING_BLOCK);
}

}  // namespace content_settings
