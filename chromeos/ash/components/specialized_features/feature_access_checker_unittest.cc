// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feature_access_checker.h"

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/feature_list_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace specialized_features {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

using enum FeatureAccessFailure;

constexpr std::string_view kSettingsTogglePref = "settings-toggle";
constexpr std::string_view kConsentAcceptedPref = "consent-accepted";

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

TEST(FeatureAccessCheckerTest, AllChecksPass) {
  TestingPrefServiceSimple pref;
  RegisterAndEnableAllPrefs(pref);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(DefaultConfig(), pref).Check()),
      IsEmpty());
}

TEST(FeatureAccessCheckerTest, CheckSettingsPrefCheckFail) {
  TestingPrefServiceSimple pref;
  RegisterAndEnableAllPrefs(pref);
  pref.SetBoolean(kSettingsTogglePref, false);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(DefaultConfig(), pref).Check()),
      ElementsAre(kDisabledInSettings));
}

TEST(FeatureAccessCheckerTest, ConsentAcceptancePrefCheckFail) {
  TestingPrefServiceSimple pref;
  RegisterAndEnableAllPrefs(pref);
  pref.SetBoolean(kConsentAcceptedPref, false);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(DefaultConfig(), pref).Check()),
      ElementsAre(kConsentNotAccepted));
}

TEST(FeatureAccessCheckerTest, FeatureFlagFail) {
  TestingPrefServiceSimple pref;
  RegisterAndEnableAllPrefs(pref);
  FeatureAccessConfig config = DefaultConfig();
  config.feature_flag = kFeatureOffByDefault;

  EXPECT_THAT(base::ToVector(FeatureAccessChecker(config, pref).Check()),
              ElementsAre(kFeatureFlagDisabled));
}

TEST(FeatureAccessCheckerTest, FeatureManagementFlagFail) {
  TestingPrefServiceSimple pref;
  RegisterAndEnableAllPrefs(pref);
  FeatureAccessConfig config = DefaultConfig();
  config.feature_management_flag = kFeatureOffByDefault;

  EXPECT_THAT(base::ToVector(FeatureAccessChecker(config, pref).Check()),
              ElementsAre(kFeatureManagementCheckFailed));
}

}  // namespace
}  // namespace specialized_features
