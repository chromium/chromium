// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/specialized_features/feature_access_checker.h"

#include "base/containers/to_vector.h"
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

FeatureAccessConfig DefaultConfig() {
  return {
      .settings_toggle_pref = kSettingsTogglePref,
      .consent_accepted_pref = kConsentAcceptedPref,
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

}  // namespace
}  // namespace specialized_features
