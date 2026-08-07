// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class RenderFrameHost;
}

namespace url {
class Origin;
}

namespace privacy_sandbox {

// When a new enum value is added:
// 1. Update kMaxValue to match it.
// 2. Update `PrivacySandboxAttestationsGatedAPIProto` in
//    `privacy_sandbox_attestations.proto`.
// 3. Update `InsertAPI` in `privacy_sandbox_attestations_parser.cc`.
enum class PrivacySandboxAttestationsGatedAPI {
  kTopics,
  kProtectedAudience,
  kPrivateAggregation,
  kSharedStorage,

  kMaxValue = kSharedStorage,
};

// The possible operations performable by parties related to the Interest
// Group API.
enum class InterestGroupApiOperation {
  kJoin,
  kLeave,
  kUpdate,
  kSell,
  kBuy,
  kRead
};

// A service which acts as a intermediary between Privacy Sandbox APIs and
// the preferences and content settings which define when they are allowed
// to be accessed. Privacy Sandbox APIs, regardless of where they live
// (renderer, browser, network etc), must consult this service to determine
// when they are allowed to run. While a basic on/off control is provided by
// this service, embedders are expected to achieve fine-grained control
// though the underlying preferences and content settings separately.
class PrivacySandboxSettings : public KeyedService {
 public:
  class Observer {
   public:
    // Fired when the Related Website Sets changes to being `enabled` as a
    // result of the kPrivacySandboxRelatedWebsiteSets preference changing.
    virtual void OnRelatedWebsiteSetsEnabledChanged(bool enabled) {}
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Allows the delegate to restrict access to the Privacy Sandbox. When
    // the Privacy Sandbox is restricted, all API access is disabled. This
    // is consulted on every access check, and it is acceptable for this to
    // change return value over the life of the service.
    virtual bool IsPrivacySandboxRestricted() const = 0;

    // Allows the delegate to query in real time if Privacy Sandbox is currently
    // unrestricted. Unlike IsPrivacySandboxRestricted, does NOT
    // restrict/unrestrict access to the Privacy Sandbox.
    virtual bool IsPrivacySandboxCurrentlyUnrestricted() const = 0;

    // Whether the current profile is Incognito or not. For Incognito, the
    // privacy sandbox APIs are restricted.
    virtual bool IsIncognitoProfile() const = 0;
  };

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

  // Returns whether the Privacy Sandbox is being restricted by the associated
  // delegate. Forwards directly to the corresponding delegate function.
  // Virtual to allow mocking in tests.
  virtual bool IsPrivacySandboxRestricted() const = 0;

  // Returns whether the Privacy Sandbox is being unrestricted by the associated
  // delegate. Forwards directly to the corresponding delegate function.
  // Virtual to allow mocking in tests. Unlike IsPrivacySandboxRestricted
  // this method always return the current restriction status.
  virtual bool IsPrivacySandboxCurrentlyUnrestricted() const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Overrides the internal delegate for test purposes.
  virtual void SetDelegateForTesting(std::unique_ptr<Delegate> delegate) = 0;

  // Source of truth for whether related website sets are enabled.
  virtual bool AreRelatedWebsiteSetsEnabled() const = 0;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
