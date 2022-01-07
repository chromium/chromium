// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

bool IsCookiesClearOnExitEnabled(HostContentSettingsMap* map) {
  return map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                       /*provider_id=*/nullptr) ==
         ContentSetting::CONTENT_SETTING_SESSION_ONLY;
}

bool HasNonDefaultBlockSetting(const ContentSettingsForOneType& cookie_settings,
                               const GURL& url,
                               const GURL& top_frame_origin) {
  // APIs are allowed unless there is an effective non-default cookie content
  // setting block exception. A default cookie content setting is one that has a
  // wildcard pattern for both primary and secondary patterns. Content
  // settings are listed in descending order of priority such that the first
  // that matches is the effective content setting. A default setting can appear
  // anywhere in the list. Content settings which appear after a default content
  // setting are completely superseded by that content setting and are thus not
  // consulted. Default settings which appear before other settings are applied
  // from higher precedence sources, such as policy. The value of a default
  // content setting applied by a higher precedence provider is not consulted
  // here. For managed policies, the state will be reflected directly in the
  // privacy sandbox preference. Other providers (such as extensions) will have
  // been considered for the initial value of the privacy sandbox preference.
  for (const auto& setting : cookie_settings) {
    if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
        setting.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      return false;
    }
    if (setting.primary_pattern.Matches(url) &&
        setting.secondary_pattern.Matches(top_frame_origin)) {
      return setting.GetContentSetting() ==
             ContentSetting::CONTENT_SETTING_BLOCK;
    }
  }
  // ContentSettingsForOneType should always end with a default content setting
  // from the default provider.
  NOTREACHED();
  return false;
}

}  // namespace

PrivacySandboxSettings::PrivacySandboxSettings(
    HostContentSettingsMap* host_content_settings_map,
    content_settings::CookieSettings* cookie_settings,
    PrefService* pref_service)
    : host_content_settings_map_(host_content_settings_map),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(host_content_settings_map_);
  DCHECK(cookie_settings_);
  // "Clear on exit" causes a cookie deletion on shutdown. But for practical
  // purposes, we're notifying the observers on startup (which should be
  // equivalent, as no cookie operations could have happened while the profile
  // was shut down).
  if (IsCookiesClearOnExitEnabled(host_content_settings_map_))
    OnCookiesCleared();
}

PrivacySandboxSettings::~PrivacySandboxSettings() = default;

bool PrivacySandboxSettings::IsFlocAllowed() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxFlocEnabled) &&
         pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

bool PrivacySandboxSettings::IsFlocAllowedForContext(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin) const {
  // If FLoC is disabled completely, it is not available in any context.
  if (!IsFlocAllowed())
    return false;

  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(url, top_frame_origin,
                                           cookie_settings);
}

base::Time PrivacySandboxSettings::FlocDataAccessibleSince() const {
  return pref_service_->GetTime(prefs::kPrivacySandboxFlocDataAccessibleSince);
}

void PrivacySandboxSettings::SetFlocDataAccessibleFromNow(
    bool reset_calculate_timer) const {
  pref_service_->SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince,
                         base::Time::Now());

  for (auto& observer : observers_)
    observer.OnFlocDataAccessibleSinceUpdated(reset_calculate_timer);
}

bool PrivacySandboxSettings::IsConversionMeasurementAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) const {
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxAllowedForContext(reporting_origin.GetURL(),
                                           top_frame_origin, cookie_settings);
}

bool PrivacySandboxSettings::ShouldSendConversionReport(
    const url::Origin& impression_origin,
    const url::Origin& conversion_origin,
    const url::Origin& reporting_origin) const {
  // Re-using the |cookie_settings| allows this function to be faster
  // than simply calling IsConversionMeasurementAllowed() twice
  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  // The |reporting_origin| needs to have been accessible in both impression
  // and conversion contexts. These are both checked when they occur, but
  // user settings may have changed between then and when the conversion report
  // is sent.
  return IsPrivacySandboxAllowedForContext(
             reporting_origin.GetURL(), impression_origin, cookie_settings) &&
         IsPrivacySandboxAllowedForContext(reporting_origin.GetURL(),
                                           conversion_origin, cookie_settings);
}

bool PrivacySandboxSettings::IsFledgeAllowed(
    const url::Origin& top_frame_origin,
    const GURL& auction_party) {
  // If the sandbox is disabled, then FLEDGE is never allowed.
  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return false;

  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  return cookie_settings_->IsFullCookieAccessAllowed(
      auction_party, net::SiteForCookies(), top_frame_origin);
}

std::vector<GURL> PrivacySandboxSettings::FilterFledgeAllowedParties(
    const url::Origin& top_frame_origin,
    const std::vector<GURL>& auction_parties) {
  // If the sandbox is disabled, then no parties are allowed.
  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return {};

  std::vector<GURL> allowed_parties;
  for (const auto& party : auction_parties) {
    if (cookie_settings_->IsFullCookieAccessAllowed(
            party, net::SiteForCookies(), top_frame_origin)) {
      allowed_parties.push_back(party);
    }
  }
  return allowed_parties;
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowed() {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

void PrivacySandboxSettings::SetPrivacySandboxEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxManuallyControlled, true);
  pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabled, enabled);
}

void PrivacySandboxSettings::OnCookiesCleared() {
  SetFlocDataAccessibleFromNow(/*reset_calculate_timer=*/false);
}

void PrivacySandboxSettings::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacySandboxSettings::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrivacySandboxSettings::IsPrivacySandboxAllowedForContext(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin,
    const ContentSettingsForOneType& cookie_settings) const {
  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return false;

  // TODO (crbug.com/1155504): Bypassing the CookieSettings class to access
  // content settings directly ignores allowlisted schemes and the storage
  // access API. These should be taken into account here.
  return !HasNonDefaultBlockSetting(
      cookie_settings, url,
      top_frame_origin ? top_frame_origin->GetURL() : GURL());
}
