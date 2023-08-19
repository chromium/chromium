// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_BLINK)
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#endif

namespace content_settings {

bool CookieSettingsBase::storage_access_api_grants_unpartitioned_storage_ =
    false;

void CookieSettingsBase::
    SetStorageAccessAPIGrantsUnpartitionedStorageForTesting(bool grants) {
  storage_access_api_grants_unpartitioned_storage_ = grants;
}

CookieSettingsBase::CookieSettingsBase()
    : is_storage_partitioned_(base::FeatureList::IsEnabled(
          net::features::kThirdPartyStoragePartitioning)),
      is_privacy_sandbox_v4_enabled_(
#if !BUILDFLAG(USE_BLINK)
          false
#else
          base::FeatureList::IsEnabled(
              privacy_sandbox::kPrivacySandboxSettings4)
#endif
      ) {
}

CookieSettingsBase::CookieSettingWithMetadata::CookieSettingWithMetadata(
    ContentSetting cookie_setting,
    absl::optional<ThirdPartyBlockingScope> third_party_blocking_scope,
    bool is_explicit_setting)
    : cookie_setting_(cookie_setting),
      third_party_blocking_scope_(third_party_blocking_scope),
      is_explicit_setting_(is_explicit_setting) {
  DCHECK(!third_party_blocking_scope_.has_value() ||
         !IsAllowed(cookie_setting_));
}

bool CookieSettingsBase::CookieSettingWithMetadata::
    BlockedByThirdPartyCookieBlocking() const {
  return !IsAllowed(cookie_setting_) && third_party_blocking_scope_.has_value();
}

bool CookieSettingsBase::CookieSettingWithMetadata::IsPartitionedStateAllowed()
    const {
  return IsAllowed(cookie_setting_) ||
         third_party_blocking_scope_ ==
             ThirdPartyBlockingScope::kUnpartitionedOnly;
}

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
  // Pass GURL() as first_party_url since we don't know the context and
  // don't want to match against (*, exception) pattern.
  // No overrides are given since existing ones only pertain to 3P checks.
  ContentSetting setting =
      GetCookieSettingInternal(origin,
                               is_privacy_sandbox_v4_enabled_ ? GURL() : origin,
                               /*is_third_party_request=*/false,
                               net::CookieSettingOverrides(), nullptr)
          .cookie_setting();
  DCHECK(IsValidSetting(setting));
  if (setting == CONTENT_SETTING_ALLOW) {
    return false;
  }
  // Non-secure cookies are readable by secure sites. We need to check for
  // https pattern if http is not allowed. The section below is independent
  // of the scheme so we can just retry from here.
  if (!is_https) {
    return ShouldDeleteCookieOnExit(cookie_settings, domain, true);
  }
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
    net::CookieSettingOverrides overrides,
    content_settings::SettingInfo* info) const {
  return GetCookieSettingInternal(
             url, first_party_url,
             IsThirdPartyRequest(url,
                                 net::SiteForCookies::FromUrl(first_party_url)),
             overrides, info)
      .cookie_setting();
}

bool CookieSettingsBase::IsFullCookieAccessAllowed(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  ContentSetting setting =
      GetCookieSettingInternal(
          url,
          GetFirstPartyURL(site_for_cookies,
                           base::OptionalToPtr(top_frame_origin)),
          IsThirdPartyRequest(url, site_for_cookies), overrides, nullptr)
          .cookie_setting();
  return IsAllowed(setting);
}

