// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/fusebox_promo.h"

#include <map>
#include <memory>
#include <vector>

#include "base/memory/stack_allocated.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests FuseboxPromo's functionality.
class FuseboxPromoTest : public testing::Test {
 public:
  FuseboxPromoTest() = default;
  ~FuseboxPromoTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        omnibox::kOmniboxFuseboxUserEdGuidedTour);
    RegisterPrefs(pref_service_);
  }

  void TearDown() override { Test::TearDown(); }

  static void RegisterPrefs(TestingPrefServiceSimple& prefs) {
    FuseboxPromo::RegisterProfilePrefs(prefs.registry());
    prefs.registry()->RegisterBooleanPref(omnibox::kFuseboxAimQuerySubmitted,
                                          false);
  }

  void TestComputeCardResultImpl(bool has_fusebox_promo_interacted,
                                 bool has_submitted_aim_query,
                                 float educational_tip_shown_count,
                                 float fusebox_promo_shown_count,
                                 float is_fusebox_eligible,
                                 EphemeralHomeModuleRank position) {
    TestingPrefServiceSimple prefs;
    RegisterPrefs(prefs);
    auto card = std::make_unique<FuseboxPromo>(&prefs);

    if (has_fusebox_promo_interacted) {
      card->OnInteract(&prefs, nullptr);
    }
    prefs.SetBoolean(omnibox::kFuseboxAimQuerySubmitted,
                     has_submitted_aim_query);

    // Signals are ordered alphabetically by SignalKey in GetInputs() map:
    // 1. kEducationalTipShownCount ("educational_tip_shown_count")
    // 2. kFuseboxPromoShownCount ("fusebox_promo_shown_count")
    // 3. kIsFuseboxEligible ("is_fusebox_eligible")
    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {educational_tip_shown_count, fusebox_promo_shown_count,
                     is_fusebox_eligible});
    CardSelectionSignals card_signal(&all_signals, kFuseboxPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(FuseboxPromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<FuseboxPromo>(&pref_service_);
  STACK_ALLOCATED_IGNORE("https://crbug.com/350256130")
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  EXPECT_NE(inputs.find(segmentation_platform::kFuseboxPromoShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kIsFuseboxEligible),
            inputs.end());
}

// Validates ComputeCardResult() across interaction, signal counts, and cooldown
// limits.
TEST_F(FuseboxPromoTest, ComputeCardResult) {
  const struct {
    const char* description;
    bool has_interacted;
    bool has_submitted_aim_query;
    float count_educational_tip_shown;
    float count_fusebox_promo_shown;
    float is_fusebox_eligible;
    EphemeralHomeModuleRank expected_rank;
  } kTestCases[] = {
      {"All conditions met: card shown at last position",
       /*has_interacted=*/false, /*has_submitted_aim_query=*/false,
       /*count_educational_tip_shown=*/0,
       /*count_fusebox_promo_shown=*/0, /*is_fusebox_eligible=*/1,
       EphemeralHomeModuleRank::kLast},
      {"Disabled: Fusebox is not eligible",
       /*has_interacted=*/false, /*has_submitted_aim_query=*/false,
       /*count_educational_tip_shown=*/0,
       /*count_fusebox_promo_shown=*/0, /*is_fusebox_eligible=*/0,
       EphemeralHomeModuleRank::kNotShown},
      {"Disabled: AI Mode query already submitted",
       /*has_interacted=*/false, /*has_submitted_aim_query=*/true,
       /*count_educational_tip_shown=*/0,
       /*count_fusebox_promo_shown=*/0, /*is_fusebox_eligible=*/1,
       EphemeralHomeModuleRank::kNotShown},
      {"Disabled: promo shown within per-card cooldown",
       /*has_interacted=*/false, /*has_submitted_aim_query=*/false,
       /*count_educational_tip_shown=*/0,
       /*count_fusebox_promo_shown=*/1, /*is_fusebox_eligible=*/1,
       EphemeralHomeModuleRank::kNotShown},
      {"Disabled: user already interacted",
       /*has_interacted=*/true, /*has_submitted_aim_query=*/false,
       /*count_educational_tip_shown=*/0,
       /*count_fusebox_promo_shown=*/0, /*is_fusebox_eligible=*/1,
       EphemeralHomeModuleRank::kNotShown},
      {"Disabled: educational tip shown within general cooldown",
       /*has_interacted=*/false, /*has_submitted_aim_query=*/false,
       /*count_educational_tip_shown=*/1,
       /*count_fusebox_promo_shown=*/0, /*is_fusebox_eligible=*/1,
       EphemeralHomeModuleRank::kNotShown},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    TestComputeCardResultImpl(
        test_case.has_interacted, test_case.has_submitted_aim_query,
        test_case.count_educational_tip_shown,
        test_case.count_fusebox_promo_shown, test_case.is_fusebox_eligible,
        test_case.expected_rank);
  }
}

// Validates that `IsEnabled()` returns false when the guided tour feature flag
// is disabled.
TEST_F(FuseboxPromoTest, IsEnabledReturnsFalseWhenFeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(omnibox::kOmniboxFuseboxUserEdGuidedTour);
  EXPECT_FALSE(FuseboxPromo::IsEnabled(&pref_service_));
}

// Validates that `IsEnabled()` returns true when under the impression limit
// and false once max impressions are recorded.
TEST_F(FuseboxPromoTest, IsEnabledReturnsFalseWhenImpressionLimitReached) {
  EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));

  int max_impressions = kSingleEphemeralCardMaxImpressions;

  // Recreate the card each iteration to simulate separate sessions, as
  // impressions are counted once per card lifetime.
  for (int i = 0; i < max_impressions; ++i) {
    auto session_card = std::make_unique<FuseboxPromo>(&pref_service_);
    EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));
    session_card->OnShow(&pref_service_, nullptr);
  }

  // Once max impressions are hit, it should no longer be enabled.
  EXPECT_FALSE(FuseboxPromo::IsEnabled(&pref_service_));
}

