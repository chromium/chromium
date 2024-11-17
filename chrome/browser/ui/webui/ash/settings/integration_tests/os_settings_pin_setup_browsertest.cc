// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/data/webui/chromeos/settings/os_people_page/pin_settings_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_surface_registry.h"
#include "chromeos/ash/components/osauth/public/auth_engine_api.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

namespace {

// PINs to be used for checking PIN verification logic.
const char kFirstPin[] = "111111";
const char kSecondPin[] = "222222";
const char kIncorrectPin[] = "333333";

// PINs to be used for checking minimal/maximum length PINs.
const size_t kMinimumPinLengthForTest = 5;
const size_t kMaximumPinLengthForTest = 10;
const char kMinimumLengthPin[] = "11223";
const char kMaximumLengthPin[] = "1122334455";

// A weak PIN used to verify that a warning is displayed.
const char kWeakPin[] = "111111";

// Twelve digit PINs are allowed for autosubmit, but no more.
const char kMaximumLengthPinForAutosubmit[] = "321321321321";
const char kTooLongPinForAutosubmit[] = "3213213213213";

// Name and value of the metric that records authentication on the lock screen
// page.
const char kPinUnlockUmaHistogramName[] = "Settings.PinUnlockSetup";
const base::HistogramBase::Sample kChoosePinOrPassword = 2;
const base::HistogramBase::Sample kEnterPin = 3;
const base::HistogramBase::Sample kConfirmPin = 4;

}  // namespace

enum class PinType {
  kPrefs,
  kCryptohome,
};

// Tests PIN-related settings in the ChromeOS settings page.
class OSSettingsPinSetupTest : public OSSettingsLockScreenBrowserTestBase,
                               public testing::WithParamInterface<PinType> {
 public:
  OSSettingsPinSetupTest()
      : OSSettingsLockScreenBrowserTestBase(ash::AshAuthFactor::kGaiaPassword),
        pin_type_(GetParam()) {
    switch (pin_type_) {
      case PinType::kPrefs:
        cryptohome_->set_supports_low_entropy_credentials(false);
        break;
      case PinType::kCryptohome:
        cryptohome_->set_supports_low_entropy_credentials(true);
        break;
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    OSSettingsLockScreenBrowserTestBase::SetUpInProcessBrowserTestFixture();

    // Override the policy provider for testing. The `ON_CALL` lines here are
    // necessary because something inside the policy stack expects those return
    // values.
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  PrefService& Prefs() {
    PrefService* service =
        ProfileHelper::Get()->GetProfileByAccountId(GetAccountId())->GetPrefs();
    CHECK(service);
    return *service;
  }

  bool GetPinAutoSubmitState() {
    return Prefs().GetBoolean(::prefs::kPinUnlockAutosubmitEnabled);
  }

  // Returns whether or not a PIN is configured in the backend.
  bool IsPinConfigured() {
    switch (pin_type_) {
      case PinType::kPrefs:
        return !Prefs().GetString(prefs::kQuickUnlockPinSecret).empty() &&
               !Prefs().GetString(prefs::kQuickUnlockPinSalt).empty();
      case PinType::kCryptohome:
        return cryptohome_->HasPinFactor(GetAccountId());
    }
  }

  void SetPinLocked() {
    switch (pin_type_) {
      case PinType::kPrefs: {
        quick_unlock::QuickUnlockStorage* qus =
            quick_unlock::QuickUnlockFactory::GetForAccountId(GetAccountId());
        CHECK(qus);
        quick_unlock::PinStoragePrefs* psp = qus->pin_storage_prefs();
        CHECK(psp);
        // Make sure to add enough unlock attempts so that PIN is locked out.
        for (int i = 0;
             i != quick_unlock::PinStoragePrefs::kMaximumUnlockAttempts; ++i) {
          psp->AddUnlockAttempt();
        }
        CHECK(!psp->IsPinAuthenticationAvailable(quick_unlock::Purpose::kAny));
        break;
      }
      case PinType::kCryptohome: {
        cryptohome_->SetPinLocked(GetAccountId(), true);
        break;
      }
    }
  }

  mojom::PinSettingsApiAsyncWaiter GoToPinSettings(
      mojom::LockScreenSettingsAsyncWaiter& lock_screen_settings) {
    pin_settings_remote_ = mojo::Remote(lock_screen_settings.GoToPinSettings());
    return mojom::PinSettingsApiAsyncWaiter(pin_settings_remote_.get());
  }

  void SetPinDisabledPolicy(bool disabled) {
    policy::PolicyMap policies;
    base::Value policy_value{disabled ? base::Value::List()
                                      : base::Value::List().Append("PIN")};

    policies.Set("QuickUnlockModeAllowlist", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 policy_value.Clone(),
                 /*external_data_fetcher=*/nullptr);

    policies.Set("WebAuthnFactors", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 policy_value.Clone(),
                 /*external_data_fetcher=*/nullptr);

    provider_.UpdateChromePolicy(policies);
  }

 private:
  PinType pin_type_;
  mojo::Remote<mojom::PinSettingsApi> pin_settings_remote_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OSSettingsPinSetupTest,
                         testing::Values(PinType::kPrefs,
                                         PinType::kCryptohome));

// The test fixture for tests that are supposed to be run with the cryptohome
// backend only. We need a separate but essentially identical class here so
// that we can use INSTANTIATE_TEST_SUITE_P with a different set of
// test::Values.
class OSSettingsPinSetupCryptohomeOnlyTest : public OSSettingsPinSetupTest {};

INSTANTIATE_TEST_SUITE_P(All,
                         OSSettingsPinSetupCryptohomeOnlyTest,
                         testing::Values(PinType::kCryptohome));

// Tests that adding a PIN works.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, AddPin) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());

  pin_settings.SetPin(kFirstPin);

  pin_settings.AssertHasPin(true);
  EXPECT_EQ(true, IsPinConfigured());
}

// Tests that changing a PIN works.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, ChangePin) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  pin_settings.SetPin(kFirstPin);
  pin_settings.AssertHasPin(true);
  EXPECT_EQ(true, IsPinConfigured());

  pin_settings.SetPin(kSecondPin);

  pin_settings.AssertHasPin(true);
  EXPECT_EQ(true, IsPinConfigured());
}

