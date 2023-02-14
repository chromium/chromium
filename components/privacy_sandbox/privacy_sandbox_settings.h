// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/canonical_topic.h"

class GURL;

namespace url {
class Origin;
}

namespace privacy_sandbox {

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
    virtual void OnTopicsDataAccessibleSinceUpdated() {}

    // Fired when Trust Token blocking has changed because of a change to the
    // Privacy Sandbox preference. Does not account for changes to third-party
    // cookie blocking, which may result in the Privacy Sandbox being disabled.
    // Trust tokens thus additionally independently consult Cookie settings.
    // TODO(crbug.com/1304132): Unify this so Trust Tokens only need to consult
    // a single source of truth.
    virtual void OnTrustTokenBlockingChanged(bool blocked) {}

    // Fired when the First-Party Sets changes to being `enabled` as a result of
    // the kPrivacySandboxFirstPartySets preference changing.
    virtual void OnFirstPartySetsEnabledChanged(bool enabled) {}
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Allows the delegate to restrict access to the Privacy Sandbox. When
    // the Privacy Sandbox is restricted, all API access is disabled. This is
    // consulted on every access check, and it is acceptable for this to change
    // return value over the life of the service.
    virtual bool IsPrivacySandboxRestricted() const = 0;

    // Whether the current profile is Incognito or not. For Incognito, the
    // privacy sandbox APIs are restricted.
    virtual bool IsIncognitoProfile() const = 0;

