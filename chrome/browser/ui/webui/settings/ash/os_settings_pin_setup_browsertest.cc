// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_lock_screen_browser_test_base.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

namespace {

const char kFirstPin[] = "111111";
const char kSecondPin[] = "22222222";
const char kIncorrectPin[] = "333333333";

}  // namespace

// Tests PIN-related settings in the ChromeOS settings page.
class OSSettingsPinSetupTest : public OSSettingsLockScreenBrowserTestBase {
 public:
  PrefService& Prefs() {
    PrefService* service =
        ProfileHelper::Get()->GetProfileByAccountId(GetAccountId())->GetPrefs();
    CHECK(service);
    return *service;
  }

  bool GetPinAutoSubmitState() {
    return Prefs().GetBoolean(::prefs::kPinUnlockAutosubmitEnabled);
  }
};

// Tests that the happy path of setting and removing PINs works.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, SetRemove) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertIsUsingPin(false);

  // Remove the pin. Nothing should happen.
  lock_screen_settings.RemovePin();
  lock_screen_settings.AssertIsUsingPin(false);
  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));

  // Set a pin. Cryptohome should be aware of the pin.
  lock_screen_settings.SetPin(kFirstPin);
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));
  // TODO(b/243696986): Lock the screen or sign out and check that the PIN
  // works.

  // Change the pin.
  lock_screen_settings.SetPin(kSecondPin);
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));

  // Change the pin, but to the same value.
  lock_screen_settings.SetPin(kSecondPin);
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));

  // Remove the pin.
  lock_screen_settings.RemovePin();
  // TODO(b/256584110): We can't reliable test the following:
  //
  //   EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));
  //
  // because the UI reports the pin as being removed before it's actually
  // removed.

  // Setting up a pin should still work.
  lock_screen_settings.SetPin(kFirstPin);
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));
}

// Tests enabling and disabling autosubmit.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, Autosubmit) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  // Set a pin. Autosubmit should be enabled.
  lock_screen_settings.SetPin(kFirstPin);
  lock_screen_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  // Change, remove and add pin again. Nothing of this should affect the pin
  // autosubmit pref.
  lock_screen_settings.SetPin(kSecondPin);
  lock_screen_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  lock_screen_settings.RemovePin();
  lock_screen_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  lock_screen_settings.SetPin(kSecondPin);
  lock_screen_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  // Disable pin autosubmit. This should turn the pref off, but the pin should
  // still be active.
  lock_screen_settings.DisablePinAutosubmit();
  lock_screen_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));

  // Try to enable pin autosubmit using the wrong pin. This should not succeed.
  lock_screen_settings.EnablePinAutosubmitIncorrectly(kIncorrectPin);
  lock_screen_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());

  // Try to enable pin autosubmit using the correct pin. This should succeed.
  lock_screen_settings.EnablePinAutosubmit(kSecondPin);
  lock_screen_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  // Even after we have authenticated with the correct pin, we should be able
  // to remove the pin.
  lock_screen_settings.RemovePin();
  lock_screen_settings.AssertIsUsingPin(false);
  // TODO(b/256584110): We can't reliable test the following:
  //
  //   EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));
  //
  // because the UI reports the pin as being removed before it's actually
  // removed.
}

}  // namespace ash::settings
