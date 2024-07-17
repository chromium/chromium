// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/mock_tracking_protection_onboarding_delegate.h"

MockTrackingProtectionOnboardingDelegate::
    MockTrackingProtectionOnboardingDelegate() {
  // Setup some reasonable default responses that generally allow APIs.
  // Tests can further override the responses as required.
  SetUpIsEnterpriseManaged(false);
  SetUpIsNewProfile(true);
  SetUpAreThirdPartyCookiesBlocked(false);
}

MockTrackingProtectionOnboardingDelegate::
    ~MockTrackingProtectionOnboardingDelegate() = default;

void MockTrackingProtectionOnboardingDelegate::SetUpIsEnterpriseManaged(
    bool managed) {
  ON_CALL(*this, IsEnterpriseManaged).WillByDefault([=]() { return managed; });
}

void MockTrackingProtectionOnboardingDelegate::SetUpIsNewProfile(
    bool new_profile) {
  ON_CALL(*this, IsNewProfile).WillByDefault([=]() { return new_profile; });
}

void MockTrackingProtectionOnboardingDelegate::SetUpAreThirdPartyCookiesBlocked(
    bool blocked) {
  ON_CALL(*this, AreThirdPartyCookiesBlocked).WillByDefault([=]() {
    return blocked;
  });
}
