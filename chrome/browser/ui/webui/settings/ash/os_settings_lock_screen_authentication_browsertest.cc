// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_settings_lock_screen_browser_test_base.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

// Test of the authentication dialog in the lock screen page in os-settings.
class OSSettingsLockScreenAuthenticationTest
    : public OSSettingsLockScreenBrowserTestBase {
 public:
  static constexpr const char* kCorrectPassword =
      OSSettingsLockScreenBrowserTestBase::kPassword;
  static constexpr char kIncorrectPassword[] = "incorrect-password";
};

IN_PROC_BROWSER_TEST_F(OSSettingsLockScreenAuthenticationTest,
                       SuccessfulUnlock) {
  auto lock_screen_settings = OpenLockScreenSettings();
  lock_screen_settings.AssertAuthenticated(false);
  lock_screen_settings.Authenticate(kCorrectPassword);
  lock_screen_settings.AssertAuthenticated(true);
}

IN_PROC_BROWSER_TEST_F(OSSettingsLockScreenAuthenticationTest, FailedUnlock) {
  auto lock_screen_settings = OpenLockScreenSettings();
  lock_screen_settings.AssertAuthenticated(false);
  lock_screen_settings.AuthenticateIncorrectly(kIncorrectPassword);
  lock_screen_settings.AssertAuthenticated(false);
  lock_screen_settings.Authenticate(kCorrectPassword);
  lock_screen_settings.AssertAuthenticated(true);
}

}  // namespace ash::settings