// Tests that removing a PIN works.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, RemovePin) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  pin_settings.SetPin(kFirstPin);
  pin_settings.AssertHasPin(true);
  EXPECT_EQ(true, IsPinConfigured());

  pin_settings.RemovePin();

  EXPECT_EQ(false, IsPinConfigured());
  pin_settings.AssertHasPin(false);
}

// Tests that PIN changes are persistent over relaunching os-settings.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, PinPersists) {
  {
    auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
    auto pin_settings = GoToPinSettings(lock_screen_settings);
    pin_settings.SetPin(kFirstPin);

    pin_settings.AssertHasPin(true);
    EXPECT_EQ(true, IsPinConfigured());
  }

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.AssertHasPin(true);
  EXPECT_EQ(true, IsPinConfigured());
}

// Tests that nothing is persisted when cancelling the PIN setup dialog after
// entering the PIN only once.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, SetPinButCancelConfirmation) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());

  pin_settings.SetPinButCancelConfirmation(kFirstPin);

  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());
}

// Tests that nothing is persisted during setup when the PIN that is entered
// the second time for confirmation does not match the first PIN. We should
// record this in UMA though.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, SetPinButFailConfirmation) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());

  pin_settings.SetPinButFailConfirmation(kFirstPin, kIncorrectPin);

  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());
}

// Tests that an error message is displayed when setting PIN fails.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupCryptohomeOnlyTest,
                       AddPinButInternalError) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());

  cryptohome_->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kAddAuthFactor,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          ::user_data_auth::CRYPTOHOME_ADD_CREDENTIALS_FAILED));
  pin_settings.SetPinButInternalError(kFirstPin);

  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());
}

// Tests that PIN setup UI validates minimal pin lengths.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, MinimumPinLength) {
  Prefs().SetInteger(prefs::kPinUnlockMinimumLength, kMinimumPinLengthForTest);

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  // Check that a minimum length PIN is accepted, but that the PIN obtained by
  // removing the last digit is rejected.
  std::string too_short_pin{kMinimumLengthPin};
  too_short_pin.pop_back();

  // SetPinButTooShort checks that a warning is displayed.
  pin_settings.SetPinButTooShort(std::move(too_short_pin), kMinimumLengthPin);
}

