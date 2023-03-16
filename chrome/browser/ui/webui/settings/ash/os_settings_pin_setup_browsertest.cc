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

// PINs to be used for checking PIN verification logic.
const char kFirstPin[] = "111111";
const char kSecondPin[] = "222222";
const char kIncorrectPin[] = "333333";

// PINs to be used for checking minimal/maximum length PINs.
const size_t kMinimumPinLengthForTest = 5;
const size_t kMaximumPinLengthForTest = 10;
const char kMinimumLengthPin[] = "11223";
const char kMaximumLengthPin[] = "1122334455";

// A weak PIN used to verify that a warning is displayed.
const char kWeakPin[] = "111111";

// Twelve digit PINs are allowed for autosubmit, but no more.
const char kMaximumLengthPinForAutosubmit[] = "321321321321";
const char kTooLongPinForAutosubmit[] = "3213213213213";

// Name and value of the metric that records authentication on the lock screen
// page.
const char kPinUnlockUmaHistogramName[] = "Settings.PinUnlockSetup";
const base::HistogramBase::Sample kChoosePinOrPassword = 2;
const base::HistogramBase::Sample kEnterPin = 3;
const base::HistogramBase::Sample kConfirmPin = 4;

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

// Tests that the happy path of setting and removing PINs works, and that all
// relevant metrics are recorded.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, SetRemove) {
  // Launch first instance of the settings page, setup and remove PIN a few
  // times.
  {
    auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
    lock_screen_settings.AssertIsUsingPin(false);
    EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));

    // Remove the pin. Nothing should happen.
    lock_screen_settings.RemovePin();
    lock_screen_settings.AssertIsUsingPin(false);
    EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));

    // Set a pin. Cryptohome should be aware of the pin, and we should record
    // that all stages of PIN setup were completed in UMA.
    {
      base::HistogramTester histograms;
      lock_screen_settings.SetPin(kFirstPin);
      EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));
      histograms.ExpectBucketCount(kPinUnlockUmaHistogramName,
                                   kChoosePinOrPassword, 1);
      // TODO(b/270962495): kEnterPin and kConfirmPin are currently not
      // recorded in the os-settings page, but this is likely a bug.
      histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kEnterPin, 0);
      histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kConfirmPin, 0);
      histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 1);
    }
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
    lock_screen_settings.AssertIsUsingPin(true);
    EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));
  }

  // Opening the settings page again should display that a PIN is configured.
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertIsUsingPin(true);
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));
}

// Tests that nothing is persisted when just selecting the "PIN and password"
// checkbox. This only makes another "setup PIN" button appear, and ony after
// clicking that button and going through the PIN setup flow is the PIN fully
// set up.
// We do have to check that the relevant metric is recorded though.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, SelectPinAndPassword) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  lock_screen_settings.AssertIsUsingPin(false);
  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));

  base::HistogramTester histograms;
  lock_screen_settings.SelectPinAndPassword();

  lock_screen_settings.AssertIsUsingPin(true);
  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));
  // TODO(b/270962495): See Comment #2 in the linked bug. We don't record
  // kChoosePinOrPassword when the user clicks on "PIN and password" but when
  // they click "setup pin". That might be a bug.
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kChoosePinOrPassword,
                               0);
  histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 0);
}

// Tests that nothing is persisted when cancelling the PIN setup dialog after
// entering the PIN only once. We should record this in UMA though.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, SetPinButCancelConfirmation) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  lock_screen_settings.AssertIsUsingPin(false);
  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));

  base::HistogramTester histograms;
  lock_screen_settings.SetPinButCancelConfirmation(kFirstPin);

  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kChoosePinOrPassword,
                               1);
  // TODO(b/270962495): kEnterPin and kConfirmPin are currently not
  // recorded in the os-settings page, but this is likely a bug.
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kEnterPin, 0);
  histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 1);
}

