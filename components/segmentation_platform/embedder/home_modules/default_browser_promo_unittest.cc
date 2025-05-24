// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests DefaultBrowserPromo's functionality.
class DefaultBrowserPromoTest : public testing::Test {
 public:
  DefaultBrowserPromoTest() = default;
  ~DefaultBrowserPromoTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

  void TestComputeCardResultImpl(
      bool hasDefaultBrowserPromoInteracted,
      float hasDefaultBrowserPromoShownInOtherSurface,
      float shouldShowNonRoleManagerDefaultBrowserPromo,
      float isUserSignedIn,
      EphemeralHomeModuleRank position) {
    pref_service_.SetUserPref(
        kDefaultBrowserPromoInteractedPref,
        std::make_unique<base::Value>(hasDefaultBrowserPromoInteracted));
    auto card = std::make_unique<DefaultBrowserPromo>(&pref_service_);
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {hasDefaultBrowserPromoShownInOtherSurface, isUserSignedIn,
                     shouldShowNonRoleManagerDefaultBrowserPromo});
    CardSelectionSignals card_signal(&all_signals, kDefaultBrowserPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(DefaultBrowserPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<DefaultBrowserPromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(
      inputs.find(
          segmentation_platform::kShouldShowNonRoleManagerDefaultBrowserPromo),
      inputs.end());
  EXPECT_NE(
      inputs.find(
          segmentation_platform::kHasDefaultBrowserPromoShownInOtherSurface),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsUserSignedIn), inputs.end());
}

// Validates that ComputeCardResult() returns kLast when default browser promo
// card is enabled.
TEST_F(DefaultBrowserPromoTest, TestComputeCardResultWithCardEnabled) {
  TestComputeCardResultImpl(/* hasDefaultBrowserPromoInteracted */ false,
                            /* hasDefaultBrowserPromoShownInOtherSurface */ 0,
                            /* shouldShowNonRoleManagerDefaultBrowserPromo */ 1,
                            /* isUserSignedIn */ 1,
                            EphemeralHomeModuleRank::kLast);
}

// Validates that when the default browser promo card is disabled because the
// non-role manager default browser promo should not be displayed, the
// ComputeCardResult() function returns kNotShown.
TEST_F(DefaultBrowserPromoTest,
       TestComputeCardResultWithCardDisabledForNotShowNonRoleManagerPromo) {
  TestComputeCardResultImpl(/* hasDefaultBrowserPromoInteracted */ false,
                            /* hasDefaultBrowserPromoShownInOtherSurface */ 0,
                            /* shouldShowNonRoleManagerDefaultBrowserPromo */ 0,
                            /* isUserSignedIn */ 1,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// default browser promo card is disabled because the user already saw the promo
// in other surfaces, such as through settings, messages, or alternative NTPs.
TEST_F(DefaultBrowserPromoTest,
       TestComputeCardResultWithCardDisabledForShownInOtherSurface) {
  TestComputeCardResultImpl(/* hasDefaultBrowserPromoInteracted */ false,
                            /* hasDefaultBrowserPromoShownInOtherSurface */ 1,
                            /* shouldShowNonRoleManagerDefaultBrowserPromo */ 1,
                            /* isUserSignedIn */ 1,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// default browser promo card is disabled because the user already interacted
// with the card.
TEST_F(DefaultBrowserPromoTest,
       TestComputeCardResultWithCardDisabledForUserInteraction) {
  TestComputeCardResultImpl(/* hasDefaultBrowserPromoInteracted */ true,
                            /* hasDefaultBrowserPromoShownInOtherSurface */ 0,
                            /* shouldShowNonRoleManagerDefaultBrowserPromo */ 1,
                            /* isUserSignedIn */ 1,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// default browser promo card is disabled because the user has not signed in.
TEST_F(DefaultBrowserPromoTest,
       TestComputeCardResultWithCardDisabledForUserNotSignedIn) {
  TestComputeCardResultImpl(/* hasDefaultBrowserPromoInteracted */ false,
                            /* hasDefaultBrowserPromoShownInOtherSurface */ 0,
                            /* shouldShowNonRoleManagerDefaultBrowserPromo */ 1,
                            /* isUserSignedIn */ 0,
                            EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
