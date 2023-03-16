// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/settings/chromeos/test_api.test-mojom-test-utils.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

class OSSettingsRecoveryTest : public OSSettingsLockScreenBrowserTestBase {};

// A test fixture that runs tests with recovery feature disabled.
class OSSettingsRecoveryTestWithoutFeature : public OSSettingsRecoveryTest {
 public:
  OSSettingsRecoveryTestWithoutFeature() {
    feature_list_.InitAndDisableFeature(ash::features::kCryptohomeRecovery);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test fixture that runs tests with recovery feature enabled and with
// (faked) hardware support.
class OSSettingsRecoveryTestWithFeature : public OSSettingsRecoveryTest {
 public:
  OSSettingsRecoveryTestWithFeature() {
    feature_list_.InitAndEnableFeature(ash::features::kCryptohomeRecovery);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A test fixture that runs tests with recovery feature enabled but without
// hardware support.
class OSSettingsRecoveryTestWithFeatureWithoutHardwareSupport
    : public OSSettingsRecoveryTestWithFeature {
 public:
  OSSettingsRecoveryTestWithFeatureWithoutHardwareSupport() {
    cryptohome_.set_supports_low_entropy_credentials(false);
  }
};

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithoutFeature, ControlInvisible) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryControlVisibility(false);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeatureWithoutHardwareSupport,
                       ControlInvisible) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryControlVisibility(false);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, ControlVisible) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryControlVisibility(true);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, CheckingEnables) {
  EXPECT_FALSE(cryptohome_.HasRecoveryFactor(GetAccountId()));

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(false);
  lock_screen_settings.EnableRecoveryConfiguration();
  lock_screen_settings.AssertRecoveryConfigured(true);

  EXPECT_TRUE(cryptohome_.HasRecoveryFactor(GetAccountId()));
}

// The following test sets the cryptohome recovery toggle to "on".
// It clicks on the recovery toggle, expecting the recovery dialog to show up.
// It then clicks on the cancel button of the dialog.
// Expected result: The dialog disappears and the toggle is still on.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature,
                       UncheckingDisablesAndCancelClick) {
  cryptohome_.AddRecoveryFactor(GetAccountId());

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(true);
  lock_screen_settings.DisableRecoveryConfiguration(
      mojom::LockScreenSettings::RecoveryDialogAction::CancelDialog);
  lock_screen_settings.AssertRecoveryConfigured(true);
  // After the CancelClick on the dialog, the recovery configuration
  // should remain enabled.
  EXPECT_TRUE(cryptohome_.HasRecoveryFactor(GetAccountId()));
}

// The following test sets the cryptohome recovery toggle to "on".
// It clicks on the recovery toggle, expecting the recovery dialog to show up.
// It then clicks on the disable button of the dialog.
// Expected result: The dialog disappears and the toggle is off.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature,
                       UncheckingDisablesAndDisableClick) {
  cryptohome_.AddRecoveryFactor(GetAccountId());

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(true);
  lock_screen_settings.DisableRecoveryConfiguration(
      mojom::LockScreenSettings::RecoveryDialogAction::ConfirmDisabling);
  lock_screen_settings.AssertRecoveryConfigured(false);

  EXPECT_FALSE(cryptohome_.HasRecoveryFactor(GetAccountId()));
}

// Check that trying to change recovery with an invalidated auth session shows
// the password prompt again.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, DestroyedSession) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();

  // Try to change recovery setting, but with an invalid auth session. This
  // should throw us back to the password prompt.
  cryptohome_.DestroySessions();
  lock_screen_settings.TryEnableRecoveryConfiguration();
  lock_screen_settings.AssertAuthenticated(false);

  // Check that it's still possible to authenticate and change recovery
  // settings.
  EXPECT_FALSE(cryptohome_.HasRecoveryFactor(GetAccountId()));
  lock_screen_settings.Authenticate(kPassword);
  lock_screen_settings.EnableRecoveryConfiguration();
  EXPECT_TRUE(cryptohome_.HasRecoveryFactor(GetAccountId()));
}

}  // namespace ash::settings
