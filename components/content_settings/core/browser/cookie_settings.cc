// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/features.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)
#include "components/content_settings/core/common/features.h"
#else
#include "third_party/blink/public/common/features_generated.h"
#endif

namespace content_settings {

CookieSettings::CookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs,
    bool is_incognito,
    const char* extension_scheme)
    : host_content_settings_map_(host_content_settings_map),
      is_incognito_(is_incognito),
      extension_scheme_(extension_scheme),
      block_third_party_cookies_(false) {
  content_settings_observation_.Observe(host_content_settings_map_.get());
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&CookieSettings::OnCookiePreferencesChanged,
                          base::Unretained(this)));
  OnCookiePreferencesChanged();
}

ContentSetting CookieSettings::GetDefaultCookieSetting(
    std::string* provider_id) const {
  return host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, provider_id);
}

ContentSettingsForOneType CookieSettings::GetCookieSettings() const {
  ContentSettingsForOneType settings;
  host_content_settings_map_->GetSettingsForOneType(
      ContentSettingsType::COOKIES, &settings);
  return settings;
}

void CookieSettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kCookieControlsMode,
      static_cast<int>(CookieControlsMode::kIncognitoOnly),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void CookieSettings::SetDefaultCookieSetting(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, setting);
}

void CookieSettings::SetCookieSetting(const GURL& primary_url,
                                      ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::COOKIES, setting);
}

void CookieSettings::ResetCookieSetting(const GURL& primary_url) {
  host_content_settings_map_->SetNarrowestContentSetting(
      primary_url, GURL(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_DEFAULT);
}

// TODO(crbug.com/1386190): Update to take in CookieSettingOverrides.
bool CookieSettings::IsThirdPartyAccessAllowed(
    const GURL& first_party_url,
    content_settings::SettingSource* source) {
  // Use GURL() as an opaque primary url to check if any site
  // could access cookies in a 3p context on |first_party_url|.
  return IsAllowed(GetCookieSetting(GURL(), first_party_url,
                                    net::CookieSettingOverrides(), source));
}

void CookieSettings::SetThirdPartyCookieSetting(const GURL& first_party_url,
                                                ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::COOKIES, setting);
}

void CookieSettings::ResetThirdPartyCookieSetting(const GURL& first_party_url) {
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsStorageDurable(const GURL& origin) const {
  // TODO(dgrogan): Don't use host_content_settings_map_ directly.
  // https://crbug.com/539538
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      origin /*primary*/, origin /*secondary*/,
      ContentSettingsType::DURABLE_STORAGE);
  return setting == CONTENT_SETTING_ALLOW;
}

ContentSetting CookieSettings::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain) const {
  // The content setting patterns are treated as domains, not URLs, so the
  // scheme is irrelevant (so we can just arbitrarily pass false).
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      cookie_domain, false /* secure scheme */);

  return host_content_settings_map_->GetContentSetting(
      cookie_domain_url, GURL(), ContentSettingsType::LEGACY_COOKIE_ACCESS);
}

bool CookieSettings::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  return site_for_cookies.RepresentativeUrl().SchemeIs(kChromeUIScheme) &&
         url.SchemeIsCryptographic();
}

void CookieSettings::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_change_registrar_.RemoveAll();
}

bool CookieSettings::ShouldAlwaysAllowCookies(
    const GURL& url,
    const GURL& first_party_url) const {
  if (first_party_url.SchemeIs(kChromeUIScheme) &&
      url.SchemeIsCryptographic()) {
    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extension_scheme_) &&
      first_party_url.SchemeIs(extension_scheme_)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return false;
}

