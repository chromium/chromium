// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "components/browsing_topics/common/common_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "content/public/browser/interest_group_api_operation.h"

#include "base/time/time.h"

class GURL;

namespace content {
class RenderFrameHost;
}

namespace url {
class Origin;
}

namespace privacy_sandbox {

class CanonicalTopic;

// When a new enum value is added:
// 1. Update kMaxValue to match it.
// 2. Update `PrivacySandboxAttestationsGatedAPIProto` in
//    `privacy_sandbox_attestations.proto`.
// 3. Update `InsertAPI` in `privacy_sandbox_attestations_parser.cc`.
enum class PrivacySandboxAttestationsGatedAPI {
  kTopics,
  kProtectedAudience,
  kPrivateAggregation,
  kAttributionReporting,
  kSharedStorage,
  kLocalUnpartitionedDataAccess,

  kMaxValue = kLocalUnpartitionedDataAccess,
};

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

    // Allows the delegate to query in real time if Privacy Sandbox is currently
    // unrestricted. Unlike IsPrivacySandboxRestricted, does NOT
    // restrict/unrestrict access to the Privacy Sandbox.
    virtual bool IsPrivacySandboxCurrentlyUnrestricted() const = 0;

    // Whether the current profile is Incognito or not. For Incognito, the
    // privacy sandbox APIs are restricted.
    virtual bool IsIncognitoProfile() const = 0;

    // Whether there is an appropriate level of consent for the Topics API.
    // When this returns false, access control functions for Topics will
    // return as not allowed.
    virtual bool HasAppropriateTopicsConsent() const = 0;

    // Whether the profile is subject to being given notice of restrictions to
    // the standard set of Privacy Sandbox APIs.
    virtual bool IsSubjectToM1NoticeRestricted() const = 0;

    // Whether the Privacy Sandbox is partially enabled based on
    // restrictions.
    virtual bool IsRestrictedNoticeEnabled() const = 0;

    // Whether the profile is eligible for 3PCD experiment. The eligibility
    // applies for both mode A and mode B experiments.
    virtual bool IsCookieDeprecationExperimentEligible() const = 0;

    // Returns the profile's computed eligibility for 3PCD experiment. The
    // eligibility applies for both mode A and mode B experiments. Unlike
    // `IsCookieDeprecationExperimentEligible` this method returns the real time
    // eligibility.
    virtual TpcdExperimentEligibility
    GetCookieDeprecationExperimentCurrentEligibility() const = 0;

    // Whether cookie deprecation label is allowed.
    virtual bool IsCookieDeprecationLabelAllowed() const = 0;

