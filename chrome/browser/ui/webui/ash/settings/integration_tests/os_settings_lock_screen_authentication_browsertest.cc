// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

namespace {

using PasswordType = OSSettingsLockScreenBrowserTestBase::PasswordType;

// Name and value of the metric that records authentication on the lock screen
// page.
const char kPinUnlockUmaHistogramName[] = "Settings.PinUnlockSetup";
const base::HistogramBase::Sample kEnterPasswordCorrectly = 1;

}  // namespace

// Test of the authentication dialog in the lock screen page in os-settings.
class OSSettingsLockScreenAuthenticationTest
    : public OSSettingsLockScreenBrowserTestBase,
      public testing::WithParamInterface<PasswordType> {
 public:
  OSSettingsLockScreenAuthenticationTest()
      : OSSettingsLockScreenBrowserTestBase(GetParam()) {}

  // Password constants used in test cases. The correct password is the same
  // one as the one set up through the OSSettingsLockScreenBrowserTestBase test
  // fixture.
  static constexpr const char* kCorrectPassword =
      OSSettingsLockScreenBrowserTestBase::kPassword;
  static constexpr char kIncorrectPassword[] = "incorrect-password";
};

INSTANTIATE_TEST_SUITE_P(OSSettingsLockScreenAuthenticationTests,
                         OSSettingsLockScreenAuthenticationTest,
                         testing::Values(PasswordType::kGaia,
                                         PasswordType::kLocal));

IN_PROC_BROWSER_TEST_P(OSSettingsLockScreenAuthenticationTest,
                       SuccessfulUnlock) {
  base::HistogramTester histograms;
  auto lock_screen_settings = OpenLockScreenSettings();
  lock_screen_settings.AssertAuthenticated(false);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName,
                               kEnterPasswordCorrectly, 0);

  lock_screen_settings.Authenticate(kCorrectPassword);

  lock_screen_settings.AssertAuthenticated(true);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName,
                               kEnterPasswordCorrectly, 1);
}

IN_PROC_BROWSER_TEST_P(OSSettingsLockScreenAuthenticationTest, FailedUnlock) {
  base::HistogramTester histograms;
  auto lock_screen_settings = OpenLockScreenSettings();

  lock_screen_settings.AssertAuthenticated(false);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName,
                               kEnterPasswordCorrectly, 0);

  lock_screen_settings.AuthenticateIncorrectly(kIncorrectPassword);

  lock_screen_settings.AssertAuthenticated(false);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName,
                               kEnterPasswordCorrectly, 0);

  // Check that we can still authenticate after an unsuccessful attempt:
  lock_screen_settings.Authenticate(kCorrectPassword);

  lock_screen_settings.AssertAuthenticated(true);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName,
                               kEnterPasswordCorrectly, 1);
}

}  // namespace ash::settings
