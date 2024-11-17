// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/chromeos/settings/os_people_page/password_settings_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

class OSSettingsAuthFactorSetupTest
    : public OSSettingsLockScreenBrowserTestBase {
  using OSSettingsLockScreenBrowserTestBase::
      OSSettingsLockScreenBrowserTestBase;

 public:
  explicit OSSettingsAuthFactorSetupTest(ash::AshAuthFactor type)
      : OSSettingsLockScreenBrowserTestBase(type) {}
  mojom::PasswordSettingsApiAsyncWaiter GoToPasswordSettings(
      mojom::LockScreenSettingsAsyncWaiter& lock_screen_settings) {
    password_settings_remote_ =
        mojo::Remote(lock_screen_settings.GoToPasswordSettings());
    return mojom::PasswordSettingsApiAsyncWaiter(
        password_settings_remote_.get());
  }

 private:
  mojo::Remote<mojom::PasswordSettingsApi> password_settings_remote_;
};

class OSSettingsAuthFactorSetupTestWithGaiaPassword
    : public OSSettingsAuthFactorSetupTest,
      public testing::WithParamInterface<bool> {
 public:
  OSSettingsAuthFactorSetupTestWithGaiaPassword()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kGaiaPassword) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features.push_back(features::kChangePasswordFactorSetup);
    } else {
      disabled_features.push_back(features::kChangePasswordFactorSetup);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OSSettingsAuthFactorSetupTestWithGaiaPassword,
                         testing::Bool());

class OSSettingsAuthFactorSetupTestWithLocalPassword
    : public OSSettingsAuthFactorSetupTest {
 public:
  OSSettingsAuthFactorSetupTestWithLocalPassword()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kLocalPassword) {}
};

// If user has Gaia password, the control for changing passwords is shown if
// `kChangePasswordFactorSetup` feature is enabled; otherwise, it should not be
// shown.
IN_PROC_BROWSER_TEST_P(OSSettingsAuthFactorSetupTestWithGaiaPassword,
                       Visibility) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  bool should_show_password_control = GetParam();
  lock_screen_settings.AssertPasswordControlVisibility(
      should_show_password_control);
  if (should_show_password_control) {
    mojom::PasswordSettingsApiAsyncWaiter password_settings =
        GoToPasswordSettings(lock_screen_settings);
    password_settings.AssertCanOpenLocalPasswordDialog();
    password_settings.AssertSubmitButtonDisabledForInvalidPasswordInput();
    password_settings.AssertSubmitButtonEnabledForValidPasswordInput();
  }
}

// The control for changing passwords is shown if user has local password.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithLocalPassword, Shown) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
}

class OSSettingsAuthFactorSetupTestWithPinOnly
    : public OSSettingsAuthFactorSetupTest {
 public:
  OSSettingsAuthFactorSetupTestWithPinOnly()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kCryptohomePin) {
    cryptohome_->set_supports_low_entropy_credentials(true);
    cryptohome_->set_enable_auth_check(false);

    scoped_feature_list_.InitAndEnableFeature(
        features::kChangePasswordFactorSetup);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The control for setting passwords is shown if user has pin only setup.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithPinOnly, Shown) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);
  password_settings.AssertCanOpenLocalPasswordDialog();
  password_settings.AssertSubmitButtonDisabledForInvalidPasswordInput();
  password_settings.AssertSubmitButtonEnabledForValidPasswordInput();
}

}  // namespace ash::settings
