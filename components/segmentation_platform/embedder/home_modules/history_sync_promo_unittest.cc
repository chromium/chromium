// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/history_sync_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class HistorySyncPromoTest : public testing::Test {
 public:
  HistorySyncPromoTest() = default;
  ~HistorySyncPromoTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TestComputeCardResultImpl(bool historySyncPromoInteracted,
                                 float historySyncPromoShownCount,
                                 float isEligibleToHistoryOptIn,
                                 float educationalTipShownCount,
                                 EphemeralHomeModuleRank position) {
    pref_service_.SetUserPref(
        kHistorySyncPromoInteractedPref,
        std::make_unique<base::Value>(historySyncPromoInteracted));
    auto card = std::make_unique<HistorySyncPromo>(&pref_service_);
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {educationalTipShownCount, historySyncPromoShownCount,
                     isEligibleToHistoryOptIn});
    CardSelectionSignals card_signal(&all_signals, kHistorySyncPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(HistorySyncPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<HistorySyncPromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kIsEligibleToHistoryOptIn),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kHistorySyncPromoShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
}

TEST_F(HistorySyncPromoTest, HiddenWhenUserIsNotEligibleToHistorySync) {
  TestComputeCardResultImpl(
      /* historySyncPromoInteracted */ false,
      /* historySyncPromoShownCount */ 0,
      /* isEligibleToHistoryOptIn */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(HistorySyncPromoTest, ShownWhenUserIsEligibleToHistorySync) {
  TestComputeCardResultImpl(
      /* historySyncPromoInteracted */ false,
      /* historySyncPromoShownCount */ 0,
      /* isEligibleToHistoryOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kLast);
}

TEST_F(HistorySyncPromoTest, HiddenWhenUserHasInteractedWithCard) {
  TestComputeCardResultImpl(
      /* historySyncPromoInteracted */ true,
      /* historySyncPromoShownCount */ 0,
      /* isEligibleToHistoryOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(HistorySyncPromoTest, HiddenWhenUserHasReachedLimit) {
  TestComputeCardResultImpl(
      /* historySyncPromoInteracted */ true,
      /* historySyncPromoShownCount */ 1,
      /* isEligibleToHistoryOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(HistorySyncPromoTest,
       HiddenForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* historySyncPromoInteracted */ true,
      /* historySyncPromoShownCount */ 1,
      /* isEligibleToHistoryOptIn */ 1,
      /* educationalTipShownCount */ 1, EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
