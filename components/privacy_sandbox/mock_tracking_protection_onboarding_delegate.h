// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_MOCK_TRACKING_PROTECTION_ONBOARDING_DELEGATE_H_
#define COMPONENTS_PRIVACY_SANDBOX_MOCK_TRACKING_PROTECTION_ONBOARDING_DELEGATE_H_

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockTrackingProtectionOnboardingDelegate
    : public privacy_sandbox::TrackingProtectionOnboarding::Delegate {
 public:
  MockTrackingProtectionOnboardingDelegate();
  ~MockTrackingProtectionOnboardingDelegate() override;

  void SetUpIsEnterpriseManaged(bool managed);
  MOCK_METHOD(bool, IsEnterpriseManaged, (), (const, override));

  void SetUpIsNewProfile(bool new_profile);
  MOCK_METHOD(bool, IsNewProfile, (), (const, override));

  void SetUpAreThirdPartyCookiesBlocked(bool blocked);
  MOCK_METHOD(bool, AreThirdPartyCookiesBlocked, (), (const, override));
};

#endif  // COMPONENTS_PRIVACY_SANDBOX_MOCK_TRACKING_PROTECTION_ONBOARDING_DELEGATE_H_
