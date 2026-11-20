// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_notifications_promo.h"

#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class TipsNotificationsPromoTest : public testing::Test {
 public:
  TipsNotificationsPromoTest() = default;
  ~TipsNotificationsPromoTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kAndroidTipsNotifications}, {});
    TipsNotificationsPromo::RegisterProfilePrefs(pref_service_.registry());
  }

  void TestComputeCardResultImpl(bool tipsNotificationsPromoInteracted,
                                 float tipsNotificationsPromoShownCount,
                                 float isEligibleToTipsOptIn,
                                 float educationalTipShownCount,
                                 EphemeralHomeModuleRank position) {
    auto card = std::make_unique<TipsNotificationsPromo>(&pref_service_);

    if (tipsNotificationsPromoInteracted) {
      card->OnInteract(&pref_service_, nullptr);
    }

    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {educationalTipShownCount, isEligibleToTipsOptIn,
                     tipsNotificationsPromoShownCount});
    CardSelectionSignals card_signal(&all_signals, kTipsNotificationsPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TipsNotificationsPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<TipsNotificationsPromo>(&pref_service_);
  STACK_ALLOCATED_IGNORE("https://crbug.com/350256130")
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
      /* isEligibleToTipsOptIn */ 0,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(TipsNotificationsPromoTest, ShownWhenEligibleToTipsNotifications) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kLast);
}

TEST_F(TipsNotificationsPromoTest, HiddenWhenUserHasInteractedWithCard) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ true,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(TipsNotificationsPromoTest, HiddenWhenUserHasReachedLimit) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 1,
      /* isEligibleToTipsOptIn */ 1,
      /* educationalTipShownCount */ 0, EphemeralHomeModuleRank::kNotShown);
}

TEST_F(TipsNotificationsPromoTest,
       HiddenForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* tipsNotificationsPromoInteracted */ false,
      /* tipsNotificationsPromoShownCount */ 0,
      /* isEligibleToTipsOptIn */ 1,
      /* educationalTipShownCount */ 1, EphemeralHomeModuleRank::kNotShown);
}

// Validates that `IsEnabled()` returns true when under the impression limit and
// false otherwise.
TEST_F(TipsNotificationsPromoTest,
       IsEnabledReturnsFalseWhenImpressionLimitReached) {
  EXPECT_TRUE(TipsNotificationsPromo::IsEnabled(&pref_service_));

  // Recreate the card each iteration to simulate separate sessions.
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions; ++i) {
    auto session_card =
        std::make_unique<TipsNotificationsPromo>(&pref_service_);
    EXPECT_TRUE(TipsNotificationsPromo::IsEnabled(&pref_service_));
    session_card->OnShow(&pref_service_, nullptr);
  }

  // Once max impressions are hit, it should no longer be enabled.
  EXPECT_FALSE(TipsNotificationsPromo::IsEnabled(&pref_service_));
}

TEST_F(TipsNotificationsPromoTest, IsEnabledReturnsFalseWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAndroidTipsNotifications);

  EXPECT_FALSE(TipsNotificationsPromo::IsEnabled(&pref_service_));
}

TEST_F(TipsNotificationsPromoTest, IsEnabledReturnsTrueWhenForcedToShow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kSegmentationPlatformEphemeralCardRanker,
        {{features::kEphemeralCardRankerForceShowCardParam,
          kTipsNotificationsPromo}}}},
      {features::kAndroidTipsNotifications});

  EXPECT_TRUE(TipsNotificationsPromo::IsEnabled(&pref_service_));
}

}  // namespace segmentation_platform::home_modules