// Validates that `IsEnabled()` returns false once the user has interacted with
// the promo card.
TEST_F(FuseboxPromoTest, IsEnabledReturnsFalseAfterInteraction) {
  EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));

  auto card = std::make_unique<FuseboxPromo>(&pref_service_);
  card->OnInteract(&pref_service_, nullptr);

  EXPECT_FALSE(FuseboxPromo::IsEnabled(&pref_service_));
}

// Validates that `IsEnabled()` returns false once the user has submitted an AI
// Mode query.
TEST_F(FuseboxPromoTest, IsEnabledReturnsFalseAfterAimQuerySubmitted) {
  EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));

  pref_service_.SetBoolean(omnibox::kFuseboxAimQuerySubmitted, true);

  EXPECT_FALSE(FuseboxPromo::IsEnabled(&pref_service_));
}

// Validates that multiple `OnShow()` calls within the same session only count
// as a single impression.
TEST_F(FuseboxPromoTest, MultipleOnShowCallsInOneSessionCountOnce) {
  auto first_session_card = std::make_unique<FuseboxPromo>(&pref_service_);
  for (int i = 0; i < kSingleEphemeralCardMaxImpressions + 1; ++i) {
    first_session_card->OnShow(&pref_service_, nullptr);
  }
  EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));

  // Showing the card in the remaining sessions should hit the max limit on the
  // exact final session, proving the first session recorded only 1 impression.
  for (int i = 1; i < kSingleEphemeralCardMaxImpressions; ++i) {
    EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));
    auto session_card = std::make_unique<FuseboxPromo>(&pref_service_);
    session_card->OnShow(&pref_service_, nullptr);
  }
  EXPECT_FALSE(FuseboxPromo::IsEnabled(&pref_service_));
}

// Validates that `ComputeCardResult()` returns `kNotShown` if any required
// signal is missing.
TEST_F(FuseboxPromoTest, ComputeCardResultReturnsNotShownWhenSignalMissing) {
  auto card = std::make_unique<FuseboxPromo>(&pref_service_);
  const std::vector<SignalKey> kRequiredSignals = {
      kEducationalTipShownCount, kFuseboxPromoShownCount, kIsFuseboxEligible};

  for (const SignalKey& missing_signal : kRequiredSignals) {
    SCOPED_TRACE("Missing signal: " + missing_signal);
    AllCardSignals all_signals =
        CreateAllCardSignals(card.get(), {/*educational_tip_shown_count=*/0,
                                          /*fusebox_promo_shown_count=*/0,
                                          /*is_fusebox_eligible=*/1});
    all_signals.signal_map_[kFuseboxPromo].erase(missing_signal);
    CardSelectionSignals card_signal(&all_signals, kFuseboxPromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
  }
}

// Validates that force-showing the card overrides feature/pref/signal checks.
TEST_F(FuseboxPromoTest, ForceShowOverridesChecks) {
  // Force-show the card while disabling the guided tour feature flag.
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kSegmentationPlatformEphemeralCardRanker,
        {{features::kEphemeralCardRankerForceShowCardParam, kFuseboxPromo}}}},
      {omnibox::kOmniboxFuseboxUserEdGuidedTour});

  // Set both suppression prefs (already interacted and AI Mode query
  // submitted) that would normally hide the card.
  auto card = std::make_unique<FuseboxPromo>(&pref_service_);
  card->OnInteract(&pref_service_, nullptr);
  pref_service_.SetBoolean(omnibox::kFuseboxAimQuerySubmitted, true);

  EXPECT_TRUE(FuseboxPromo::IsEnabled(&pref_service_));

  // Pass ineligible signals (cooldowns active and Fusebox ineligible).
  AllCardSignals all_signals =
      CreateAllCardSignals(card.get(), {/*educational_tip_shown_count=*/1,
                                        /*fusebox_promo_shown_count=*/1,
                                        /*is_fusebox_eligible=*/0});
  CardSelectionSignals card_signal(&all_signals, kFuseboxPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kLast, result.position);
  EXPECT_EQ(kFuseboxPromo, result.result_label);
}

// Validates that force-hiding the card disables registration and returns
// `kNotShown`.
TEST_F(FuseboxPromoTest, ForceHideOverridesChecks) {
  // Force-hide the card while keeping the guided tour feature flag enabled.
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{omnibox::kOmniboxFuseboxUserEdGuidedTour, {}},
       {features::kSegmentationPlatformEphemeralCardRanker,
        {{features::kEphemeralCardRankerForceHideCardParam, kFuseboxPromo}}}},
      {});

  EXPECT_FALSE(FuseboxPromo::IsEnabled(&pref_service_));

  // Pass otherwise-eligible signals and verify the card is still hidden.
  auto card = std::make_unique<FuseboxPromo>(&pref_service_);
  AllCardSignals all_signals =
      CreateAllCardSignals(card.get(), {/*educational_tip_shown_count=*/0,
                                        /*fusebox_promo_shown_count=*/0,
                                        /*is_fusebox_eligible=*/1});
  CardSelectionSignals card_signal(&all_signals, kFuseboxPromo);
  CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
  EXPECT_EQ(EphemeralHomeModuleRank::kNotShown, result.position);
  EXPECT_EQ(kFuseboxPromo, result.result_label);
}

}  // namespace segmentation_platform::home_modules
