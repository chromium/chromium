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

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(TabGroupPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kTabGroupExists), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kNumberOfTabs), inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kTabGroupPromoShownCount),
            inputs.end());
}

// Validates that ComputeCardResult() returns kTop when tab group promo
// card is enabled.
TEST_F(TabGroupPromoTest, TestComputeCardResultWithCardEnabled) {
  pref_service_.SetUserPref(kTabGroupPromoInteractedPref,
                            std::make_unique<base::Value>(false));
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kNumberOfTabs */ 11, /* kTabGroupExists */ 0,
                   /* kTabGroupPromoShownCount */ 0});
  CardSelectionSignals card_signal(&all_signals, kTabGroupPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kTop, result.position);
}

// Validates that when the tab group promo card is disabled because the user
// already has tab group, the ComputeCardResult() function returns kNotShown.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForTabGroupExists) {
  pref_service_.SetUserPref(kTabGroupPromoInteractedPref,
                            std::make_unique<base::Value>(false));
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kNumberOfTabs */ 11, /* kTabGroupExists */ 1,
                   /* kTabGroupPromoShownCount */ 0});
  CardSelectionSignals card_signal(&all_signals, kTabGroupPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// user has 10 or fewer tabs.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForNotEnoughTabs) {
  pref_service_.SetUserPref(kTabGroupPromoInteractedPref,
                            std::make_unique<base::Value>(false));
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kNumberOfTabs */ 10, /* kTabGroupExists */ 0,
                   /* kTabGroupPromoShownCount */ 0});
  CardSelectionSignals card_signal(&all_signals, kTabGroupPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// card has been displayed to the user more times than the single day limit
// allows.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForHasReachedSessionLimit) {
  pref_service_.SetUserPref(kTabGroupPromoInteractedPref,
                            std::make_unique<base::Value>(false));
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kNumberOfTabs */ 11, /* kTabGroupExists */ 0,
                   /* kTabGroupPromoShownCount */ 3});
  CardSelectionSignals card_signal(&all_signals, kTabGroupPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// tab group promo card is disabled because the user already interacted with
// the card.
TEST_F(TabGroupPromoTest,
       TestComputeCardResultWithCardDisabledForUserInteraction) {
  pref_service_.SetUserPref(kTabGroupPromoInteractedPref,
                            std::make_unique<base::Value>(true));
  auto card = std::make_unique<TabGroupPromo>(&pref_service_);
  AllCardSignals all_signals = CreateAllCardSignals(
      card.get(), {/* kNumberOfTabs */ 11, /* kTabGroupExists */ 0,
                   /* kTabGroupPromoShownCount */ 0});
  CardSelectionSignals card_signal(&all_signals, kTabGroupPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
}

}  // namespace segmentation_platform::home_modules