// Tests that PIN setup UI validates maximal pin lengths.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, MaximumPinLength) {
  Prefs().SetInteger(prefs::kPinUnlockMaximumLength, kMaximumPinLengthForTest);

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  // Check that a maximum length PIN is accepted, but that the PIN obtained by
  // duplicating the last digit is rejected.
  std::string too_long_pin{kMaximumLengthPin};
  too_long_pin += too_long_pin.back();

  // SetPinButTooLong checks that a warning is displayed.
  pin_settings.SetPinButTooLong(std::move(too_long_pin), kMaximumLengthPin);
}

// Tests that a warning is displayed when setting up a weak PIN, but that it is
// still possible.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, WeakPinWarning) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  // SetPinWithWarning checks that a warning is displayed.
  pin_settings.SetPinWithWarning(kWeakPin);

  pin_settings.AssertHasPin(true);
  EXPECT_EQ(true, IsPinConfigured());
}

// Tests that the PIN setup dialog handles key events appropriately.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, PressKeysInPinSetupDialog) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.CheckPinSetupDialogKeyInput();
}

// Tests that all relevant metrics are recorded when cancelling PIN setup
// immediately.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, SetPinMetricsCancelImmediately) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  base::HistogramTester histograms;

  pin_settings.SetPinButCancelImmediately();

  // The UI doesn't wait for the asynchronous metrics calls to finish, which is
  // why we need the RunLoop here:
  base::RunLoop().RunUntilIdle();
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kChoosePinOrPassword,
                               1);
  histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 1);
}

// Tests that all relevant metrics are recorded when cancelling PIN setup in
// the confirmation step.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest,
                       SetPinMetricsCancelConfirmation) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  base::HistogramTester histograms;

  pin_settings.SetPinButCancelConfirmation(kFirstPin);

  // The UI doesn't wait for the asynchronous metrics calls to finish, which is
  // why we need the RunLoop here:
  base::RunLoop().RunUntilIdle();
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kChoosePinOrPassword,
                               1);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kEnterPin, 1);
  histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 2);
}

// Tests that all relevant metrics are recorded when adding a PIN.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, SetPinMetricsFull) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  base::HistogramTester histograms;

  pin_settings.SetPin(kFirstPin);

  // The UI doesn't wait for the asynchronous metrics calls to finish, which is
  // why we need the RunLoop here:
  base::RunLoop().RunUntilIdle();
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kChoosePinOrPassword,
                               1);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kEnterPin, 1);
  histograms.ExpectBucketCount(kPinUnlockUmaHistogramName, kConfirmPin, 1);
  histograms.ExpectTotalCount(kPinUnlockUmaHistogramName, 3);
}

// Tests that the PIN control is disabled when PIN is disabled by policy.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, PinDisabledByPolicy) {
  SetPinDisabledPolicy(true);

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.AssertDisabled(true);
}

// Tests that the PIN control is enabled when PIN is allowed policy.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, PinNotDisabledByPolicy) {
  SetPinDisabledPolicy(false);

  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.AssertDisabled(false);
}

// Tests that the PIN control gets disabled when the policy disabling PIN is
// activated while the settings page is opened.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, PinDisabledPolicyWhileOpen) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);
  pin_settings.AssertDisabled(false);

  SetPinDisabledPolicy(true);

  pin_settings.AssertDisabled(true);
}

// Tests enabling and disabling autosubmit.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, Autosubmit) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  // Set a pin. Autosubmit should be enabled.
  pin_settings.SetPin(kFirstPin);
  pin_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  // Change, remove and add pin again. Nothing of this should affect the pin
  // autosubmit pref.
  pin_settings.SetPin(kSecondPin);
  pin_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  pin_settings.RemovePin();
  pin_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  pin_settings.SetPin(kSecondPin);
  pin_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  // Disable pin autosubmit. This should turn the pref off, but the pin should
  // still be active.
  pin_settings.DisablePinAutosubmit();
  pin_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());
  EXPECT_EQ(true, IsPinConfigured());

  // Try to enable pin autosubmit using the wrong pin. This should not succeed.
  pin_settings.EnablePinAutosubmitIncorrectly(kIncorrectPin);
  pin_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());

  // Try to enable pin autosubmit using the correct pin. This should succeed.
  pin_settings.EnablePinAutosubmit(kSecondPin);
  pin_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());

  // Even after we have authenticated with the correct pin, we should be able
  // to remove the pin.
  pin_settings.RemovePin();
  pin_settings.AssertHasPin(false);
  EXPECT_EQ(false, IsPinConfigured());
}

