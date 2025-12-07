// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tab_group_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests TabGroupPromo's functionality.
class TabGroupPromoTest : public testing::Test {
 public:
  TabGroupPromoTest() = default;
  ~TabGroupPromoTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

  void TestComputeCardResultImpl(bool hasTabGroupPromoInteracted,
                                 float numberOfTabs,
                                 float tabGroupExists,
                                 float tabGroupPromoShownCount,
                                 float isUserSignedIn,
                                 float educationalTipShownCount,
                                 EphemeralHomeModuleRank position) {
    pref_service_.SetUserPref(
        kTabGroupPromoInteractedPref,
        std::make_unique<base::Value>(hasTabGroupPromoInteracted));
    auto card = std::make_unique<TabGroupPromo>(&pref_service_);
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {educationalTipShownCount, isUserSignedIn, numberOfTabs,
                     tabGroupExists, tabGroupPromoShownCount});
    CardSelectionSignals card_signal(&all_signals, kTabGroupPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(TabGroupPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 5u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kTabGroupExists), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kNumberOfTabs), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kTabGroupPromoShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsUserSignedIn), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
}

// Validates that ComputeCardResult() returns kLast when tab group promo
// card is enabled.
TEST_F(TabGroupPromoTest, TestComputeCardResultWithCardEnabled) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ false,
                            /* numberOfTabs */ 11,
                            /* tabGroupExists */ 0,
                            /* tabGroupPromoShownCount */ 0,
                            /* isUserSignedIn */ 1,
                            /* educationalTipShownCount */ 0,
                            EphemeralHomeModuleRank::kLast);
}

// Validates that when the tab group promo card is disabled because the user
// already has tab group, the ComputeCardResult() function returns kNotShown.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForTabGroupExists) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ false,
                            /* numberOfTabs */ 11,
                            /* tabGroupExists */ 1,
                            /* tabGroupPromoShownCount */ 0,
                            /* isUserSignedIn */ 1,
                            /* educationalTipShownCount */ 0,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// user has 10 or fewer tabs.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForNotEnoughTabs) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ false,
                            /* numberOfTabs */ 10,
                            /* tabGroupExists */ 0,
                            /* tabGroupPromoShownCount */ 0,
                            /* isUserSignedIn */ 1,
                            /* educationalTipShownCount */ 0,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// card has been displayed to the user more times than the limit allows.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForHasReachedSessionLimit) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ false,
                            /* numberOfTabs */ 11,
                            /* tabGroupExists */ 0,
                            /* tabGroupPromoShownCount */ 1,
                            /* isUserSignedIn */ 1,
                            /* educationalTipShownCount */ 0,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// tab group promo card is disabled because the user already interacted with
// the card.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForUserInteraction) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ true,
                            /* numberOfTabs */ 11,
                            /* tabGroupExists */ 0,
                            /* tabGroupPromoShownCount */ 0,
                            /* isUserSignedIn */ 1,
                            /* educationalTipShownCount */ 0,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// tab group promo card is disabled because the user is not signed in.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForUserNotSignedIn) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ false,
                            /* numberOfTabs */ 11,
                            /* tabGroupExists */ 0,
                            /* tabGroupPromoShownCount */ 0,
                            /* isUserSignedIn */ 0,
                            /* educationalTipShownCount */ 0,
                            EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when
// educational tip card has been displayed to the user more times than the limit
// allows.
TEST_F(
    TabGroupPromoTest,
    TestComputeCardResultWithCardDisabledForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(/* hasTabGroupPromoInteracted */ false,
                            /* numberOfTabs */ 11,
                            /* tabGroupExists */ 0,
                            /* tabGroupPromoShownCount */ 0,
                            /* isUserSignedIn */ 1,
                            /* educationalTipShownCount */ 1,
                            EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
