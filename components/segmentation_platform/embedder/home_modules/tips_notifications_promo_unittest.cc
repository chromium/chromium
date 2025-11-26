// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"

#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class TipsNotificationsPromoTest : public testing::Test {
 public:
  TipsNotificationsPromoTest() = default;
  ~TipsNotificationsPromoTest() override = default;

  void SetUp() override {
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
  }

  void TestComputeCardResultImpl(bool tipsNotificationsPromoInteracted,
                                 float tipsNotificationsPromoShownCount,
                                 float isEligibleToTipsOptIn,
                                 float educationalTipShownCount,
                                 EphemeralHomeModuleRank position) {
    pref_service_.SetUserPref(
        kTipsNotificationsPromoInteractedPref,
        std::make_unique<base::Value>(tipsNotificationsPromoInteracted));
    auto card = std::make_unique<TipsNotificationsPromo>(&pref_service_);
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {tipsNotificationsPromoShownCount, isEligibleToTipsOptIn,
                     educationalTipShownCount});
    CardSelectionSignals card_signal(&all_signals, kTipsNotificationsPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(TipsNotificationsPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<TipsNotificationsPromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(
      inputs.find(segmentation_platform::kTipsNotificationsPromoShownCount),
      inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsEligibleToTipsOptIn),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
}

TEST_F(TipsNotificationsPromoTest, HiddenWhenNotEligibleToTipsNotifications) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(TipsNotificationsPromoTest, ShownWhenEligibleToTipsNotifications) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kLast);
}

TEST_F(TipsNotificationsPromoTest, HiddenWhenUserHasInteractedWithCard) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ true,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(TipsNotificationsPromoTest, HiddenWhenUserHasReachedLimit) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 1,
      /* isEligibleToTipsOptIn */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(TipsNotificationsPromoTest,
       HiddenForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 0,
      /* educationalTipShownCount */ 1, EphemeralHomeModuleRank::kNotShown);
}

}  // namespace segmentation_platform::home_modules
