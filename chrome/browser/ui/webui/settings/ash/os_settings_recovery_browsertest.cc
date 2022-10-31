// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
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

const char kPassword[] = "asdf";
const char kAuthToken[] = "123";

}  // namespace

namespace ash::settings {

class OSSettingsRecoveryTest : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    cryptohome_.set_enable_auth_check(true);
    cryptohome_.MarkUserAsExisting(GetAccountId());
    cryptohome_.AddGaiaPassword(GetAccountId(), kPassword);

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

 private:
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

// TODO(b/239416325): This should eventually check state in fake user data
// auth, not in the auth factor config mojo service.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, CheckingEnables) {
  auto auth_factor_config = auth::GetAuthFactorConfigForTesting();
  auto recovery_editor = auth::GetRecoveryFactorEditorForTesting();

  ASSERT_EQ(auth::mojom::RecoveryFactorEditor::ConfigureResult::kSuccess,
            recovery_editor.Configure(kAuthToken, false));

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettings();
  lock_screen_settings.AssertRecoveryConfigured(false);
  lock_screen_settings.ToggleRecoveryConfiguration();

  EXPECT_TRUE(auth_factor_config.IsConfigured(
      kAuthToken, auth::mojom::AuthFactor::kRecovery));
}

// TODO(b/239416325): This should eventually check state in fake user data
// auth, not in the auth factor config mojo service.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, UncheckingDisables) {
  auto auth_factor_config = auth::GetAuthFactorConfigForTesting();
  auto recovery_editor = auth::GetRecoveryFactorEditorForTesting();

  ASSERT_EQ(auth::mojom::RecoveryFactorEditor::ConfigureResult::kSuccess,
            recovery_editor.Configure(kAuthToken, true));

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettings();
  lock_screen_settings.AssertRecoveryConfigured(true);
  lock_screen_settings.ToggleRecoveryConfiguration();

  EXPECT_FALSE(auth_factor_config.IsConfigured(
      kAuthToken, auth::mojom::AuthFactor::kRecovery));
}

}  // namespace ash::settings
