// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/chromeos/settings/os_people_page/password_settings_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

class OSSettingsAuthFactorSetupTest
    : public OSSettingsLockScreenBrowserTestBase {
  using OSSettingsLockScreenBrowserTestBase::
      OSSettingsLockScreenBrowserTestBase;

 public:
  explicit OSSettingsAuthFactorSetupTest(
      ash::AshAuthFactor type,
      LoggedInUserMixin::LogInType login_type =
          LoggedInUserMixin::LogInType::kConsumer)
      : OSSettingsLockScreenBrowserTestBase(type, login_type) {}
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
    : public OSSettingsAuthFactorSetupTest {
 public:
  OSSettingsAuthFactorSetupTestWithGaiaPassword()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kGaiaPassword) {}
};

// If user has Gaia password, the control for changing passwords is shown.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithGaiaPassword,
                       Visibility) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);
  password_settings.AssertCanRemovePassword(false);
  password_settings.AssertCanSwitchToLocalPassword(true);
  password_settings.AssertCanOpenLocalPasswordDialog();
  password_settings.AssertSubmitButtonDisabledForInvalidPasswordInput();
  password_settings.AssertSubmitButtonEnabledForValidPasswordInput();
}

class OSSettingsAuthFactorSetupTestWithLocalPassword
    : public OSSettingsAuthFactorSetupTest {
 public:
  OSSettingsAuthFactorSetupTestWithLocalPassword()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kLocalPassword) {}
};

// The control for changing passwords is shown if user has local password.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithLocalPassword, Shown) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);
  password_settings.AssertCanRemovePassword(false);
  password_settings.AssertCanSwitchToLocalPassword(false);
}

class OSSettingsAuthFactorSetupTestWithManagedUser
    : public OSSettingsAuthFactorSetupTest {
 public:
  OSSettingsAuthFactorSetupTestWithManagedUser()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kGaiaPassword,
                                      LoggedInUserMixin::LogInType::kManaged) {
    cryptohome_->set_enable_auth_check(false);
  }
};

// Test that password controls are not shown for managed users.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithManagedUser,
                       PasswordSettingsNotShown) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(false);
}

class OSSettingsAuthFactorSetupTestWithPinOnly
    : public OSSettingsAuthFactorSetupTest {
 public:
  OSSettingsAuthFactorSetupTestWithPinOnly()
      : OSSettingsAuthFactorSetupTest(ash::AshAuthFactor::kCryptohomePin) {
    cryptohome_->set_supports_low_entropy_credentials(true);
    cryptohome_->set_enable_auth_check(false);
  }
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

// Tests that setting a password works.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithPinOnly, SetPassword) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);
  password_settings.AssertCanOpenLocalPasswordDialog();
  password_settings.SetPassword();
  password_settings.AssertHasPassword(true);
}

// Tests that removing a password works.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithPinOnly,
                       RemovePassword) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);
  password_settings.SetPassword();

  password_settings.AssertHasPassword(true);
  password_settings.AssertCanRemovePassword(true);

  password_settings.RemovePassword();
  password_settings.AssertHasPassword(false);
}

// Tests that removing a password does not work when the pin is legacy pin.
IN_PROC_BROWSER_TEST_F(OSSettingsAuthFactorSetupTestWithPinOnly,
                       RemovePasswordWithLegacyPin) {
  cryptohome_->SetPinType(GetAccountId(), true);
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(true);
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);
  password_settings.SetPassword();
  password_settings.AssertHasPassword(true);
  password_settings.AssertCanRemovePassword(false);
}

}  // namespace ash::settings
