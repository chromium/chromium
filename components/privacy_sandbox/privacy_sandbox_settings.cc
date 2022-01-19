// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
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

// Convert a stored FLEDGE block eTLD+1 into a content settings pattern. This
// ensures that if Public Suffix List membership changes, the stored item
// continues to match as when it was set.
// TODO (crbug.com/1287153): This is somewhat hacky and can be removed when
// FLEDGE is controlled by a content setting directly.
ContentSettingsPattern FledgeBlockToContentSettingsPattern(
    const std::string& entry) {
  return ContentSettingsPattern::FromString("[*.]" + entry);
}

}  // namespace

PrivacySandboxSettings::PrivacySandboxSettings(
    HostContentSettingsMap* host_content_settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
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

void PrivacySandboxSettings::SetFledgeJoiningAllowed(
    const std::string& top_frame_etld_plus1,
    bool allowed) {
  DictionaryPrefUpdate scoped_pref_update(
      pref_service_, prefs::kPrivacySandboxFledgeJoinBlocked);
  auto* pref_data = scoped_pref_update.Get();
  DCHECK(pref_data);
  DCHECK(pref_data->is_dict());

  // Ensure that the provided etld_plus1 actually is an etld+1.
  auto effective_top_frame_etld_plus1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          top_frame_etld_plus1,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  DCHECK(effective_top_frame_etld_plus1 == top_frame_etld_plus1);

  // Ignore attempts to configure an empty etld+1.
  if (effective_top_frame_etld_plus1.length() == 0) {
    NOTREACHED() << "Cannot control FLEDGE joining for empty eTLD+1";
    return;
  }

  if (allowed) {
    // Existence of the key implies blocking, so simply removing the key is
    // sufficient. If the key wasn't already present, the following is a no-op.
    pref_data->RemoveKey(effective_top_frame_etld_plus1);
  } else {
    // Overriding the creation date for keys which already exist is acceptable.
    // Time range based deletions are typically started from the current time,
    // and so this will be more aggressively removed. This decreases the chance
    // a potentially sensitive website remains in preferences.
    pref_data->SetKey(effective_top_frame_etld_plus1,
                      base::TimeToValue(base::Time::Now()));
  }
}

void PrivacySandboxSettings::ClearFledgeJoiningAllowedSettings(
    base::Time start_time,
    base::Time end_time) {
  DictionaryPrefUpdate scoped_pref_update(
      pref_service_, prefs::kPrivacySandboxFledgeJoinBlocked);
  auto* pref_data = scoped_pref_update.Get();
  DCHECK(pref_data);
  DCHECK(pref_data->is_dict());

  // Shortcut for maximum time range deletion
  if (start_time == base::Time() && end_time == base::Time::Max()) {
    pref_data->DictClear();
    return;
  }

  std::vector<std::string> keys_to_remove;
  for (auto entry : pref_data->DictItems()) {
    absl::optional<base::Time> created_time = base::ValueToTime(entry.second);
    if (created_time.has_value() && start_time <= created_time &&
        created_time <= end_time) {
      keys_to_remove.push_back(entry.first);
    }
  }

  for (const auto& key : keys_to_remove)
    pref_data->RemoveKey(key);
}

bool PrivacySandboxSettings::IsFledgeJoiningAllowed(
    const url::Origin& top_frame_origin) const {
  DictionaryPrefUpdate scoped_pref_update(
      pref_service_, prefs::kPrivacySandboxFledgeJoinBlocked);
  auto* pref_data = scoped_pref_update.Get();
  DCHECK(pref_data);
  DCHECK(pref_data->is_dict());
  for (auto entry : pref_data->DictItems()) {
    if (FledgeBlockToContentSettingsPattern(entry.first)
            .Matches(top_frame_origin.GetURL())) {
      return false;
    }
  }
  return true;
}

bool PrivacySandboxSettings::IsFledgeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& auction_party) {
  // If the sandbox is disabled, then FLEDGE is never allowed.
  if (!pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled))
    return false;

  // Third party cookies must also be available for this context. An empty site
  // for cookies is provided so the context is always treated as a third party.
  return cookie_settings_->IsFullCookieAccessAllowed(
      auction_party.GetURL(), net::SiteForCookies(), top_frame_origin);
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
