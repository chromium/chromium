// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

// Deep link ID to the auto screen lock control on the OS settings page.
const char kAutoScreenLockSettingsId[] = "1109";

namespace ash::settings {

// Tests the toggle that controls automatic lock in the lock-screen section of
// the chrome://os-settings webui.
class OSSettingsAutoScreenLockTest
    : public OSSettingsLockScreenBrowserTestBase {
 public:
  OSSettingsAutoScreenLockTest()
      : OSSettingsLockScreenBrowserTestBase(ash::AshAuthFactor::kGaiaPassword) {
  }
  // Returns whether or not automatic screen lock is enabled according to
  // preferences.
  bool IsAutoScreenLockPrefEnabled() {
    PrefService* service =
        ProfileHelper::Get()->GetProfileByAccountId(GetAccountId())->GetPrefs();
    CHECK(service);
    return service->GetBoolean(prefs::kEnableAutoScreenLock);
  }
};

IN_PROC_BROWSER_TEST_F(OSSettingsAutoScreenLockTest, Toggle) {
  // Open settings page and flip the automatic screen lock setting a few times.
  // After this block, automatic screen lock should be enabled.
  {
    mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
        OpenLockScreenSettingsAndAuthenticate();

    lock_screen_settings.AssertAutoLockScreenEnabled(false);
    CHECK(!IsAutoScreenLockPrefEnabled());

    lock_screen_settings.EnableAutoLockScreen();

    lock_screen_settings.AssertAutoLockScreenEnabled(true);
    CHECK(IsAutoScreenLockPrefEnabled());

    lock_screen_settings.DisableAutoLockScreen();

    lock_screen_settings.AssertAutoLockScreenEnabled(false);
    CHECK(!IsAutoScreenLockPrefEnabled());

    lock_screen_settings.EnableAutoLockScreen();
  }

  // Launch a new instance of os-settings and check that the lock screen
  // setting is displayed as expected.
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertAutoLockScreenEnabled(true);
}

// Checks that the deep link to the auto screen lock toggle works.
IN_PROC_BROWSER_TEST_F(OSSettingsAutoScreenLockTest, DeepLink) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsDeepLinkAndAuthenticate(kAutoScreenLockSettingsId);
  lock_screen_settings.AssertAutoLockScreenFocused();
}

}  // namespace ash::settings
