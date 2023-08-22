// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_OS_SETTINGS_LOCK_SCREEN_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_OS_SETTINGS_LOCK_SCREEN_BROWSER_TEST_BASE_H_

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_browser_test_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/data/webui/settings/chromeos/test_api.test-mojom-test-utils.h"
#include "components/account_id/account_id.h"

namespace ash::settings {

// Fixture for browser tests of the "lock screen" section in
// chrome://os-settings. The fixture creates a user with a gaia password and
// logs in. It provides methods to open the os settings page and navigate to
// the "lock screen" section. Tests can then operate this section via its test
// api.
class OSSettingsLockScreenBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  enum class PasswordType {
    kGaia,
    kLocal,
  };
  // The password of the user that is set up by this fixture.
  static constexpr char kPassword[] = "the-password";

  explicit OSSettingsLockScreenBrowserTestBase(
      PasswordType = PasswordType::kGaia);
  ~OSSettingsLockScreenBrowserTestBase() override;

  void SetUpOnMainThread() override;

  // Opens the ChromeOS settings app and navigates to the "lock screen"
  // section. The return value is an AsyncWaiter that can be used to interact
  // with the WebUI. The returned AsyncWaiter is valid until this function is
  // called again.
  mojom::LockScreenSettingsAsyncWaiter OpenLockScreenSettings();

  // Calls `OpenLockScreenSettings` and authenticates.
  mojom::LockScreenSettingsAsyncWaiter OpenLockScreenSettingsAndAuthenticate();

  // Opens the ChromeOS settings app with a deep link to an item on the "lock
  // screen" section and authenticates.
  mojom::LockScreenSettingsAsyncWaiter
  OpenLockScreenSettingsDeepLinkAndAuthenticate(const std::string& setting_id);

  // The account ID of the user set up by this fixture.
  const AccountId& GetAccountId();

 protected:
  PasswordType password_type_;
  CryptohomeMixin cryptohome_{&mixin_host_};
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(), this};
  OSSettingsBrowserTestMixin os_settings_mixin_{&mixin_host_};

 private:
  // Opens the os settings page and saves the test api remote in
  // `os_settings_driver_remote_`. Returns an async waiter to the remote.
  mojom::OSSettingsDriverAsyncWaiter OpenOSSettings(
      const std::string& relative_url = "");

  mojo::Remote<mojom::OSSettingsDriver> os_settings_driver_remote_;
  mojo::Remote<mojom::LockScreenSettings> lock_screen_settings_remote_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_OS_SETTINGS_LOCK_SCREEN_BROWSER_TEST_BASE_H_
