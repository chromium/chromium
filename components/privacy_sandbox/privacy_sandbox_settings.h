// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

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

  // Determine whether |destination_origin| is allowed to receive events
  // (reportEvent(), automatic beacons) reported by an API like Protected
  // Audience or Shared Storage. This does not check if the API itself is
  // allowed by the calling context, since the corresponding registerAdBeacon
  // and selectUrl caller sites were also checked for attestation.
  virtual bool IsEventReportingDestinationAttested(
      const url::Origin& destination_origin,
      privacy_sandbox::PrivacySandboxAttestationsGatedAPI invoking_api)
      const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Source of truth for whether related website sets are enabled.
  virtual bool AreRelatedWebsiteSetsEnabled() const = 0;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