// Tests that nothing is persisted during setup when the PIN that is entered
// the second time for confirmation does not match the first PIN. We should
// record this in UMA though.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, SetPinButFailConfirmation) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  lock_screen_settings.AssertIsUsingPin(false);
  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));

  base::HistogramTester histograms;
  lock_screen_settings.SetPinButFailConfirmation(kFirstPin, kIncorrectPin);

  EXPECT_EQ(false, cryptohome_.HasPinFactor(GetAccountId()));
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kChoosePinOrPassword,
                               1);
  // TODO(b/270962495): kEnterPin and kConfirmPin are currently not
  // recorded in the os-settings page, but this is likely a bug.
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kEnterPin, 0);
  histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 1);
}

// Tests that PIN setup UI validates minimal pin lengths.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, MinimumPinLength) {
  Prefs().SetInteger(prefs::kPinUnlockMinimumLength, kMinimumPinLengthForTest);

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  // Check that a minimum length PIN is accepted, but that the PIN obtained by
  // removing the last digit is rejected.
  std::string too_short_pin{kMinimumLengthPin};
  too_short_pin.pop_back();

  // SetPinButTooShort checks that a warning is displayed.
  lock_screen_settings.SetPinButTooShort(std::move(too_short_pin),
                                         kMinimumLengthPin);
}

// Tests that PIN setup UI validates maximal pin lengths.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, MaximumPinLength) {
  Prefs().SetInteger(prefs::kPinUnlockMaximumLength, kMaximumPinLengthForTest);

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  // Check that a maximum length PIN is accepted, but that the PIN obtained by
  // duplicating the last digit is rejected.
  std::string too_long_pin{kMaximumLengthPin};
  too_long_pin += too_long_pin.back();

  // SetPinButTooLong checks that a warning is displayed.
  lock_screen_settings.SetPinButTooLong(std::move(too_long_pin),
                                        kMaximumLengthPin);
}

// Tests that a warning is displayed when setting up a weak PIN, but that it is
// still possible.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, WeakPinWarning) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  // SetPinWithWarning checks that a warning is displayed.
  lock_screen_settings.SetPinWithWarning(kWeakPin);

  lock_screen_settings.AssertIsUsingPin(true);
  EXPECT_EQ(true, cryptohome_.HasPinFactor(GetAccountId()));
}

// Tests that the PIN setup dialog handles key events appropriately.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, PressKeysInPinSetupDialog) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  // CheckPinSetupDialogKeyInput checks that some relevant keys (digit,
  // character, function) have an effect or have no effect.
  lock_screen_settings.CheckPinSetupDialogKeyInput();
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

// Tests the maximum length of PINs for autosubmit.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, MaximumLengthAutosubmit) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  // Set a maximum length pin. Autosubmit should be enabled.
  lock_screen_settings.SetPin(kMaximumLengthPinForAutosubmit);
  lock_screen_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());
  // Remove the PIN again.
  lock_screen_settings.RemovePin();

  // Set an overly long PIN. Autosubmit should be disabled, and we shouldn't be
  // able to turn it on.
  lock_screen_settings.SetPin(kTooLongPinForAutosubmit);
  lock_screen_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());

  lock_screen_settings.EnablePinAutosubmitTooLong(kTooLongPinForAutosubmit);
  lock_screen_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());
}

// Tests that the user is asked to reauthenticate when trying to enable PIN
// autosubmit but with a locked-out PIN.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, AutosubmitWithLockedPin) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();

  lock_screen_settings.SetPin(kFirstPin);
  // We disable autosubmit so that we can try to reenable.
  lock_screen_settings.DisablePinAutosubmit();

  cryptohome_.SetPinLocked(GetAccountId(), true);

  lock_screen_settings.TryEnablePinAutosubmitWithLockedPin(
      kFirstPin, OSSettingsLockScreenBrowserTestBase::kPassword);
  EXPECT_EQ(false, GetPinAutoSubmitState());
  lock_screen_settings.AssertPinAutosubmitEnabled(false);
}

}  // namespace ash::settings
