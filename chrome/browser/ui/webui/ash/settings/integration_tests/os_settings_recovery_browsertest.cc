// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/test/browser_test.h"

namespace {

const char kRecoveryFactorBehaviorPolicy[] = "RecoveryFactorBehavior";

}

namespace ash::settings {

class OSSettingsRecoveryTest : public OSSettingsLockScreenBrowserTestBase {
 public:
  OSSettingsRecoveryTest()
      : OSSettingsLockScreenBrowserTestBase(ash::AshAuthFactor::kGaiaPassword) {
  }
};

// A test fixture that runs tests with recovery feature enabled but without
// hardware support.
class OSSettingsRecoveryTestWithoutHardwareSupport
    : public OSSettingsRecoveryTest {
 public:
  OSSettingsRecoveryTestWithoutHardwareSupport() {
    cryptohome_->set_supports_low_entropy_credentials(false);
  }
};

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithoutHardwareSupport,
                       ControlInvisibleNotAvailable) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryControlVisibility(true);
  lock_screen_settings.AssertRecoveryControlAvailability(false);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTest, ControlVisible) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryControlVisibility(true);
  lock_screen_settings.AssertRecoveryControlAvailability(true);
}

IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTest, CheckingEnables) {
  EXPECT_FALSE(cryptohome_->HasRecoveryFactor(GetAccountId()));

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(false);
  lock_screen_settings.EnableRecoveryConfiguration();
  lock_screen_settings.AssertRecoveryConfigured(true);

  EXPECT_TRUE(cryptohome_->HasRecoveryFactor(GetAccountId()));
}

// The following test sets the cryptohome recovery toggle to "on".
// It clicks on the recovery toggle, expecting the recovery dialog to show up.
// It then clicks on the cancel button of the dialog.
// Expected result: The dialog disappears and the toggle is still on.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTest,
                       UncheckingDisablesAndCancelClick) {
  cryptohome_->AddRecoveryFactor(GetAccountId());

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(true);
  lock_screen_settings.DisableRecoveryConfiguration(
      mojom::LockScreenSettings::RecoveryDialogAction::CancelDialog);
  lock_screen_settings.AssertRecoveryConfigured(true);
  // After the CancelClick on the dialog, the recovery configuration
  // should remain enabled.
  EXPECT_TRUE(cryptohome_->HasRecoveryFactor(GetAccountId()));
}

// The following test sets the cryptohome recovery toggle to "on".
// It clicks on the recovery toggle, expecting the recovery dialog to show up.
// It then clicks on the disable button of the dialog.
// Expected result: The dialog disappears and the toggle is off.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTest,
                       UncheckingDisablesAndDisableClick) {
  cryptohome_->AddRecoveryFactor(GetAccountId());

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(true);
  lock_screen_settings.DisableRecoveryConfiguration(
      mojom::LockScreenSettings::RecoveryDialogAction::ConfirmDisabling);
  lock_screen_settings.AssertRecoveryConfigured(false);

  EXPECT_FALSE(cryptohome_->HasRecoveryFactor(GetAccountId()));
}

// Check that the kDataRecovery deep link id can navigate to
// the recovery toggle.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTest, NavigaionToRecoveryToggle) {
  cryptohome_->AddRecoveryFactor(GetAccountId());
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsDeepLinkAndAuthenticate(base::NumberToString(
          static_cast<int>(chromeos::settings::mojom::Setting::kDataRecovery)));
  lock_screen_settings.AssertRecoveryControlFocused();
}

// Check that trying to change recovery with an invalidated auth session shows
// the password prompt again.
// TODO(crbug.com/40906200): Re-enable this test
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTest, DISABLED_DestroyedSession) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();

  // Try to change recovery setting, but with an invalid auth session. This
  // should throw us back to the password prompt.
  cryptohome_->DestroySessions();
  lock_screen_settings.TryEnableRecoveryConfiguration();
  lock_screen_settings.AssertAuthenticated(false);

  // Check that it's still possible to authenticate and change recovery
  // settings.
  EXPECT_FALSE(cryptohome_->HasRecoveryFactor(GetAccountId()));
  lock_screen_settings.Authenticate(kPassword);
  lock_screen_settings.EnableRecoveryConfiguration();
  EXPECT_TRUE(cryptohome_->HasRecoveryFactor(GetAccountId()));
}

struct CryptohomeRecoveryPolicySetting {
  bool value;
  bool is_recommendation;
};

class OSSettingsRecoveryTestWithPolicy : public OSSettingsRecoveryTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    OSSettingsRecoveryTest::SetUpInProcessBrowserTestFixture();

    // Override and policy provider for testing. The `ON_CALL` lines here are
    // necessary because something inside the policy stack expects those return
    // values.
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetCryptohomeRecoveryPolicy(policy::PolicyLevel level, bool value) {
    policy::PolicyMap policies;
    policies.Set(kRecoveryFactorBehaviorPolicy, level,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(value),
                 /*external_data_fetcher=*/nullptr);
    provider_.UpdateChromePolicy(policies);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

// Check that the recovery toggle cannot be flipped to "on" if a policy
// mandates that cryptohome recovery is disabled.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithPolicy, DisabledMandatory) {
  SetCryptohomeRecoveryPolicy(policy::POLICY_LEVEL_MANDATORY, false);

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.TryEnableRecoveryConfiguration();

  // This will repeateadly check that the recovery toggle is set to "off" for
  // some time, so we would catch if the toggle flips asynchronously after some
  // time.
  lock_screen_settings.AssertRecoveryConfigured(false);
  EXPECT_FALSE(cryptohome_->HasRecoveryFactor(GetAccountId()));
}

// Check that the recovery toggle can be flipped if a policy mandates that
// recovery is enabled but recovery is currently not enabled.
IN_PROC_BROWSER_TEST_F(OSSettingsRecoveryTestWithPolicy,
                       EnabledMandatoryButDisabled) {
  SetCryptohomeRecoveryPolicy(policy::POLICY_LEVEL_MANDATORY, true);
  EXPECT_FALSE(cryptohome_->HasRecoveryFactor(GetAccountId()));

  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertRecoveryConfigured(false);

  lock_screen_settings.EnableRecoveryConfiguration();

  lock_screen_settings.AssertRecoveryConfigured(true);
  EXPECT_TRUE(cryptohome_->HasRecoveryFactor(GetAccountId()));

  // Try to disable again -- this should have no effect, because the policy
  // mandates that recovery must be configured.
  lock_screen_settings.TryDisableRecoveryConfiguration();

  // This will repeateadly check that the recovery toggle is set to "on" for
  // some time, so we would catch if the toggle flips asynchronously after some
  // time.
  lock_screen_settings.AssertRecoveryConfigured(true);
  EXPECT_TRUE(cryptohome_->HasRecoveryFactor(GetAccountId()));
}

}  // namespace ash::settings
