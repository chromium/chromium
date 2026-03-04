// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/default_browser_promo_ephemeral_module.h"

#include "base/test/scoped_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

// Tests the functionality of `DefaultBrowserPromoEphemeralModule`.
class DefaultBrowserPromoEphemeralModuleTest : public testing::Test {
 public:
  DefaultBrowserPromoEphemeralModuleTest() = default;
  ~DefaultBrowserPromoEphemeralModuleTest() override = default;

  void SetUp() override {
    Test::SetUp();
    DefaultBrowserPromoEphemeralModule::RegisterProfilePrefs(
        pref_service_.registry());
    feature_list_.InitAndEnableFeature(features::kDefaultBrowserMagicStackIos);
  }

  void TearDown() override {
    feature_list_.Reset();
    Test::TearDown();
  }

  // Verifies that `ComputeCardResult` returns the expected rank for the given
  // signals.
  void TestComputeCardResultImpl(float isNewUser,
                                 float isDefaultBrowser,
                                 EphemeralHomeModuleRank expectedRank) {
    auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
    AllCardSignals all_signals =
        CreateAllCardSignals(card.get(), {isNewUser, isDefaultBrowser});
    CardSelectionSignals card_signal(&all_signals,
                                     kDefaultBrowserPromoEphemeralModule);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(expectedRank, result.position);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
};

// Tests that the `GetInputs()` method returns the expected inputs.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       TestGetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 2u);

  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(segmentation_platform::kIsNewUser), inputs.end());
  EXPECT_NE(inputs.find(kIsDefaultBrowserSignalKey), inputs.end());
}

// Tests that `ComputeCardResult()` returns `kTop` with valid signals and
// `kNotShown` otherwise.
TEST_F(DefaultBrowserPromoEphemeralModuleTest, TestComputeCardResult) {
  TestComputeCardResultImpl(/* isNewUser */ 0, /* isDefaultBrowser */ 0,
                            /* expectedRank */ EphemeralHomeModuleRank::kTop);

  TestComputeCardResultImpl(
      /* isNewUser */ 1, /* isDefaultBrowser */ 0,
      /* expectedRank */ EphemeralHomeModuleRank::kNotShown);

  TestComputeCardResultImpl(
      /* isNewUser */ 0, /* isDefaultBrowser */ 1,
      /* expectedRank */ EphemeralHomeModuleRank::kNotShown);

  TestComputeCardResultImpl(
      /* isNewUser */ 1, /* isDefaultBrowser */ 1,
      /* expectedRank */ EphemeralHomeModuleRank::kNotShown);
}

// Validates that `IsEnabled()` returns true when under the impression limit and
// false otherwise.
TEST_F(DefaultBrowserPromoEphemeralModuleTest,
       IsEnabledReturnsFalseWhenImpressionLimitReached) {
  auto card = std::make_unique<DefaultBrowserPromoEphemeralModule>();

  EXPECT_TRUE(DefaultBrowserPromoEphemeralModule::IsEnabled(&pref_service_));

  int max_impressions =
      features::kMaxDefaultBrowserMagicStackIosImpressions.Get();

  for (int i = 0; i < max_impressions; ++i) {
    EXPECT_TRUE(DefaultBrowserPromoEphemeralModule::IsEnabled(&pref_service_));
    card->OnShow(&pref_service_, nullptr);
  }

  // Once max impressions are hit, it should no longer be enabled.
  EXPECT_FALSE(DefaultBrowserPromoEphemeralModule::IsEnabled(&pref_service_));
}

}  // namespace segmentation_platform::home_modules
