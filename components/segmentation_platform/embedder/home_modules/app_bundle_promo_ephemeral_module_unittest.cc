// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/app_bundle_promo_ephemeral_module.h"

#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/test_utils.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace home_modules {

// Tests the functionality of `AppBundlePromoEphemeralModule`.
class AppBundlePromoEphemeralModuleTest : public testing::Test {
 public:
  AppBundlePromoEphemeralModuleTest() = default;
  ~AppBundlePromoEphemeralModuleTest() override = default;

  void SetUp() override {
    Test::SetUp();
    feature_list_.InitAndEnableFeature(features::kAppBundlePromoEphemeralCard);
  }

  void TearDown() override {
    feature_list_.Reset();
    Test::TearDown();
  }

  void TestComputeCardResultImpl(float appBundleAppsInstalled,
                                 EphemeralHomeModuleRank expectedRank) {
    auto card = std::make_unique<AppBundlePromoEphemeralModule>();
    AllCardSignals all_signals =
        CreateAllCardSignals(card.get(), {appBundleAppsInstalled});
    CardSelectionSignals card_signal(&all_signals,
                                     kAppBundlePromoEphemeralModule);
    CardSelectionInfo::ShowResult result = card->ComputeCardResult(card_signal);
    EXPECT_EQ(expectedRank, result.position);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the `GetInputs(...)` method returns the expected inputs.
TEST_F(AppBundlePromoEphemeralModuleTest, TestGetInputsReturnsExpectedInputs) {
  auto card = std::make_unique<AppBundlePromoEphemeralModule>();
  std::map<SignalKey, FeatureQuery> inputs = card->GetInputs();
  EXPECT_EQ(inputs.size(), 1u);

  // Verify that the inputs map contains the expected keys.
  EXPECT_NE(inputs.find(kAppBundleAppsInstalledCountSignalKey), inputs.end());
}

// Tests that ComputeCardResult() returns `kTop` with valid signals and
// `kNotShown` otherwise.
TEST_F(AppBundlePromoEphemeralModuleTest, TestComputeCardResult) {
  // If there are `kMaxAppBundleAppsInstalled` or fewer bundle apps on a user's
  // device, the card should be shown. If there are more, then the card should
  // be hidden.
  TestComputeCardResultImpl(
      /* appBundleAppsInstalled */ features::kMaxAppBundleAppsInstalled.Get(),
      /* expectedRank */ EphemeralHomeModuleRank::kTop);
  TestComputeCardResultImpl(
      /* appBundleAppsInstalled */ features::kMaxAppBundleAppsInstalled.Get() +
          1,
      /* expectedRank */ EphemeralHomeModuleRank::kNotShown);
}

// Tests that IsEnabled() returns `true` when under impression limit and false
// otherwise.
TEST_F(AppBundlePromoEphemeralModuleTest, TestIsEnabled) {
  EXPECT_TRUE(AppBundlePromoEphemeralModule::IsEnabled(
      /* impressions */ features::kMaxAppBundlePromoImpressions.Get() - 1));
  EXPECT_FALSE(AppBundlePromoEphemeralModule::IsEnabled(
      /* impressions */ features::kMaxAppBundlePromoImpressions.Get()));
}

}  // namespace home_modules
}  // namespace segmentation_platform
