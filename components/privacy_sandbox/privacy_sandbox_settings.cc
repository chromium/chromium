// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
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

// Convert a stored FLEDGE block eTLD+1 into applicable content settings
// patterns. This ensures that if Public Suffix List membership changes, the
// stored item continues to match as when it was set. Multiple patterns are set
// to support IP address fallbacks, which do not support [*.] prefixes.
// TODO (crbug.com/1287153): This is somewhat hacky and can be removed when
// FLEDGE is controlled by a content setting directly.
std::vector<ContentSettingsPattern> FledgeBlockToContentSettingsPatterns(
    const std::string& entry) {
  return {ContentSettingsPattern::FromString("[*.]" + entry),
          ContentSettingsPattern::FromString(entry)};
}

}  // namespace

PrivacySandboxSettings::PrivacySandboxSettings(
    HostContentSettingsMap* host_content_settings_map,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    PrefService* pref_service,
    bool incognito_profile)
    : host_content_settings_map_(host_content_settings_map),
      cookie_settings_(cookie_settings),
      pref_service_(pref_service),
      incognito_profile_(incognito_profile) {
  DCHECK(pref_service_);
  DCHECK(host_content_settings_map_);
  DCHECK(cookie_settings_);
  // "Clear on exit" causes a cookie deletion on shutdown. But for practical
  // purposes, we're notifying the observers on startup (which should be
  // equivalent, as no cookie operations could have happened while the profile
  // was shut down).
  if (IsCookiesClearOnExitEnabled(host_content_settings_map_))
    OnCookiesCleared();

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kPrivacySandboxApisEnabledV2,
      base::BindRepeating(&PrivacySandboxSettings::OnPrivacySandboxPrefChanged,
                          base::Unretained(this)));
}

PrivacySandboxSettings::~PrivacySandboxSettings() = default;

bool PrivacySandboxSettings::IsFlocAllowed() const {
  return pref_service_->GetBoolean(prefs::kPrivacySandboxFlocEnabled) &&
         IsPrivacySandboxEnabled();
}

bool PrivacySandboxSettings::IsFlocAllowedForContext(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin) const {
  // If FLoC is disabled completely, it is not available in any context.
  if (!IsFlocAllowed())
    return false;

  ContentSettingsForOneType cookie_settings;
  cookie_settings_->GetCookieSettings(&cookie_settings);

  return IsPrivacySandboxEnabledForContext(url, top_frame_origin,
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

  return IsPrivacySandboxEnabledForContext(reporting_origin.GetURL(),
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
  return IsPrivacySandboxEnabledForContext(
             reporting_origin.GetURL(), impression_origin, cookie_settings) &&
         IsPrivacySandboxEnabledForContext(reporting_origin.GetURL(),
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

  // Hosts are also accepted as a fallback. This may occur if the private
  // registry has changed, and what the caller may be assuming is an eTLD+1 no
  // longer is. Simply ignoring non-eTLD+1's may thus result in unexpected
  // access.
  if (effective_top_frame_etld_plus1 != top_frame_etld_plus1) {
    // Add a dummy scheme and use GURL to confirm the provided string is a valid
    // host.
    const GURL url("https://" + top_frame_etld_plus1);
    effective_top_frame_etld_plus1 = url.host();
  }

  // Ignore attempts to configure an empty etld+1. This will also catch the
  // case where the eTLD+1 was not even a host, as GURL will have canonicalised
  // it to empty.
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
    if (base::ranges::any_of(FledgeBlockToContentSettingsPatterns(entry.first),
                             [&](const auto& pattern) {
                               return pattern.Matches(
                                   top_frame_origin.GetURL());
                             })) {
      return false;
    }
  }
  return true;
}

bool PrivacySandboxSettings::IsFledgeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& auction_party) {
  // If the sandbox is disabled, then FLEDGE is never allowed.
  if (!IsPrivacySandboxEnabled())
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
  if (!IsPrivacySandboxEnabled())
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

bool PrivacySandboxSettings::IsPrivacySandboxEnabled() const {
  // Which preference is consulted is dependent on whether release 3 of the
  // settings is available.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    // For Privacy Sandbox Settings 3, APIs are disabled in incognito.
    if (incognito_profile_)
      return false;

    // The V2 pref was introduced with the 3rd Privacy Sandbox release.
    return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabledV2);
  }

  return pref_service_->GetBoolean(prefs::kPrivacySandboxApisEnabled);
}

void PrivacySandboxSettings::SetPrivacySandboxEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kPrivacySandboxManuallyControlled, true);

  // Only apply the decision to the appropriate preference. Confirmation logic
  // DCHECKS that the user has not been able to enable the V2 preference
  // without seeing a dialog.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabledV2, enabled);
  } else {
    pref_service_->SetBoolean(prefs::kPrivacySandboxApisEnabled, enabled);
  }
}

bool PrivacySandboxSettings::IsTrustTokensAllowed() {
  // The PrivacySandboxSettings is only involved in Trust Token access
  // decisions when the Release 3 flag is enabled.
  if (!base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3))
    return true;

  return IsPrivacySandboxEnabled();
}

void PrivacySandboxSettings::OnCookiesCleared() {
  SetFlocDataAccessibleFromNow(/*reset_calculate_timer=*/false);
}

void PrivacySandboxSettings::OnPrivacySandboxPrefChanged() {
  // The PrivacySandboxSettings is only involved in Trust Token access
  // decisions when the Release 3 flag is enabled.
  if (!base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3))
    return;

  for (auto& observer : observers_)
    observer.OnTrustTokenBlockingChanged(!IsTrustTokensAllowed());
}

void PrivacySandboxSettings::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrivacySandboxSettings::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrivacySandboxSettings::IsPrivacySandboxEnabledForContext(
    const GURL& url,
    const absl::optional<url::Origin>& top_frame_origin,
    const ContentSettingsForOneType& cookie_settings) const {
  if (!IsPrivacySandboxEnabled())
    return false;

  // TODO (crbug.com/1155504): Bypassing the CookieSettings class to access
  // content settings directly ignores allowlisted schemes and the storage
  // access API. These should be taken into account here.
  return !HasNonDefaultBlockSetting(
      cookie_settings, url,
      top_frame_origin ? top_frame_origin->GetURL() : GURL());
}