    // Whether third-party cookies are blocked due to cookie deprecation
    // experiment. Also returns false if users explicitly block third-party
    // cookies.
    virtual bool AreThirdPartyCookiesBlockedByCookieDeprecationExperiment()
        const = 0;
  };

  // Returns whether the Topics API is allowed at all. If false, Topics API
  // calculations should not occur. If true, the more specific function,
  // IsTopicsApiAllowedForContext(), should be consulted for the relevant
  // context.
  virtual bool IsTopicsAllowed() const = 0;

  // Determines whether the Topics API is allowable in a particular context.
  // |top_frame_origin| is used to check for content settings which could both
  // affect 1P and 3P contexts.
  // If provided, `console_frame` is used to log errors to the console upon
  // attestation failure.
  virtual bool IsTopicsAllowedForContext(
      const url::Origin& top_frame_origin,
      const GURL& url,
      content::RenderFrameHost* console_frame = nullptr) const = 0;

  // Returns whether |topic| can be either considered as a top topic for the
  // current epoch, or provided to a website as a previous / current epochs
  // site assigned topic.
  virtual bool IsTopicAllowed(const CanonicalTopic& topic) = 0;

  // Returns whether |topic| is prioritized by Finch settings.
  virtual bool IsTopicPrioritized(const CanonicalTopic& topic) = 0;

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

  // Returns whether any Attribution Reporting operation would ever be allowed.
  // If false, no attribution reporting operation is allowed (e.g. because the
  // user has disabled the setting). If true, the appropriate context specific
  // check must also be made.
  virtual bool IsAttributionReportingEverAllowed() const = 0;

  // Determines whether Attribution Reporting is allowable in a particular
  // context. Should be called at both source and trigger registration. At each
  // of these points |top_frame_origin| is the same as either the source origin
  // or the destination origin respectively.
  // If provided, `console_frame` is used to log errors to the console upon
  // attestation failure.
  virtual bool IsAttributionReportingAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      content::RenderFrameHost* console_frame = nullptr) const = 0;

  // Called before sending the associated attribution report to
  // |reporting_origin|. Re-checks that |reporting_origin| is allowable as a 3P
  // on both |source_origin| and |destination_origin|.
  // If provided, `console_frame` is used to log errors to the console upon
  // attestation failure.
  virtual bool MaySendAttributionReport(
      const url::Origin& source_origin,
      const url::Origin& destination_origin,
      const url::Origin& reporting_origin,
      content::RenderFrameHost* console_frame = nullptr) const = 0;

  // Determines whether Attribution Reporting API's transitional debug reporting
  // is allowable in a particular context. Note that
  // `IsAttributionReportingAllowed()` should be called prior to this.
  // |can_bypass| indicates whether the result can be bypassed which is set to
  // true when it's disallowed due to the cookie deprecation experiment.
  //
  // TODO(crbug.com/40941634): Clean up `can_bypass` after the cookie
  // deprecation experiment.
  virtual bool IsAttributionReportingTransitionalDebuggingAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      bool& can_bypass) const = 0;

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

  // Determine whether |auction_party| can register an interest group, or sell
  // buy in an auction, on |top_frame_origin|.
  // If provided, `console_frame` is used to log errors to the console upon
  // attestation failure.
  virtual bool IsFledgeAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& auction_party,
      content::InterestGroupApiOperation interest_group_api_operation,
      content::RenderFrameHost* console_frame = nullptr) const = 0;

  // Determine whether |destination_origin| is allowed to receive events
  // (reportEvent(), automatic beacons) reported by an API like Protected
  // Audience or Shared Storage. This does not check if the API itself is
  // allowed by the calling context, since the corresponding registerAdBeacon
  // and selectUrl caller sites were also checked for attestation.
  virtual bool IsEventReportingDestinationAttested(
      const url::Origin& destination_origin,
      privacy_sandbox::PrivacySandboxAttestationsGatedAPI invoking_api)
      const = 0;

  // Determines whether Shared Storage is allowable in a particular context.
  // `top_frame_origin` can be the same as `accessing_origin` in the case of a
  // top-level document calling Shared Storage.
  //
  // If non-null, `out_debug_message` is updated in this call to relay details
  // back to the caller about how the returned boolean result was obtained.
  //
  // If provided, `console_frame` is used to log errors to the console upon
  // attestation failure.
  //
  // The out parameter `out_block_is_site_setting_specific` will be set to true
  // in the case that the return value is false and the failure to be allowed is
  // due to site-settings. Otherwise the parameter will be set to false (because
  // either the return value is true, or the failure is due to a
  // non-site-setting-specific reason).
  virtual bool IsSharedStorageAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      content::RenderFrameHost* console_frame,
      bool* out_block_is_site_setting_specific) const = 0;

  // Controls whether Shared Storage SelectURL is allowable for
  // `accessing_origin` in the context of `top_frame_origin`. Does not override
  // a false return value from IsSharedStorageAllowed.
  //
  // If non-null, `out_debug_message` is updated in this call to relay details
  // back to the caller about how the returned boolean result was obtained.
  //
  // The out parameter `out_block_is_site_setting_specific` will be set to true
  // in the case that the return value is false and the failure to be allowed is
  // due to site-settings. Otherwise the parameter will be set to false (because
  // either the return value is true, or the failure is due to a
  // non-site-setting-specific reason).
  virtual bool IsSharedStorageSelectURLAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific) const = 0;

  // Controls whether shared storage access from fenced frame is allowable for
  // `accessing_origin` in the context of `top_frame_origin`.
  //
  // If provided, `console_frame` is used to log errors to the console upon
  // attestation failure.
  virtual bool IsLocalUnpartitionedDataAccessAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      content::RenderFrameHost* console_frame) const = 0;

  // Determines whether the Private Aggregation API is allowable in a particular
  // context. `top_frame_origin` is the associated top-frame origin of the
  // calling context. Applicable to all uses of Private Aggregation.
  //
  // The out parameter `out_block_is_site_setting_specific` will be set to true
  // in the case that the return value is false and the failure to be allowed is
  // due to site-settings. Otherwise the parameter will be set to false (because
  // either the return value is true, or the failure is due to a
  // non-site-setting-specific reason).
  virtual bool IsPrivateAggregationAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin,
      bool* out_block_is_site_setting_specific) const = 0;

  // Determines whether the Private Aggregation API's debug mode is allowable in
  // a particular context. Note that if IsPrivateAggregationAllowed() is false,
  // this will always be false too. `top_frame_origin` is the associated
  // top-frame origin of the calling context. Applicable to all uses of Private
  // Aggregation.
  virtual bool IsPrivateAggregationDebugModeAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const = 0;

  // Returns the profile computed eligibility for 3PCD experiments.
  // This consults the delegate for the real time eligibility of the profile.
  // The eligibility applies for both mode A and mode B experiments.
  virtual TpcdExperimentEligibility
  GetCookieDeprecationExperimentCurrentEligibility() const = 0;

  // Determines whether cookie deprecation label is allowable. This consults
  // whether the profile is eligible for 3PCD experiments. If true, the more
  // specific function, IsCookieDeprecationLabelAllowed(), should be consulted
  // for the relevant context.
  virtual bool IsCookieDeprecationLabelAllowed() const = 0;

  // Determines whether cookie deprecation label is allowable for
  // `context_origin` in the context of `top_frame_origin`.
  virtual bool IsCookieDeprecationLabelAllowedForContext(
      const url::Origin& top_frame_origin,
      const url::Origin& context_origin) const = 0;

  // Allows all Privacy Sandbox prefs for testing. This should be used if tests
  // don't depend on specific access control and just would like to have Privacy
  // Sandbox allowed. Doesn't affect other non-default settings which might
  // disallow APIs e.g. site data exceptions.
  virtual void SetAllPrivacySandboxAllowedForTesting() = 0;

  // Blocks Topics pref for testing.
  virtual void SetTopicsBlockedForTesting() = 0;

  // Returns whether the Privacy Sandbox is being restricted by the associated
  // delegate. Forwards directly to the corresponding delegate function.
  // Virtual to allow mocking in tests.
  virtual bool IsPrivacySandboxRestricted() const = 0;

  // Returns whether the Privacy Sandbox is being unrestricted by the associated
  // delegate. Forwards directly to the corresponding delegate function.
  // Virtual to allow mocking in tests. Unlike IsPrivacySandboxRestricted
  // this method always return the current restriction status.
  virtual bool IsPrivacySandboxCurrentlyUnrestricted() const = 0;

  // Returns whether the privacy sandbox restricted notice should be shown,
  // based on account characteristics. Forwards to the delegate. Virtual for
  // mocking in tests.
  virtual bool IsSubjectToM1NoticeRestricted() const = 0;

  // Returns whether the Privacy Sandbox is partially enabled based on
  // restrictions. Forwards to the delegate. Virtual for
  // mocking in tests.
  virtual bool IsRestrictedNoticeEnabled() const = 0;

  // Called when there's a broad cookies clearing action. For example, this
  // should be called on "Clear browsing data", but shouldn't be called on the
  // Clear-Site-Data header, as it's restricted to a specific site.
  virtual void OnCookiesCleared() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Overrides the internal delegate for test purposes.
  virtual void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) = 0;

  // Source of truth for whether related websites are enabled.
  virtual bool AreRelatedWebsiteSetsEnabled() const = 0;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
