// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_browser_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

namespace {

const char kPassword[] = "the-password";
const char kFirstPin[] = "111111";
const char kSecondPin[] = "22222222";
const char kIncorrectPin[] = "333333333";

}  // namespace

// Tests PIN-related settings in the ChromeOS settings page.
class OSSettingsPinSetupTest : public MixinBasedInProcessBrowserTest {
 public:
  OSSettingsPinSetupTest() {
    // We configure FakeUserDataAuthClient here and not later because the
    // global PinBackend object reads whether or not cryptohome PINs are
    // supported on startup. If we set up FakeUserDataAuthClient in
    // SetUpOnMainThread, then PinBackend would determine whether PINs are
    // supported before we have configured FakeUserDataAuthClient.
    UserDataAuthClient::InitializeFake();
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
  // enters the password.
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

  bool GetPinAutoSubmitState() {
    PrefService* service =
        ProfileHelper::Get()->GetProfileByAccountId(GetAccountId())->GetPrefs();
    CHECK(service);
    return service->GetBoolean(::prefs::kPinUnlockAutosubmitEnabled);
  }

  const AccountId& GetAccountId() {
    return logged_in_user_mixin_.GetAccountId();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  CryptohomeMixin cryptohome_{&mixin_host_};
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(), this};
  OSSettingsBrowserTestMixin os_settings_mixin_{&mixin_host_};

  mojo::Remote<mojom::OSSettingsDriver> os_settings_driver_remote_;
  mojo::Remote<mojom::LockScreenSettings> lock_screen_settings_remote_;
};

// Tests that the happy path of setting and removing PINs works.
IN_PROC_BROWSER_TEST_F(OSSettingsPinSetupTest, SetRemove) {
  auto lock_screen_settings = OpenLockScreenSettings();
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
  auto lock_screen_settings = OpenLockScreenSettings();

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