ContentSetting CookieSettings::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    net::CookieSettingOverrides overrides,
    content_settings::SettingSource* source) const {
  // Auto-allow in extensions or for WebUI embedding a secure origin.
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    return CONTENT_SETTING_ALLOW;
  }

  // First get any host-specific settings.
  SettingInfo info;
  const base::Value value = host_content_settings_map_->GetWebsiteSetting(
      url, first_party_url, ContentSettingsType::COOKIES, &info);
  if (source)
    *source = info.source;

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply CONTENT_SETTING_BLOCKED.
  bool block_third = info.primary_pattern.MatchesAllHosts() &&
                     info.secondary_pattern.MatchesAllHosts() &&
                     ShouldBlockThirdPartyCookies() &&
                     !first_party_url.SchemeIs(extension_scheme_);

  // We should always have a value, at least from the default provider.
  DCHECK(value.is_int());
  ContentSetting setting = ValueToContentSetting(value);
  bool block = block_third && is_third_party_request;

  if (!block) {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_ALLOWED);
  }

#if !BUILDFLAG(IS_IOS)
  // IOS doesn't use blink and as such cannot check our feature flag. Disabling
  // by default there should be no-op as the lack of Blink also means no grants
  // would be generated. Everywhere else we'll use |kStorageAccessAPI| to gate
  // our checking logic.
  // We'll perform this check after we know if we will |block| or not to avoid
  // performing extra work in scenarios we already allow.

  // TODO(https://crbug.com/1411765): instead of using a BUILDFLAG and checking
  // the feature here, we should rely on CookieSettingsFactory to plumb in this
  // boolean instead.
  bool storage_access_api_enabled =
      base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI);
  if (block && storage_access_api_enabled &&
      ShouldConsiderStorageAccessGrants(overrides)) {
    ContentSetting host_setting = host_content_settings_map_->GetContentSetting(
        url, first_party_url, ContentSettingsType::STORAGE_ACCESS);

    if (host_setting == CONTENT_SETTING_ALLOW) {
      block = false;
      FireStorageAccessHistogram(net::cookie_util::StorageAccessResult::
                                     ACCESS_ALLOWED_STORAGE_ACCESS_GRANT);
    }
  }

  if (block && storage_access_api_enabled &&
      ShouldConsiderTopLevelStorageAccessGrants(overrides)) {
    ContentSetting host_setting = host_content_settings_map_->GetContentSetting(
        url, first_party_url, ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS);

    if (host_setting == CONTENT_SETTING_ALLOW) {
      block = false;
      FireStorageAccessHistogram(
          net::cookie_util::StorageAccessResult::
              ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT);
    }
  }
#endif

  if (block && is_third_party_request &&
      overrides.Has(net::CookieSettingOverride::kForceThirdPartyByUser)) {
    block = false;
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED);
  }

  if (block) {
    FireStorageAccessHistogram(
        net::cookie_util::StorageAccessResult::ACCESS_BLOCKED);
  }

  return block ? CONTENT_SETTING_BLOCK : setting;
}

CookieSettings::~CookieSettings() = default;

bool CookieSettings::ShouldBlockThirdPartyCookiesInternal() {
  DCHECK(thread_checker_.CalledOnValidThread());

#if BUILDFLAG(IS_IOS)
  if (!base::FeatureList::IsEnabled(kImprovedCookieControls))
    return false;
#endif

  CookieControlsMode mode = static_cast<CookieControlsMode>(
      pref_change_registrar_.prefs()->GetInteger(prefs::kCookieControlsMode));

  switch (mode) {
    case CookieControlsMode::kBlockThirdParty:
      return true;
    case CookieControlsMode::kIncognitoOnly:
      return is_incognito_;
    case CookieControlsMode::kOff:
      return false;
  }
  return false;
}

void CookieSettings::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::COOKIES)) {
    for (auto& observer : observers_)
      observer.OnCookieSettingChanged();
  }
}

void CookieSettings::OnCookiePreferencesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool new_block_third_party_cookies = ShouldBlockThirdPartyCookiesInternal();

  {
    base::AutoLock auto_lock(lock_);
    if (block_third_party_cookies_ == new_block_third_party_cookies)
      return;
    block_third_party_cookies_ = new_block_third_party_cookies;
  }
  for (Observer& obs : observers_)
    obs.OnThirdPartyCookieBlockingChanged(new_block_third_party_cookies);
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}

}  // namespace content_settings
