// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feature_access_checker.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/feature_list_buildflags.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/test/task_environment.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/variations_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace specialized_features {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

using enum FeatureAccessFailure;

constexpr std::string_view kSettingsTogglePref = "settings-toggle";
constexpr std::string_view kConsentAcceptedPref = "consent-accepted";
constexpr std::string_view kSecretKeyFlag = "secret-key-flag";

BASE_FEATURE(kFeatureOnByDefault,
             "OnByDefaultName",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeatureOffByDefault,
             "OffByDefaultName",
             base::FEATURE_DISABLED_BY_DEFAULT);

void RegisterAndEnableAllPrefs(TestingPrefServiceSimple& pref) {
  pref.registry()->RegisterBooleanPref(kSettingsTogglePref, true);
  pref.registry()->RegisterBooleanPref(kConsentAcceptedPref, true);
}

class FeatureAccessCheckerTest : public testing::Test {
 public:
  FeatureAccessCheckerTest()
      : metrics_enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {
    // Prefs need to be registered before creating a metrics_state_manager and
    // variations_service, as the ctors will try to access a few prefs.
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, &metrics_enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());
  }

  FeatureAccessCheckerTest(const FeatureAccessCheckerTest&) = delete;
  FeatureAccessCheckerTest& operator=(const FeatureAccessCheckerTest&) = delete;

  ~FeatureAccessCheckerTest() override = default;

  void SetUp() override { RegisterAndEnableAllPrefs(pref_); }

  signin::IdentityManager* GetIdentityManager() {
    return identity_test_environment_.identity_manager();
  }

  FeatureAccessChecker::VariationsServiceCallback
  GetVariationsServiceCallback() {
    return base::BindRepeating(
        [](variations::VariationsService* service) { return service; },
        variations_service_.get());
  }

 protected:
  TestingPrefServiceSimple pref_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  TestingPrefServiceSimple pref_service_;
  // These metrics objects are just required to set up the variations_service.
  // They are not used directly in any test.
  metrics::TestEnabledStateProvider metrics_enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

TEST_F(FeatureAccessCheckerTest, AllPrefAndFeatureChecksPassIfUnset) {
  EXPECT_THAT(base::ToVector(FeatureAccessChecker(
                                 /* config= */ {}, &pref_, GetIdentityManager(),
                                 GetVariationsServiceCallback())
                                 .Check()),
              IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, CheckSettingsPrefCheckPass) {
  FeatureAccessConfig config;
  config.settings_toggle_pref = kSettingsTogglePref;
  pref_.SetBoolean(kSettingsTogglePref, true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, CheckSettingsPrefCheckFail) {
  FeatureAccessConfig config;
  config.settings_toggle_pref = kSettingsTogglePref;
  pref_.SetBoolean(kSettingsTogglePref, false);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kDisabledInSettings));
}

TEST_F(FeatureAccessCheckerTest, CheckSettingsPrefCheckFailIfNoPrefService) {
  FeatureAccessConfig config;
  config.settings_toggle_pref = kSettingsTogglePref;
  pref_.SetBoolean(kSettingsTogglePref, true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, /*prefs=*/nullptr,
                                          GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kDisabledInSettings));
}

TEST_F(FeatureAccessCheckerTest, ConsentAcceptancePrefCheckPass) {
  FeatureAccessConfig config;
  config.consent_accepted_pref = kConsentAcceptedPref;
  pref_.SetBoolean(kConsentAcceptedPref, true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, ConsentAcceptancePrefCheckFail) {
  FeatureAccessConfig config;
  config.consent_accepted_pref = kConsentAcceptedPref;
  pref_.SetBoolean(kConsentAcceptedPref, false);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kConsentNotAccepted));
}

TEST_F(FeatureAccessCheckerTest,
       ConsentAcceptancePrefCheckFailIfNoPrefService) {
  FeatureAccessConfig config;
  config.consent_accepted_pref = kConsentAcceptedPref;
  pref_.SetBoolean(kConsentAcceptedPref, true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, /*prefs=*/nullptr,
                                          GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kConsentNotAccepted));
}

TEST_F(FeatureAccessCheckerTest, FeatureFlagPass) {
  FeatureAccessConfig config;
  config.feature_flag = &kFeatureOnByDefault;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, FeatureFlagFail) {
  FeatureAccessConfig config;
  config.feature_flag = &kFeatureOffByDefault;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kFeatureFlagDisabled));
}

