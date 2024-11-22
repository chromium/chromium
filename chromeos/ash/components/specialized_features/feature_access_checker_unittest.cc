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

TEST(FeatureAccessCheckerTest, CheckSettingsPrefCheckPass) {
  TestingPrefServiceSimple pref_service_simple;
  pref_service_simple.registry()->RegisterBooleanPref("some_toggle", true);
  pref_service_simple.registry()->RegisterBooleanPref("some_consent_acceptance",
                                                      true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(
                         {.settings_toggle_pref = "some_toggle",
                          .consent_accepted_pref = "some_consent_acceptance"},
                         pref_service_simple)
                         .Check()),
      IsEmpty());
}

TEST(FeatureAccessCheckerTest, CheckSettingsPrefCheckFail) {
  TestingPrefServiceSimple pref_service_simple;
  pref_service_simple.registry()->RegisterBooleanPref("some_toggle", false);
  pref_service_simple.registry()->RegisterBooleanPref("some_consent_acceptance",
                                                      true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(
                         {.settings_toggle_pref = "some_toggle",
                          .consent_accepted_pref = "some_consent_acceptance"},
                         pref_service_simple)
                         .Check()),
      ElementsAre(kDisabledInSettings));
}

TEST(FeatureAccessCheckerTest, ConsentAcceptancePrefCheckPass) {
  TestingPrefServiceSimple pref_service_simple;
  pref_service_simple.registry()->RegisterBooleanPref("some_toggle", true);
  pref_service_simple.registry()->RegisterBooleanPref("some_consent_acceptance",
                                                      true);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(
                         {.settings_toggle_pref = "some_toggle",
                          .consent_accepted_pref = "some_consent_acceptance"},
                         pref_service_simple)
                         .Check()),
      IsEmpty());
}

TEST(FeatureAccessCheckerTest, ConsentAcceptancePrefCheckFail) {
  TestingPrefServiceSimple pref_service_simple;
  pref_service_simple.registry()->RegisterBooleanPref("some_toggle", true);
  pref_service_simple.registry()->RegisterBooleanPref("some_consent_acceptance",
                                                      false);

  EXPECT_THAT(
      base::ToVector(FeatureAccessChecker(
                         {.settings_toggle_pref = "some_toggle",
                          .consent_accepted_pref = "some_consent_acceptance"},
                         pref_service_simple)
                         .Check()),
      ElementsAre(kConsentNotAccepted));
}

}  // namespace
}  // namespace specialized_features