bool CookieSettingsBase::IsCookieSessionOnly(const GURL& origin) const {
  // Pass GURL() as first_party_url since we don't know the context and
  // don't want to match against (*, exception) pattern.
  // No overrides are given since existing ones only pertain to 3P checks.
  ContentSetting setting =
      GetCookieSettingInternal(origin,
                               is_privacy_sandbox_v4_enabled_ ? GURL() : origin,
                               /*is_third_party_request=*/false,
                               net::CookieSettingOverrides(), nullptr)
          .cookie_setting();
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

bool CookieSettingsBase::ShouldConsider3pcdSupportSettings(
    net::CookieSettingOverrides overrides) const {
  return base::FeatureList::IsEnabled(net::features::kTpcdSupportSettings) &&
         overrides.Has(net::CookieSettingOverride::k3pcdSupport);
}

bool CookieSettingsBase::ShouldConsiderStorageAccessGrants(
    net::CookieSettingOverrides overrides) const {
  return overrides.Has(net::CookieSettingOverride::kStorageAccessGrantEligible);
}

net::CookieSettingOverrides CookieSettingsBase::SettingOverridesForStorage()
    const {
  net::CookieSettingOverrides overrides;
  if (storage_access_api_grants_unpartitioned_storage_ ||
      is_storage_partitioned_) {
    overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
  }
  if (is_storage_partitioned_) {
    overrides.Put(
        net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
    // TODO(crbug.com/1466156): Revisit whether the global setting/pref should
    // be checked here.
    overrides.Put(net::CookieSettingOverride::k3pcdSupport);
  }
  return overrides;
}

bool CookieSettingsBase::ShouldConsiderTopLevelStorageAccessGrants(
    net::CookieSettingOverrides overrides) const {
  return overrides.Has(
      net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
}

CookieSettingsBase::CookieSettingWithMetadata
CookieSettingsBase::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides,
    SettingInfo* info) const {
  // Auto-allow in extensions or for WebUI embedding a secure origin.
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    return {/*cookie_setting=*/CONTENT_SETTING_ALLOW,
            /*third_party_blocking_scope=*/absl::nullopt,
            /*is_explicit_setting=*/false};
  }

  // First get any host-specific settings.
  SettingInfo setting_info;
  ContentSetting setting = GetContentSetting(
      url, first_party_url, ContentSettingsType::COOKIES, &setting_info);
  if (info) {
    *info = setting_info;
  }

  bool is_explicit_setting = !setting_info.primary_pattern.MatchesAllHosts() ||
                             !setting_info.secondary_pattern.MatchesAllHosts();

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply CONTENT_SETTING_BLOCKED.
  bool block_third =
      IsAllowed(setting) && !is_explicit_setting && is_third_party_request &&
      ShouldBlockThirdPartyCookies() &&
      !IsThirdPartyCookiesAllowedScheme(first_party_url.scheme());

  if (IsAllowed(setting) && !block_third) {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_ALLOWED);
  }

  if (block_third) {
    bool has_storage_access_opt_in =
        ShouldConsiderStorageAccessGrants(overrides);
    bool has_storage_access_permission_grant =
        IsAllowedByStorageAccessGrant(url, first_party_url);

    net::cookie_util::FireStorageAccessInputHistogram(
        has_storage_access_opt_in, has_storage_access_permission_grant);

    if (IsStorageAccessApiEnabled() && has_storage_access_opt_in &&
        has_storage_access_permission_grant) {
      block_third = false;
      FireStorageAccessHistogram(net::cookie_util::StorageAccessResult::
                                     ACCESS_ALLOWED_STORAGE_ACCESS_GRANT);
    }

    if (IsStorageAccessApiEnabled() &&
        ShouldConsiderTopLevelStorageAccessGrants(overrides) &&
        GetContentSetting(url, first_party_url,
                          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS) ==
            CONTENT_SETTING_ALLOW) {
      block_third = false;
      FireStorageAccessHistogram(
          net::cookie_util::StorageAccessResult::
              ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT);
    }
  }

  if (block_third && ShouldConsider3pcdSupportSettings(overrides) &&
      GetContentSetting(url, first_party_url,
                        ContentSettingsType::TPCD_SUPPORT) ==
          CONTENT_SETTING_ALLOW) {
    // TODO (crbug.com/1466156): Revisit this after a decision has been made on
    // how an explicit 3PC setting will be differentiated from an experimental
    // one.
    block_third = false;
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_3PCD);
  }

  if (!IsAllowed(setting) || block_third) {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED);
  }

  absl::optional<ThirdPartyBlockingScope> scope;
  if (block_third) {
    scope = IsAllowed(setting)
                ? ThirdPartyBlockingScope::kUnpartitionedOnly
                : ThirdPartyBlockingScope::kUnpartitionedAndPartitioned;
  }
  return {block_third ? CONTENT_SETTING_BLOCK : setting, scope,
          is_explicit_setting};
}

bool CookieSettingsBase::IsAllowedByStorageAccessGrant(
    const GURL& url,
    const GURL& first_party_url) const {
  // The Storage Access API allows access in A(B(A)) case (or similar). Do the
  // same-origin check first for performance reasons.
  const url::Origin origin = url::Origin::Create(url);
  const url::Origin first_party_origin = url::Origin::Create(first_party_url);
  if (origin.IsSameOriginWith(first_party_origin) ||
      net::SchemefulSite(origin) == net::SchemefulSite(first_party_origin)) {
    return true;
  }

  return GetContentSetting(url, first_party_url,
                           ContentSettingsType::STORAGE_ACCESS) ==
         CONTENT_SETTING_ALLOW;
}

ContentSetting CookieSettingsBase::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain) const {
  // The content setting patterns are treated as domains, not URLs, so the
  // scheme is irrelevant (so we can just arbitrarily pass false).
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      cookie_domain, false /* secure scheme */);

  return GetContentSetting(cookie_domain_url, GURL(),
                           ContentSettingsType::LEGACY_COOKIE_ACCESS);
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