TEST_F(FeatureAccessCheckerTest, FeatureManagementFlagPass) {
  FeatureAccessConfig config;
  config.feature_management_flag = &kFeatureOnByDefault;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, FeatureManagementFlagFail) {
  FeatureAccessConfig config;
  config.feature_management_flag = &kFeatureOffByDefault;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kFeatureManagementCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, SecretKeyCheckPass) {
  std::string key_val = "hunter2";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            key_val);
  FeatureAccessConfig config;
  std::string hashed = base::SHA1HashString(key_val);
  config.secret_key = {.flag = std::string(kSecretKeyFlag),
                       .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, SecretKeyCheckFail) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config;
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = std::string(kSecretKeyFlag),
                       .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, SecretKeyCheckFailIfNoIdentityManager) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config;
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = std::string(kSecretKeyFlag),
                       .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_,
                                          /*identity_manager=*/nullptr,
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(FeatureAccessCheckerTest,
       SecretKeyCheckFailsIfWrongKeyWithGoogleAccountIfExemptionNotSet) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@google.com", signin::ConsentLevel::kSignin);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config;
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = std::string(kSecretKeyFlag),
                       .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(FeatureAccessCheckerTest,
       SecretKeyCheckFailsIfWrongKeyNonGoogleAccountIfGoogleAccountsExempted) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@gmail.com", signin::ConsentLevel::kSignin);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config;
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = std::string(kSecretKeyFlag),
                       .sha1_hashed_key_value = hashed};
  config.allow_google_accounts_skip_secret_key = true;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(
    FeatureAccessCheckerTest,
    SecretKeyCheckPassesIfWrongKeyWithGoogleAccountIfGoogleAccountsExempted) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@google.com", signin::ConsentLevel::kSignin);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config;
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = std::string(kSecretKeyFlag),
                       .sha1_hashed_key_value = hashed};
  config.allow_google_accounts_skip_secret_key = true;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, MantaAccountCapabilitiesCheckPass) {
  AccountInfo account = identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_can_use_manta_service(true);
  signin::UpdateAccountInfoForAccount(
      identity_test_environment_.identity_manager(), account);
  FeatureAccessConfig config;
  config.capability_callback =
      base::BindRepeating([](AccountCapabilities capabilities) {
        return capabilities.can_use_manta_service();
      });

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, MantaAccountCapabilitiesCheckFailIfFalse) {
  AccountInfo account = identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_can_use_manta_service(false);
  FeatureAccessConfig config;
  config.capability_callback =
      base::BindRepeating([](AccountCapabilities capabilities) {
        return capabilities.can_use_manta_service();
      });

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kAccountCapabilitiesCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, MantaAccountCapabilitiesCheckFailIfUnset) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@gmail.com", signin::ConsentLevel::kSignin);
  FeatureAccessConfig config;
  config.capability_callback =
      base::BindRepeating([](AccountCapabilities capabilities) {
        return capabilities.can_use_manta_service();
      });

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kAccountCapabilitiesCheckFailed));
}

TEST_F(FeatureAccessCheckerTest,
       MantaAccountCapabilitiesCheckFailIfNoIdentityService) {
  AccountInfo account = identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@gmail.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_can_use_manta_service(true);
  signin::UpdateAccountInfoForAccount(
      identity_test_environment_.identity_manager(), account);
  FeatureAccessConfig config;
  config.capability_callback =
      base::BindRepeating([](AccountCapabilities capabilities) {
        return capabilities.can_use_manta_service();
      });

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_,
                                          /*identity_manager=*/nullptr,
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kAccountCapabilitiesCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, CountryCodeCheckPassIfNothingInList) {
  FeatureAccessConfig config;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "fr");

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, CountryCodeCheckPassIfExactMatch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");
  FeatureAccessConfig config;
  config.country_codes = {"us"};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, CountryCodeCheckPassOneOfMany) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "jp");
  FeatureAccessConfig config;
  config.country_codes = {"us", "fr", "jp"};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, CountryCodeCheckFailCountryNotInList) {
  FeatureAccessConfig config;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "fr");
  config.country_codes = {"us"};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kCountryCheckFailed));
}

TEST_F(FeatureAccessCheckerTest,
       CountryCodeCheckFailNoVariationsServiceCallback) {
  FeatureAccessConfig config;
  config.country_codes = {"us"};

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(
                         config, &pref_, GetIdentityManager(),
                         /*variations_service_callback=*/base::NullCallback())
                         .Check()),
      ElementsAre(kCountryCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, CountryCodeCheckFailNoVariationsService) {
  FeatureAccessConfig config;
  config.country_codes = {"us"};

  EXPECT_THAT(base::ToVector(FeatureAccessChecker(
                                 config, &pref_, GetIdentityManager(),
                                 base::BindRepeating(
                                     []() -> variations::VariationsService* {
                                       return nullptr;
                                     }))
                                 .Check()),
              ElementsAre(kCountryCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, KioskModeCheckPassIfNotInKioskMode) {
  FeatureAccessConfig config;
  config.disabled_in_kiosk_mode = true;

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, KioskModeCheckFailIfInKioskMode) {
  FeatureAccessConfig config;
  config.disabled_in_kiosk_mode = true;
  user_manager::UserManager::RegisterPrefs(pref_service_.registry());
  user_manager::ScopedUserManager scoped_user_manager(
      std::make_unique<user_manager::FakeUserManager>(&pref_service_));

  chromeos::SetUpFakeKioskSession();

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(config, &pref_, GetIdentityManager(),
                                          GetVariationsServiceCallback())
                         .Check()),
      ElementsAre(kDisabledInKioskModeCheckFailed));
}

}  // namespace
}  // namespace specialized_features
