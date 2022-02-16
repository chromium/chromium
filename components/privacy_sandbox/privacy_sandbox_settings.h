// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class HostContentSettingsMap;
class PrefService;

namespace content_settings {
class CookieSettings;
}

namespace url {
class Origin;
}

// A service which acts as a intermediary between Privacy Sandbox APIs and the
// preferences and content settings which define when they are allowed to be
// accessed. Privacy Sandbox APIs, regardless of where they live (renderer,
// browser, network etc), must consult this service to determine when
// they are allowed to run. While a basic on/off control is provided by this
// service, embedders are expected to achieve fine-grained control though
// the underlying preferences and content settings separately.
class PrivacySandboxSettings : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnFlocDataAccessibleSinceUpdated(bool reset_compute_timer) {}

    virtual void OnTrustTokenBlockingChanged(bool blocked) {}
  };

  PrivacySandboxSettings(
      HostContentSettingsMap* host_content_settings_map,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      PrefService* pref_service,
      bool incognito_profile);
  ~PrivacySandboxSettings() override;

  // Returns whether FLoC is allowed at all. If false, FLoC calculations should
  // not occur. If true, the more specific function, IsFlocAllowedForContext(),
  // should be consulted for the relevant context.
  bool IsFlocAllowed() const;

  // Determines whether FLoC is allowable in a particular context.
  // |top_frame_origin| is used to check for content settings which could both
  // affect 1P and 3P contexts.
  bool IsFlocAllowedForContext(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin) const;

  // Returns the point in time from which history is eligible to be used when
  // calculating a user's FLoC ID. Reset when a user clears all cookies, or
  // when the browser restarts with "Clear on exit" enabled. The returned time
  // will have been fuzzed for local privacy, and so may be in the future, in
  // which case no history is eligible.
  base::Time FlocDataAccessibleSince() const;

  // Sets the time when history is accessible for FLoC calculation to the
  // current time, optionally resetting the time to the next FLoC id calculation
  // if |reset_calculate_timer| is true.
  void SetFlocDataAccessibleFromNow(bool reset_calculate_timer) const;

  // Determines whether Conversion Measurement is allowable in a particular
  // context. Should be called at both impression & conversion. At each of these
  // points |top_frame_origin| is the same as either the impression origin or
  // the conversion origin respectively.
  bool IsConversionMeasurementAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const;

  // Called before sending the associated conversion report to
  // |reporting_origin|. Re-checks that |reporting_origin| is allowable as a 3P
  // on both |impression_origin| and |conversion_origin|.
  bool ShouldSendConversionReport(const url::Origin& impression_origin,
                                  const url::Origin& conversion_origin,
                                  const url::Origin& reporting_origin) const;

  // Sets the ability for |top_frame_etld_plus1| to join the profile to interest
  // groups to |allowed|. This information is stored in preferences, and is made
  // available to the API via IsFledgeJoiningAllowed(). |top_frame_etld_plus1|
  // should in most circumstances be a valid eTLD+1, but hosts are accepted to
  // allow for shifts in private registries. Entries are converted into wildcard
  // subdomain ContentSettingsPattern before comparison.
  void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                               bool allowed);

  // Clears any FLEDGE joining block settings with creation times between
  // |start_time| and |end_time|.
  void ClearFledgeJoiningAllowedSettings(base::Time start_time,
                                         base::Time end_time);

  // Determines whether the user may be joined to FLEDGE interest groups on, or
  // by, |top_frame_origin|. This is an additional check that must be
  // combined with the more generic IsFledgeAllowed().
  bool IsFledgeJoiningAllowed(const url::Origin& top_frame_origin) const;

  // Determine whether |auction_party| can register an interest group, or sell /
  // buy in an auction, on |top_frame_origin|.
  bool IsFledgeAllowed(const url::Origin& top_frame_origin,
                       const url::Origin& auction_party);

  // Filter |auction_parties| down to those that may participate as a buyer for
  // auctions run on |top_frame_origin|. Logically equivalent to calling
  // IsFledgeAllowed() for each element of |auction_parties|.
  std::vector<GURL> FilterFledgeAllowedParties(
      const url::Origin& top_frame_origin,
      const std::vector<GURL>& auction_parties);

  // Returns whether the Privacy Sandbox is "generally" available. A return
  // value of false indicates that the sandbox is completely disabled. A return
  // value of true *must* be followed up by the appropriate context specific
  // check.
  bool IsPrivacySandboxEnabled() const;

  // Disables the Privacy Sandbox completely if |enabled| is false, if |enabled|
  // is true, more granular checks will still be performed to determine if
  // specific APIs are available in specific contexts.
  void SetPrivacySandboxEnabled(bool enabled);

  // Returns whether Trust Tokens are "generally" available. A return value of
  // false is authoritative, while a value of true must be followed by the
  // appropriate context specific check.
  bool IsTrustTokensAllowed();

  // Called when there's a broad cookies clearing action. For example, this
  // should be called on "Clear browsing data", but shouldn't be called on the
  // Clear-Site-Data header, as it's restricted to a specific site.
  void OnCookiesCleared();

  // Called when the main privacy sandbox preference is changed.
  void OnPrivacySandboxPrefChanged();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Determines based on the current features, preferences and provided
  // |cookie_settings| whether Privacy Sandbox APIs are generally allowable for
  // |url| on |top_frame_origin|. Individual APIs may perform additional checks
  // for allowability (such as incognito) ontop of this. |cookie_settings| is
  // provided as a parameter to allow callers to cache it between calls.
  bool IsPrivacySandboxEnabledForContext(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin,
      const ContentSettingsForOneType& cookie_settings) const;

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  bool incognito_profile_;
};

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