// Tests the maximum length of PINs for autosubmit.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, MaximumLengthAutosubmit) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  // Set a maximum length pin. Autosubmit should be enabled.
  pin_settings.SetPin(kMaximumLengthPinForAutosubmit);
  pin_settings.AssertPinAutosubmitEnabled(true);
  EXPECT_EQ(true, GetPinAutoSubmitState());
  // Remove the PIN again.
  pin_settings.RemovePin();

  // Set an overly long PIN. Autosubmit should be disabled, and we shouldn't be
  // able to turn it on.
  pin_settings.SetPin(kTooLongPinForAutosubmit);
  pin_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());

  pin_settings.EnablePinAutosubmitTooLong(kTooLongPinForAutosubmit);
  pin_settings.AssertPinAutosubmitEnabled(false);
  EXPECT_EQ(false, GetPinAutoSubmitState());
}

// Tests that the user is asked to reauthenticate when trying to enable PIN
// autosubmit but with a locked-out PIN.
IN_PROC_BROWSER_TEST_P(OSSettingsPinSetupTest, AutosubmitWithLockedPin) {
  auto go_to_lock_screen_settings_and_authenticate = [&]() {
    if (ash::features::IsUseAuthPanelInSessionEnabled()) {
      OpenLockScreenSettings();
      Authenticate();
      return mojom::LockScreenSettingsAsyncWaiter{
          lock_screen_settings_remote_.get()};
    } else {
      return OpenLockScreenSettingsAndAuthenticate();
    }
  };

  auto lock_screen_settings = go_to_lock_screen_settings_and_authenticate();

  auto pin_settings = GoToPinSettings(lock_screen_settings);
  pin_settings.SetPin(kFirstPin);
  // We disable autosubmit so that we can try to reenable.
  pin_settings.DisablePinAutosubmit();
  SetPinLocked();

  pin_settings.TryEnablePinAutosubmit(kFirstPin);

  if (ash::features::IsUseAuthPanelInSessionEnabled()) {
    base::test::TestFuture<AuthSurfaceRegistry::AuthSurface> future;
    auto subscription =
        ash::AuthParts::Get()->GetAuthSurfaceRegistry()->RegisterShownCallback(
            future.GetCallback());

    auto surface = future.Get();
    ASSERT_EQ(surface, AuthSurfaceRegistry::AuthSurface::kInSession);

    base::RunLoop().RunUntilIdle();

    Authenticate();

    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(false, GetPinAutoSubmitState());
    pin_settings.AssertPinAutosubmitEnabled(false);
  } else {
    lock_screen_settings.AssertAuthenticated(false);

    lock_screen_settings.Authenticate(
        OSSettingsLockScreenBrowserTestBase::kPassword);
    EXPECT_EQ(false, GetPinAutoSubmitState());
    pin_settings.AssertPinAutosubmitEnabled(false);
  }
}

// Tests PIN-only related settings in the ChromeOS settings page.
class OSSettingsPinOnlySetupTest : public OSSettingsLockScreenBrowserTestBase {
 public:
  OSSettingsPinOnlySetupTest()
      : OSSettingsLockScreenBrowserTestBase(
            ash::AshAuthFactor::kCryptohomePin) {
    cryptohome_->set_supports_low_entropy_credentials(true);
  }

  mojom::PinSettingsApiAsyncWaiter GoToPinSettings(
      mojom::LockScreenSettingsAsyncWaiter& lock_screen_settings) {
    pin_settings_remote_ = mojo::Remote(lock_screen_settings.GoToPinSettings());
    return mojom::PinSettingsApiAsyncWaiter(pin_settings_remote_.get());
  }

 private:
  mojo::Remote<mojom::PinSettingsApi> pin_settings_remote_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

// Tests that the PIN control is disabled when PIN is the only factor.
IN_PROC_BROWSER_TEST_F(OSSettingsPinOnlySetupTest, PinOnlyRemobalDisabled) {
  auto lock_screen_settings = OpenLockScreenSettingsAndAuthenticate();
  auto pin_settings = GoToPinSettings(lock_screen_settings);

  pin_settings.AssertMoreButtonDisabled(true);
}

}  // namespace ash::settings
