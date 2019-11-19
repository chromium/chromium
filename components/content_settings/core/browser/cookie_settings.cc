// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cookies/cookie_util.h"
#include "url/gurl.h"

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
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&CookieSettings::OnCookiePreferencesChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::Bind(&CookieSettings::OnCookiePreferencesChanged,
                 base::Unretained(this)));
  OnCookiePreferencesChanged();
}

ContentSetting CookieSettings::GetDefaultCookieSetting(
    std::string* provider_id) const {
  return host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, provider_id);
}

void CookieSettings::GetCookieSettings(
    ContentSettingsForOneType* settings) const {
  host_content_settings_map_->GetSettingsForOneType(
      ContentSettingsType::COOKIES, std::string(), settings);
}

void CookieSettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kBlockThirdPartyCookies, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kCookieControlsMode,
      static_cast<int>(kImprovedCookieControlsDefaultInIncognito.Get()
                           ? CookieControlsMode::kIncognitoOnly
                           : CookieControlsMode::kOff),
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
      primary_url, GURL(), ContentSettingsType::COOKIES, std::string(),
      setting);
}

void CookieSettings::ResetCookieSetting(const GURL& primary_url) {
  host_content_settings_map_->SetNarrowestContentSetting(
      primary_url, GURL(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsThirdPartyAccessAllowed(const GURL& first_party_url) {
  // Use GURL() as an opaque primary url to check if any site
  // could access cookies in a 3p context on |first_party_url|.
  return IsCookieAccessAllowed(GURL(), first_party_url);
}

void CookieSettings::SetThirdPartyCookieSetting(const GURL& first_party_url,
                                                ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::COOKIES, std::string(), setting);
}

void CookieSettings::ResetThirdPartyCookieSetting(const GURL& first_party_url) {
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsStorageDurable(const GURL& origin) const {
  // TODO(dgrogan): Don't use host_content_settings_map_ directly.
  // https://crbug.com/539538
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      origin /*primary*/, origin /*secondary*/,
      ContentSettingsType::DURABLE_STORAGE,
      std::string() /*resource_identifier*/);
  return setting == CONTENT_SETTING_ALLOW;
}

void CookieSettings::GetSettingForLegacyCookieAccess(
    const std::string& cookie_domain,
    ContentSetting* setting) const {
  DCHECK(setting);

  // The content setting patterns are treated as domains, not URLs, so the
  // scheme is irrelevant (so we can just arbitrarily pass false).
  GURL cookie_domain_url = net::cookie_util::CookieOriginToURL(
      cookie_domain, false /* secure scheme */);

  *setting = host_content_settings_map_->GetContentSetting(
      cookie_domain_url, GURL(), ContentSettingsType::LEGACY_COOKIE_ACCESS,
      std::string() /* resource_identifier */);
}

bool CookieSettings::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const GURL& site_for_cookies) const {
  return site_for_cookies.SchemeIs(kChromeUIScheme) &&
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

void CookieSettings::GetCookieSettingInternal(
    const GURL& url,
    const GURL& first_party_url,
    bool is_third_party_request,
    content_settings::SettingSource* source,
    ContentSetting* cookie_setting) const {
  DCHECK(cookie_setting);
  // Auto-allow in extensions or for WebUI embedding a secure origin.
  if (ShouldAlwaysAllowCookies(url, first_party_url)) {
    *cookie_setting = CONTENT_SETTING_ALLOW;
    return;
  }

  // First get any host-specific settings.
  SettingInfo info;
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          url, first_party_url, ContentSettingsType::COOKIES, std::string(),
          &info);
  if (source)
    *source = info.source;

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply CONTENT_SETTING_BLOCKED.
  bool block_third = info.primary_pattern.MatchesAllHosts() &&
                     info.secondary_pattern.MatchesAllHosts() &&
                     ShouldBlockThirdPartyCookies() &&
                     !first_party_url.SchemeIs(extension_scheme_);

  // We should always have a value, at least from the default provider.
  DCHECK(value);
  ContentSetting setting = ValueToContentSetting(value.get());
  bool block = block_third && is_third_party_request;
  *cookie_setting = block ? CONTENT_SETTING_BLOCK : setting;
}

CookieSettings::~CookieSettings() {
}

bool CookieSettings::IsCookieControlsEnabled() {
  if (base::FeatureList::IsEnabled(
          kImprovedCookieControlsForThirdPartyCookieBlocking) &&
      pref_change_registrar_.prefs()->GetBoolean(
          prefs::kBlockThirdPartyCookies)) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(kImprovedCookieControls))
    return false;

  CookieControlsMode mode = static_cast<CookieControlsMode>(
      pref_change_registrar_.prefs()->GetInteger(prefs::kCookieControlsMode));

  switch (mode) {
    case CookieControlsMode::kOn:
      return true;
    case CookieControlsMode::kIncognitoOnly:
      return is_incognito_;
    case CookieControlsMode::kOff:
      return false;
  }
  return false;
}

void CookieSettings::OnCookiePreferencesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool new_block_third_party_cookies =
      pref_change_registrar_.prefs()->GetBoolean(
          prefs::kBlockThirdPartyCookies) ||
      IsCookieControlsEnabled();

  // Safe to read |block_third_party_cookies_| without locking here because the
  // only place that writes to it is this method and it will always be run on
  // the same thread.
  if (block_third_party_cookies_ != new_block_third_party_cookies) {
    {
      base::AutoLock auto_lock(lock_);
      block_third_party_cookies_ = new_block_third_party_cookies;
    }
    for (Observer& obs : observers_)
      obs.OnThirdPartyCookieBlockingChanged(new_block_third_party_cookies);
  }
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}

}  // namespace content_settings
