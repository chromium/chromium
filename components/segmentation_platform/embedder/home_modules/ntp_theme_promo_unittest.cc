// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ntp_theme_promo.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry_android.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests NtpThemePromo's functionality.
class NtpThemePromoTest : public testing::Test {
 public:
  NtpThemePromoTest() = default;
  ~NtpThemePromoTest() override = default;

  void SetUp() override {
    NtpThemePromo::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterDictionaryPref(
        "ntp.custom_background_dict2");
  }

  void TearDown() override { Test::TearDown(); }

  void TestComputeCardResultImpl(bool hasNtpThemePromoInteracted,
                                 float ntpThemePromoShownCount,
                                 float educationalTipShownCount,
                                 float supportCustomizedNtpTheme,
                                 EphemeralHomeModuleRank position) {
    auto card = std::make_unique<NtpThemePromo>(&pref_service_);

    if (hasNtpThemePromoInteracted) {
      card->OnInteract(&pref_service_, nullptr);
    }

    AllCardSignals all_signals = CreateAllCardSignals(
        card.get(), {educationalTipShownCount, ntpThemePromoShownCount,
                     supportCustomizedNtpTheme});
    CardSelectionSignals card_signal(&all_signals, kNtpThemePromo);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(position, result.position);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

// Verifies that the `GetInputs(…)` method returns the expected inputs.
TEST_F(NtpThemePromoTest, GetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<NtpThemePromo>(&pref_service_);
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 3u);
  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kNtpThemePromoShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kEducationalTipShownCount),
            inputs.end());
  EXPECT_NE(inputs.find(segmentation_platform::kSupportCustomizedNtpTheme),
            inputs.end());
}

// Validates that ComputeCardResult() returns kLast when ntp theme promo card
// is enabled.
TEST_F(NtpThemePromoTest, TestComputeCardResultWithCardEnabled) {
  TestComputeCardResultImpl(
      /* hasNtpThemePromoInteracted */ false,
      /* ntpThemePromoShownCount */ 0,
      /* educationalTipShownCount */ 0,
      /* supportCustomizedNtpTheme */ 1.0, EphemeralHomeModuleRank::kLast);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// card has been displayed to the user more times than the limit allows.
TEST_F(NtpThemePromoTest,
       TestComputeCardResultWithCardDisabledForHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* hasNtpThemePromoInteracted */ false,
      /* ntpThemePromoShownCount */ 1,
      /* educationalTipShownCount */ 0,
      /* supportCustomizedNtpTheme */ 1.0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when the
// ntp theme promo card is disabled because the user already interacted with
// the card.
TEST_F(NtpThemePromoTest,
       TestComputeCardResultWithCardDisabledForUserInteraction) {
  TestComputeCardResultImpl(
      /* hasNtpThemePromoInteracted */ true,
      /* ntpThemePromoShownCount */ 0,
      /* educationalTipShownCount */ 0,
      /* supportCustomizedNtpTheme */ 1.0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that the ComputeCardResult() function returns kNotShown when
// educational tip card has been displayed to the user more times than the limit
// allows.
TEST_F(
    NtpThemePromoTest,
    TestComputeCardResultWithCardDisabledForEducationalTipCardHasReachedSessionLimit) {
  TestComputeCardResultImpl(
      /* hasNtpThemePromoInteracted */ false,
      /* ntpThemePromoShownCount */ 0,
      /* educationalTipShownCount */ 1,
      /* supportCustomizedNtpTheme */ 1.0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that ComputeCardResult() returns kNotShown when
// support_customized_ntp_theme is false.
TEST_F(NtpThemePromoTest, TestComputeCardResultWithCardDisabledForNoSupport) {
  TestComputeCardResultImpl(
      /* hasNtpThemePromoInteracted */ false,
      /* ntpThemePromoShownCount */ 0,
      /* educationalTipShownCount */ 0,
      /* supportCustomizedNtpTheme */ 0.0, EphemeralHomeModuleRank::kNotShown);
}

// Validates that `IsEnabled()` returns true when under the impression limit and
// false otherwise.
TEST_F(NtpThemePromoTest, IsEnabledReturnsFalseWhenImpressionLimitReached) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kNewTabPageCustomizationV2,
      {{"show_promo", "true"}});

  auto card = std::make_unique<NtpThemePromo>(&pref_service_);

  EXPECT_TRUE(NtpThemePromo::IsEnabled(&pref_service_));

  int max_impressions = kSingleEphemeralCardMaxImpressions;

  // Recreate the card each iteration to simulate separate sessions, as
  // impressions are counted once per card lifetime.
  for (int i = 0; i < max_impressions; ++i) {
    auto session_card = std::make_unique<NtpThemePromo>(&pref_service_);
    EXPECT_TRUE(NtpThemePromo::IsEnabled(&pref_service_));
    session_card->OnShow(&pref_service_, nullptr);
  }

  // Once max impressions are hit, it should no longer be enabled.
  EXPECT_FALSE(NtpThemePromo::IsEnabled(&pref_service_));
}

// Validates that `IsEnabled()` returns false when the feature flag is disabled.
TEST_F(NtpThemePromoTest, IsEnabledReturnsFalseWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      segmentation_platform::features::kNewTabPageCustomizationV2);

  EXPECT_FALSE(NtpThemePromo::IsEnabled(&pref_service_));
}

// Validates that `IsEnabled()` returns false when the preference is managed.
TEST_F(NtpThemePromoTest, IsEnabledReturnsFalseWhenPrefManaged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kNewTabPageCustomizationV2,
      {{"show_promo", "true"}});

  pref_service_.SetManagedPref(
      "ntp.custom_background_dict2",
      std::make_unique<base::Value>(base::Value::Type::DICT));

  EXPECT_FALSE(NtpThemePromo::IsEnabled(&pref_service_));
}

// Validates that `IsEnabled()` returns false when the show_promo parameter is
// disabled.
TEST_F(NtpThemePromoTest, IsEnabledReturnsFalseWhenShowPromoParamDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      segmentation_platform::features::kNewTabPageCustomizationV2,
      {{"show_promo", "false"}});

  EXPECT_FALSE(NtpThemePromo::IsEnabled(&pref_service_));
}

}  // namespace segmentation_platform::home_modules
