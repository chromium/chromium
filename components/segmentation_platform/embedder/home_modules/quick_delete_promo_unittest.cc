// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/quick_delete_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests QuickDeletePromo's functionality.
class QuickDeletePromoTest : public testing::Test {
 public:
  QuickDeletePromoTest() = default;
  ~QuickDeletePromoTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

  void TestComputeCardResultImpl(
      bool hasQuickDeletePromoInteracted,
      float countOfClearingBrowsingData,
      float countOfClearingBrowsingDataThroughQuickDelete,
      float quickDeletePromoShownCount,
      float isUserSignedIn,
      float educationalTipShownCount,
      EphemeralHomeModuleRank position) {
    pref_service_.SetUserPref(
        kQuickDeletePromoInteractedPref,
        std::make_unique<base::Value>(hasQuickDeletePromoInteracted));
    auto card = std::make_unique<QuickDeletePromo>(&pref_service_);
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(),
        {countOfClearingBrowsingData,
         countOfClearingBrowsingDataThroughQuickDelete,
         educationalTipShownCount, isUserSignedIn, quickDeletePromoShownCount});
    CardSelectionSignals card_signal(&all_signals, kQuickDeletePromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(QuickDeletePromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<QuickDeletePromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 5u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kCountOfClearingBrowsingData),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::
                            kCountOfClearingBrowsingDataThroughQuickDelete),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kQuickDeletePromoShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsUserSignedIn), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
}

// Validates that ComputeCardResult() returns kLast when quick delete promo card
// is enabled for user who never cleared their browsing data at all in the past
// 30 days.
TEST_F(QuickDeletePromoTest,
       TestComputeCardResultWithCardEnabledForNeverClearedBrowsingData) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ false,
      /* countOfClearingBrowsingData */ 0,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 0,
      /* quickDeletePromoShownCount */ 0,
      /* isUserSignedIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kLast);
}

// Validates that ComputeCardResult() returns kLast when quick delete promo card
// is enabled for user who cleared their browsing data without knowing about the
// Quick Delete feature in the past 30 days.
TEST_F(QuickDeletePromoTest,
       TestComputeCardResultWithCardEnabledForNotUseQuickDelete) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ false,
      /* countOfClearingBrowsingData */ 5,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 0,
      /* quickDeletePromoShownCount */ 0, /* isUserSignedIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kLast);
}

// Validates that when the quick delete promo card is disabled because the user
// have used the Quick Delete feature, the ComputeCardResult() function returns
// kNotShown.
TEST_F(QuickDeletePromoTest,
       TestComputeCardResultWithCardDisabledForHaveUsedQuickDelete) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ false,
      /* countOfClearingBrowsingData */ 5,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 3,
      /* quickDeletePromoShownCount */ 0, /* isUserSignedIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// card has been displayed to the user more times than the limit allows.
TEST_F(QuickDeletePromoTest,
       TestComputeCardResultWithCardDisabledForHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ false,
      /* countOfClearingBrowsingData */ 5,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 0,
      /* quickDeletePromoShownCount */ 1, /* isUserSignedIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// quick delete promo card is disabled because the user already interacted with
// the card.
TEST_F(QuickDeletePromoTest,
       TestComputeCardResultWithCardDisabledForUserInteraction) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ true,
      /* countOfClearingBrowsingData */ 5,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 0,
      /* quickDeletePromoShownCount */ 0, /* isUserSignedIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// quick delete promo card is disabled because the user is not signed in.
TEST_F(QuickDeletePromoTest,
       TestComputeCardResultWithCardDisabledForUserNotSignedIn) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ false,
      /* countOfClearingBrowsingData */ 0,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 0,
      /* quickDeletePromoShownCount */ 0, /* isUserSignedIn */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when
// educational tip card has been displayed to the user more times than the limit
// allows.
TEST_F(
    QuickDeletePromoTest,
    TestComputeCardResultWithCardDisabledForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* hasQuickDeletePromoInteracted */ false,
      /* countOfClearingBrowsingData */ 0,
      /* countOfClearingBrowsingDataThroughQuickDelete */ 0,
      /* quickDeletePromoShownCount */ 0, /* isUserSignedIn */ 1,
      /* educationalTipShownCount */ 1, EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
