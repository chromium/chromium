// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"

namespace ash::settings {

OSSettingsLockScreenBrowserTestBase::OSSettingsLockScreenBrowserTestBase(
    PasswordType password_type)
    : password_type_(password_type) {
  // We configure FakeUserDataAuthClient (via `cryptohome_`) here and not
  // later because the global PinBackend object reads whether or not
  // cryptohome PINs are supported on startup. If we set up
  // FakeUserDataAuthClient in SetUpOnMainThread, then PinBackend would
  // determine whether PINs are supported before we have configured
  // FakeUserDataAuthClient.
  cryptohome_.set_enable_auth_check(true);
  cryptohome_.set_supports_low_entropy_credentials(true);
  cryptohome_.MarkUserAsExisting(GetAccountId());
  switch (password_type) {
    case PasswordType::kGaia:
      cryptohome_.AddGaiaPassword(GetAccountId(), kPassword);
      break;
    case PasswordType::kLocal:
      cryptohome_.AddLocalPassword(GetAccountId(), kPassword);
      break;
  }
}

OSSettingsLockScreenBrowserTestBase::~OSSettingsLockScreenBrowserTestBase() =
    default;

void OSSettingsLockScreenBrowserTestBase::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  logged_in_user_mixin_.LogInUser();
}

mojom::LockScreenSettingsAsyncWaiter
OSSettingsLockScreenBrowserTestBase::OpenLockScreenSettings() {
  auto os_settings_driver = OpenOSSettings();
  lock_screen_settings_remote_ =
      mojo::Remote(os_settings_driver.GoToLockScreenSettings());
  return mojom::LockScreenSettingsAsyncWaiter(
      lock_screen_settings_remote_.get());
}

mojom::LockScreenSettingsAsyncWaiter
OSSettingsLockScreenBrowserTestBase::OpenLockScreenSettingsAndAuthenticate() {
  OpenLockScreenSettings().Authenticate(kPassword);
  // The mojom AsyncWaiter classes have deleted copy constructors even though
  // they only hold a non-owning pointer to a mojo remote. This restriction
  // should probably be dropped, so that we can just return the async waiter
  // created by the call to `OpenLockScreenSettings` here. As a workaround, we
  // simply create a new waiter.
  return mojom::LockScreenSettingsAsyncWaiter{
      lock_screen_settings_remote_.get()};
}

mojom::LockScreenSettingsAsyncWaiter OSSettingsLockScreenBrowserTestBase::
    OpenLockScreenSettingsDeepLinkAndAuthenticate(
        const std::string& setting_id) {
  std::string relative_url = "/osPrivacy/lockScreen?settingId=";
  relative_url += setting_id;
  auto os_settings_driver = OpenOSSettings(relative_url);

  lock_screen_settings_remote_ =
      mojo::Remote(os_settings_driver.AssertOnLockScreenSettings());
  mojom::LockScreenSettingsAsyncWaiter(lock_screen_settings_remote_.get())
      .Authenticate(kPassword);

  return mojom::LockScreenSettingsAsyncWaiter(
      lock_screen_settings_remote_.get());
}

const AccountId& OSSettingsLockScreenBrowserTestBase::GetAccountId() {
  return logged_in_user_mixin_.GetAccountId();
}

mojom::OSSettingsDriverAsyncWaiter
OSSettingsLockScreenBrowserTestBase::OpenOSSettings(
    const std::string& relative_url) {
  os_settings_driver_remote_ =
      mojo::Remote(os_settings_mixin_.OpenOSSettings(relative_url));
  return mojom::OSSettingsDriverAsyncWaiter(os_settings_driver_remote_.get());
}

}  // namespace ash::settings
