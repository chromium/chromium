// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"

#include "ash/constants/ash_features.h"

#include "chrome/browser/ui/webui/settings/ash/os_settings_browser_test_mixin.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"

namespace {

const char kCorrectPassword[] = "correct-password";
const char kIncorrectPassword[] = "incorrect-password";

struct Params {
  const bool use_auth_session;
};

}  // namespace

namespace ash::settings {

// Test of the authentication dialog in the lock screen page in os-settings.
class OSSettingsLockScreenAuthenticationTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<Params> {
 public:
  OSSettingsLockScreenAuthenticationTest() {
    if (GetParam().use_auth_session) {
      feature_list_.InitWithFeatures({ash::features::kUseAuthFactors}, {});
      CHECK(ash::features::IsUseAuthFactorsEnabled());
    } else {
      feature_list_.InitWithFeatures({}, {ash::features::kUseAuthFactors});
      CHECK(!ash::features::IsUseAuthFactorsEnabled());
    }
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);

    auto account = AccountId::FromUserEmail(user_manager::kStubUserEmail);
    cryptohome_.MarkUserAsExisting(account);
    cryptohome_.AddGaiaPassword(account, kCorrectPassword);
  }

 protected:
  CryptohomeMixin cryptohome_{&mixin_host_};
  OSSettingsBrowserTestMixin os_settings_{&mixin_host_};

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(OSSettingsLockScreenAuthenticationTest,
                       SuccessfulUnlock) {
  auto lock_screen_settings = os_settings_.GoToLockScreenSettings();
  lock_screen_settings.AssertAuthenticated(false);
  lock_screen_settings.Authenticate(kCorrectPassword);
  lock_screen_settings.AssertAuthenticated(true);
}

IN_PROC_BROWSER_TEST_P(OSSettingsLockScreenAuthenticationTest, FailedUnlock) {
  auto lock_screen_settings = os_settings_.GoToLockScreenSettings();
  lock_screen_settings.AssertAuthenticated(false);
  lock_screen_settings.AuthenticateIncorrectly(kIncorrectPassword);
  lock_screen_settings.AssertAuthenticated(false);
  lock_screen_settings.Authenticate(kCorrectPassword);
  lock_screen_settings.AssertAuthenticated(true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OSSettingsLockScreenAuthenticationTest,
                         testing::Values(Params{.use_auth_session = false},
                                         Params{.use_auth_session = true}));

}  // namespace ash::settings
