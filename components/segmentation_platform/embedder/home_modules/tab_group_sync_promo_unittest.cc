// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tab_group_sync_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests TabGroupSyncPromo's functionality.
class TabGroupSyncPromoTest : public testing::Test {
 public:
  TabGroupSyncPromoTest() = default;
  ~TabGroupSyncPromoTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override { Test::TearDown(); }

  void TestComputeCardResultImpl(bool hasTabGroupSyncPromoInteracted,
                                 float syncedTabGroupExists,
                                 float tabGroupSyncPromoShownCount,
                                 float educationalTipShownCount,
                                 EphemeralHomeModuleRank position) {
    pref_service_.SetUserPref(
        kTabGroupSyncPromoInteractedPref,
        std::make_unique<base::Value>(hasTabGroupSyncPromoInteracted));
    auto card = std::make_unique<TabGroupSyncPromo>(&pref_service_);
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {educationalTipShownCount, syncedTabGroupExists,
                     tabGroupSyncPromoShownCount});
    CardSelectionSignals card_signal(&all_signals, kTabGroupSyncPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(TabGroupSyncPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<TabGroupSyncPromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kSyncedTabGroupExists),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kTabGroupSyncPromoShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
}

// Validates that ComputeCardResult() returns kLast when tab group sync promo
// card is enabled.
TEST_F(TabGroupSyncPromoTest, TestComputeCardResultWithCardEnabled) {
  TestComputeCardResultImpl(
      /* hasTabGroupSyncPromoInteracted */ false,
      /* syncedTabGroupExists */ 1,
      /* tabGroupSyncPromoShownCount */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kLast);
}

// Validates that when the tab group sync promo card is disabled because the
// user does not have synced tab group, the ComputeCardResult() function returns
// kNotShown.
TEST_F(TabGroupSyncPromoTest,
       TestComputeCardResultWithCardDisabledForSyncedTabGroupNotExists) {
  TestComputeCardResultImpl(
      /* hasTabGroupSyncPromoInteracted */ false,
      /* syncedTabGroupExists */ 0,
      /* tabGroupSyncPromoShownCount */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// card has been displayed to the user more times than the limit allows.
TEST_F(TabGroupSyncPromoTest,
       TestComputeCardResultWithCardDisabledForHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* hasTabGroupSyncPromoInteracted */ false,
      /* syncedTabGroupExists */ 1,
      /* tabGroupSyncPromoShownCount */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// tab group sync promo card is disabled because the user already interacted
// with the card.
TEST_F(TabGroupSyncPromoTest,
       TestComputeCardResultWithCardDisabledForUserInteraction) {
  TestComputeCardResultImpl(
      /* hasTabGroupSyncPromoInteracted */ true,
      /* syncedTabGroupExists */ 1,
      /* tabGroupSyncPromoShownCount */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when
// educational tip card has been displayed to the user more times than the limit
// allows.
TEST_F(
    TabGroupSyncPromoTest,
    TestComputeCardResultWithCardDisabledForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* hasTabGroupSyncPromoInteracted */ true,
      /* syncedTabGroupExists */ 1,
      /* tabGroupSyncPromoShownCount */ 0,
      /* educationalTipShownCount */ 1, EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
