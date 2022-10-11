// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_settings_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/ui/webui/settings/ash/os_settings_browser_test_mixin.h"

#include "content/public/common/content_client.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-test-utils.h"
#include "components/user_manager/user_names.h"

namespace {

const char kPassword[] = "asdf";
const char kAuthToken[] = "123";

}  // namespace

namespace ash::settings {

class OSSettingsRecoveryTest : public MixinBasedInProcessBrowserTest {
 public:
  OSSettingsRecoveryTest() = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    const auto account = AccountId::FromUserEmail(user_manager::kStubUserEmail);
    cryptohome_.MarkUserAsExisting(account);
    cryptohome_.AddGaiaPassword(account, kPassword);
  }

 protected:
  CryptohomeMixin cryptohome_{&mixin_host_};
  OSSettingsBrowserTestMixin os_settings_{&mixin_host_};
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

class OSSettingsRecoveryTestWithFeature : public OSSettingsRecoveryTest {
 public:
  OSSettingsRecoveryTestWithFeature() {
    feature_list_.InitAndEnableFeature(ash::features::kCryptohomeRecoverySetup);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithoutFeature,
                       ControlNotVisible) {
  auto lock_screen_settings = os_settings_.GoToLockScreenSettings();
  lock_screen_settings.Authenticate(kPassword);
  lock_screen_settings.AssertRecoveryControlVisibility(false);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, ControlVisible) {
  auto lock_screen_settings = os_settings_.GoToLockScreenSettings();
  lock_screen_settings.Authenticate(kPassword);
  lock_screen_settings.AssertRecoveryControlVisibility(true);
}

// TODO(b/239416325): This should eventually check state in fake user data
// auth, not in the auth factor config mojo service.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithFeature, CheckingEnables) {
  auto auth_factor_config = auth::GetAuthFactorConfigForTesting();
  auto recovery_editor = auth::GetRecoveryFactorEditorForTesting();

  ASSERT_EQ(auth::mojom::RecoveryFactorEditor::ConfigureResult::kSuccess,
            recovery_editor.Configure(kAuthToken, false));

  auto lock_screen_settings = os_settings_.GoToLockScreenSettings();
  lock_screen_settings.Authenticate(kPassword);
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

  auto lock_screen_settings = os_settings_.GoToLockScreenSettings();
  lock_screen_settings.Authenticate(kPassword);
  lock_screen_settings.AssertRecoveryConfigured(true);
  lock_screen_settings.ToggleRecoveryConfiguration();

  EXPECT_FALSE(auth_factor_config.IsConfigured(
      kAuthToken, auth::mojom::AuthFactor::kRecovery));
}

}  // namespace ash::settings
