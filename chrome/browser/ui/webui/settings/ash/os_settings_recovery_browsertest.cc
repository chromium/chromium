// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_browser_test_mixin.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-test-utils.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPassword[] = "the-password";

}  // namespace

namespace ash::settings {

class OSSettingsRecoveryTest : public MixinBasedInProcessBrowserTest {
 public:
  OSSettingsRecoveryTest() {
    cryptohome_.set_enable_auth_check(true);
    cryptohome_.set_supports_low_entropy_credentials(true);
    cryptohome_.MarkUserAsExisting(GetAccountId());
    cryptohome_.AddGaiaPassword(GetAccountId(), kPassword);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

  // Opens the ChromeOS settings app, goes to the "lock screen" section and
  // enters the password. May be called only once per test.
  mojom::LockScreenSettingsAsyncWaiter OpenLockScreenSettings() {
    CHECK(!os_settings_driver_remote_.is_bound());
    os_settings_driver_remote_ =
        mojo::Remote{os_settings_mixin_.OpenOSSettings()};

    CHECK(!lock_screen_settings_remote_.is_bound());
    lock_screen_settings_remote_ = mojo::Remote{
        mojom::OSSettingsDriverAsyncWaiter{os_settings_driver_remote_.get()}
            .GoToLockScreenSettings()};

    mojom::LockScreenSettingsAsyncWaiter{lock_screen_settings_remote_.get()}
        .Authenticate(kPassword);
    return mojom::LockScreenSettingsAsyncWaiter{
        lock_screen_settings_remote_.get()};
  }

  const AccountId& GetAccountId() {
    return logged_in_user_mixin_.GetAccountId();
  }

 protected:
  CryptohomeMixin cryptohome_{&mixin_host_};
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(), this};
  OSSettingsBrowserTestMixin os_settings_mixin_{&mixin_host_};

  mojo::Remote<mojom::OSSettingsDriver> os_settings_driver_remote_;
  mojo::Remote<mojom::LockScreenSettings> lock_screen_settings_remote_;
};

class OSSettingsRecoveryTestWithFeature : public OSSettingsRecoveryTest {
 public:
  OSSettingsRecoveryTestWithFeature() {
    feature_list_.InitAndEnableFeature(ash::features::kCryptohomeRecoverySetup);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class OSSettingsRecoveryTestWithoutFeature : public OSSettingsRecoveryTest {
 public:
  OSSettingsRecoveryTestWithoutFeature() {
    feature_list_.InitAndDisableFeature(
        ash::features::kCryptohomeRecoverySetup);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithoutFeature,
                       ControlNotVisible) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettings();
  lock_screen_settings.AssertRecoveryControlVisibility(false);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, ControlVisible) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettings();
  lock_screen_settings.AssertRecoveryControlVisibility(true);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, CheckingEnables) {
  EXPECT_FALSE(cryptohome_.HasRecoveryFactor(GetAccountId()));

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettings();
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
      OpenLockScreenSettings();
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
      OpenLockScreenSettings();
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
      OpenLockScreenSettings();

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
