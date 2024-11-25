// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feature_access_checker.h"

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/feature_list_buildflags.h"
#include "base/hash/sha1.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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

// All feature flags set to pass FeatureAccessChecker::Check()
FeatureAccessConfig DefaultConfig() {
  return {
      .settings_toggle_pref = kSettingsTogglePref,
      .consent_accepted_pref = kConsentAcceptedPref,
      .feature_flag = raw_ref(kFeatureOnByDefault),
      .feature_management_flag = raw_ref(kFeatureOnByDefault),
  };
}

void RegisterAndEnableAllPrefs(TestingPrefServiceSimple& pref) {
  pref.registry()->RegisterBooleanPref(kSettingsTogglePref, true);
  pref.registry()->RegisterBooleanPref(kConsentAcceptedPref, true);
}

class FeatureAccessCheckerTest : public testing::Test {
 public:
  FeatureAccessCheckerTest() {}

  FeatureAccessCheckerTest(const FeatureAccessCheckerTest&) = delete;
  FeatureAccessCheckerTest& operator=(const FeatureAccessCheckerTest&) = delete;

  ~FeatureAccessCheckerTest() override = default;

  void SetUp() override { RegisterAndEnableAllPrefs(pref_); }

  signin::IdentityManager* GetIdentityManager() {
    return identity_test_environment_.identity_manager();
  }

 protected:
  TestingPrefServiceSimple pref_;
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(FeatureAccessCheckerTest, AllPrefAndFeatureChecksPass) {
  EXPECT_THAT(base::ToVector(FeatureAccessChecker(DefaultConfig(), pref_,
                                                  *GetIdentityManager())
                                 .Check()),
              IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, CheckSettingsPrefCheckFail) {
  pref_.SetBoolean(kSettingsTogglePref, false);

  EXPECT_THAT(base::ToVector(FeatureAccessChecker(DefaultConfig(), pref_,
                                                  *GetIdentityManager())
                                 .Check()),
              ElementsAre(kDisabledInSettings));
}

TEST_F(FeatureAccessCheckerTest, ConsentAcceptancePrefCheckFail) {
  pref_.SetBoolean(kConsentAcceptedPref, false);

  EXPECT_THAT(base::ToVector(FeatureAccessChecker(DefaultConfig(), pref_,
                                                  *GetIdentityManager())
                                 .Check()),
              ElementsAre(kConsentNotAccepted));
}

TEST_F(FeatureAccessCheckerTest, FeatureFlagFail) {
  FeatureAccessConfig config = DefaultConfig();
  config.feature_flag = kFeatureOffByDefault;

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      ElementsAre(kFeatureFlagDisabled));
}

TEST_F(FeatureAccessCheckerTest, FeatureManagementFlagFail) {
  FeatureAccessConfig config = DefaultConfig();
  config.feature_management_flag = kFeatureOffByDefault;

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      ElementsAre(kFeatureManagementCheckFailed));
}

TEST_F(FeatureAccessCheckerTest, SecretKeyCheckPass) {
  std::string key_val = "hunter2";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            key_val);
  FeatureAccessConfig config = DefaultConfig();
  std::string hashed = base::SHA1HashString(key_val);
  config.secret_key = {.flag = kSecretKeyFlag, .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      IsEmpty());
}

TEST_F(FeatureAccessCheckerTest, SecretKeyCheckFail) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config = DefaultConfig();
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = kSecretKeyFlag, .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(FeatureAccessCheckerTest,
       SecretKeyCheckFailsIfWrongKeyWithGoogleAccountIfExemptionNotSet) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@google.com", signin::ConsentLevel::kSignin);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config = DefaultConfig();
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = kSecretKeyFlag, .sha1_hashed_key_value = hashed};

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(FeatureAccessCheckerTest,
       SecretKeyCheckFailsIfWrongKeyNonGoogleAccountIfGoogleAccountsExempted) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@gmail.com", signin::ConsentLevel::kSignin);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config = DefaultConfig();
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = kSecretKeyFlag, .sha1_hashed_key_value = hashed};
  config.allow_google_accounts_skip_secret_key = true;

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      ElementsAre(kSecretKeyCheckFailed));
}

TEST_F(
    FeatureAccessCheckerTest,
    SecretKeyCheckPassesIfWrongKeyWithGoogleAccountIfGoogleAccountsExempted) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "someone@google.com", signin::ConsentLevel::kSignin);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kSecretKeyFlag,
                                                            "nothunter2atall");
  FeatureAccessConfig config = DefaultConfig();
  std::string hashed = base::SHA1HashString("hunter2");
  config.secret_key = {.flag = kSecretKeyFlag, .sha1_hashed_key_value = hashed};
  config.allow_google_accounts_skip_secret_key = true;

  EXPECT_THAT(
      base::ToVector(
          FeatureAccessChecker(config, pref_, *GetIdentityManager()).Check()),
      IsEmpty());
}

}  // namespace
}  // namespace specialized_features