    // Whether there is an appropriate level of consent for the Topics API.
    // When this returns false, access control functions for Topics will
    // return as not allowed.
    virtual bool HasAppropriateTopicsConsent() const = 0;
  };

  // Returns whether the Topics API is allowed at all. If false, Topics API
  // calculations should not occur. If true, the more specific function,
  // IsTopicsApiAllowedForContext(), should be consulted for the relevant
  // context.
  virtual bool IsTopicsAllowed() const = 0;

  // Determines whether the Topics API is allowable in a particular context.
  // |top_frame_origin| is used to check for content settings which could both
  // affect 1P and 3P contexts.
  virtual bool IsTopicsAllowedForContext(const url::Origin& top_frame_origin,
                                         const GURL& url) const = 0;

  // Returns whether |topic| can be either considered as a top topic for the
  // current epoch, or provided to a website as a previous / current epochs
  // site assigned topic.
  virtual bool IsTopicAllowed(const CanonicalTopic& topic) = 0;

  // Sets |topic| to |allowed|. Whether a topic is allowed or not is made
  // available through IsTopicAllowed().
  virtual void SetTopicAllowed(const CanonicalTopic& topic, bool allowed) = 0;

  // Removes all Topic settings with creation times between |start_time|
  // and |end_time|. This allows for integration with the existing browsing data
  // remover, such as the one powering Clear Browser Data.
  virtual void ClearTopicSettings(base::Time start_time,
                                  base::Time end_time) = 0;

  // Returns the point in time from which history is eligible to be used when
  // calculating a user's Topics API topics. Reset when a user clears all
  // cookies, or when the browser restarts with "Clear on exit" enabled. The
  // returned time will have been fuzzed for local privacy, and so may be in the
  // future, in which case no history is eligible.
  virtual base::Time TopicsDataAccessibleSince() const = 0;

  // Returns whether any Attribution Rerpoting operation would ever be allowed.
  // If false, no attribution reporting operation is allowed (e.g. because the
  // user has disabled the setting). If true, the appropriate context specific
  // check must also be made.
  virtual bool IsAttributionReportingEverAllowed() const = 0;

  // Determines whether Attribution Reporting is allowable in a particular
  // context. Should be called at both source and trigger registration. At each
  // of these points |top_frame_origin| is the same as either the source origin
  // or the destination origin respectively.
  virtual bool IsAttributionReportingAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const = 0;

  // Called before sending the associated attribution report to
  // |reporting_origin|. Re-checks that |reporting_origin| is allowable as a 3P
  // on both |source_origin| and |destination_origin|.
  virtual bool MaySendAttributionReport(
      const url::Origin& source_origin,
      const url::Origin& destination_origin,
      const url::Origin& reporting_origin) const = 0;

  // Sets the ability for |top_frame_etld_plus1| to join the profile to interest
  // groups to |allowed|. This information is stored in preferences, and is made
  // available to the API via IsFledgeJoiningAllowed(). |top_frame_etld_plus1|
  // should in most circumstances be a valid eTLD+1, but hosts are accepted to
  // allow for shifts in private registries. Entries are converted into wildcard
  // subdomain ContentSettingsPattern before comparison.
  virtual void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                                       bool allowed) = 0;

  // Clears any FLEDGE joining block settings with creation times between
  // |start_time| and |end_time|.
  virtual void ClearFledgeJoiningAllowedSettings(base::Time start_time,
                                                 base::Time end_time) = 0;

  // Determines whether the user may be joined to FLEDGE interest groups on, or
  // by, |top_frame_origin|. This is an additional check that must be
  // combined with the more generic IsFledgeAllowed().
  virtual bool IsFledgeJoiningAllowed(
      const url::Origin& top_frame_origin) const = 0;

  // Determine whether |auction_party| can register an interest group, or sell
  // buy in an auction, on |top_frame_origin|.
  virtual bool IsFledgeAllowed(const url::Origin& top_frame_origin,
                               const url::Origin& auction_party) const = 0;

  // Determines whether Shared Storage is allowable in a particular context.
  // `top_frame_origin` can be the same as `accessing_origin` in the case of a
  // top-level document calling Shared Storage.
  virtual bool IsSharedStorageAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) const = 0;

  // Controls whether Shared Storage SelectURL is allowable for
  // `accessing_origin` in the context of `top_frame_origin`. Does not override
  // a false return value from IsSharedStorageAllowed.
  // TODO(crbug.com/1378703): This just redirects to the general
  // IsSharedStorageAllowed(). The implementation needs to be updated to reflect
  // the M1 preferences when release 4 is enabled.
  virtual bool IsSharedStorageSelectURLAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) const = 0;

  // Determines whether the Private Aggregation API is allowable in a particular
  // context. `top_frame_origin` is the associated top-frame origin of the
  // calling context. Applicable to all uses of Private Aggregation.
  virtual bool IsPrivateAggregationAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const = 0;

  // Returns whether the profile has the Privacy Sandbox enabled. This consults
  // the main preference, as well as the delegate to check whether the sandbox
  // is restricted. It does not consider any cookie settings. A return value of
  // false means that no Privacy Sandbox operations can occur. A return value of
  // true must be followed up with the appropriate IsXAllowed() call.
  virtual bool IsPrivacySandboxEnabled() const = 0;

  // Allows all Privacy Sandbox prefs for testing. This should be used if tests
  // don't depend on specific access control and just would like to have Privacy
  // Sandbox allowed. Doesn't affect other non-default settings which might
  // disallow APIs e.g. site data exceptions.
  virtual void SetAllPrivacySandboxAllowedForTesting() = 0;

  // Blocks Topics pref for testing.
  virtual void SetTopicsBlockedForTesting() = 0;

  // Disables the Privacy Sandbox completely if |enabled| is false, if |enabled|
  // is true, more granular checks will still be performed, and the delegate
  // consulted, to determine if specific APIs are available in specific
  // contexts.
  // DEPRECATED: Use `SetAllPrivacySandboxAllowedForTesting()` to allow all
  // Privacy Sandbox prefs or per-API block-for-testing functions.
  virtual void SetPrivacySandboxEnabled(bool enabled) = 0;

  // Returns whether Trust Tokens are "generally" available. A return value of
  // false is authoritative, while a value of true must be followed by the
  // appropriate context specific check.
  virtual bool IsTrustTokensAllowed() = 0;

  // Returns whether the Privacy Sandbox is being restricted by the associated
  // delegate. Forwards directly to the corresponding delegate function.
  // Virtual to allow mocking in tests.
  virtual bool IsPrivacySandboxRestricted() const = 0;

  // Called when there's a broad cookies clearing action. For example, this
  // should be called on "Clear browsing data", but shouldn't be called on the
  // Clear-Site-Data header, as it's restricted to a specific site.
  virtual void OnCookiesCleared() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Overrides the internal delegate for test purposes.
  virtual void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) = 0;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
