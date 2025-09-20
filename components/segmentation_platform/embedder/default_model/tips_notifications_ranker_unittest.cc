// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/tips_notifications_ranker.h"

#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
class TipsNotificationsRankerTest : public DefaultModelTestBase {
 public:
  TipsNotificationsRankerTest()
      : DefaultModelTestBase(std::make_unique<TipsNotificationsRanker>()) {}
  ~TipsNotificationsRankerTest() override = default;
  void SetUp() override { DefaultModelTestBase::SetUp(); }
  void TearDown() override { DefaultModelTestBase::TearDown(); }
};

TEST_F(TipsNotificationsRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputNoResult) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAndroidTipsNotifications, {{"trust_and_safety", "true"}});

  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  std::vector<float> input(kTipsNotificationsRankerFeaturesCount, 1);
  ExpectClassifierResults(input, {});
}

TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputForTrustAndSafety) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAndroidTipsNotifications, {{"trust_and_safety", "true"}});

  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // Test EnhancedSafeBrowsing with all features not being used.
  std::vector<float> input1(kTipsNotificationsRankerFeaturesCount, 0);
  ExpectClassifierResults(input1, {kEnhancedSafeBrowsing});

  // Test QuickDelete with ESB being used.
  std::vector<float> input2(kTipsNotificationsRankerFeaturesCount, 0);
  input2[kEnhancedSafeBrowsingUseCountIdx] = 1;
  input2[kEnhancedSafeBrowsingIsEnabledIdx] = 1;
  ExpectClassifierResults(input2, {kQuickDelete});

  // Test GoogleLens with ESB and QuickDelete being used.
  std::vector<float> input3(kTipsNotificationsRankerFeaturesCount, 0);
  input3[kEnhancedSafeBrowsingUseCountIdx] = 1;
  input3[kEnhancedSafeBrowsingIsEnabledIdx] = 1;
  input3[kQuickDeleteMagicStackShownCountIdx] = 1;
  input3[kQuickDeleteWasEverUsedIdx] = 1;
  ExpectClassifierResults(input3, {kGoogleLens});

  // Test BottomOmnibox with ESB, QuickDelete and Google Lens being used.
  std::vector<float> input4(kTipsNotificationsRankerFeaturesCount, 1);
  input4[kBottomOmniboxIsEnabledIdx] = 0;
  input4[kBottomOmniboxWasEverUsedIdx] = 0;
  ExpectClassifierResults(input4, {kBottomOmnibox});
}

TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputForEssentials) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAndroidTipsNotifications, {{"essential", "true"}});

  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // Test QuickDelete with all features not being used.
  std::vector<float> input1(kTipsNotificationsRankerFeaturesCount, 0);
  ExpectClassifierResults(input1, {kQuickDelete});

  // Test BottomOmnibox with QuickDelete being used.
  std::vector<float> input2(kTipsNotificationsRankerFeaturesCount, 0);
  input2[kQuickDeleteMagicStackShownCountIdx] = 1;
  input2[kQuickDeleteWasEverUsedIdx] = 1;
  ExpectClassifierResults(input2, {kBottomOmnibox});

  // Test ESB with QuickDelete and BottomOmnibox being used.
  std::vector<float> input3(kTipsNotificationsRankerFeaturesCount, 0);
  input3[kQuickDeleteMagicStackShownCountIdx] = 1;
  input3[kQuickDeleteWasEverUsedIdx] = 1;
  input3[kBottomOmniboxIsEnabledIdx] = 1;
  input3[kBottomOmniboxWasEverUsedIdx] = 1;
  ExpectClassifierResults(input3, {kEnhancedSafeBrowsing});

  // Test GoogleLens with QuickDelete, BottomOmnibox and ESB being used.
  std::vector<float> input4(kTipsNotificationsRankerFeaturesCount, 1);
  input4[kGoogleLensNewTabPageUseCountIdx] = 0;
  input4[kGoogleLensMobileOmniboxUseCountIdx] = 0;
  input4[kGoogleLensTasksSurfaceUseCountIdx] = 0;
  ExpectClassifierResults(input4, {kGoogleLens});
}

TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputForNewFeatures) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAndroidTipsNotifications, {{"new_features", "true"}});

  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // Test GoogleLens with all features not being used.
  std::vector<float> input1(kTipsNotificationsRankerFeaturesCount, 0);
  ExpectClassifierResults(input1, {kGoogleLens});

  // Test BottomOmnibox with GoogleLens being used.
  std::vector<float> input2(kTipsNotificationsRankerFeaturesCount, 0);
  input2[kGoogleLensNewTabPageUseCountIdx] = 1;
  input2[kGoogleLensMobileOmniboxUseCountIdx] = 1;
  input2[kGoogleLensTasksSurfaceUseCountIdx] = 1;
  ExpectClassifierResults(input2, {kBottomOmnibox});

  // Test QuickDelete with GoogleLens and BottomOmnibox being used.
  std::vector<float> input3(kTipsNotificationsRankerFeaturesCount, 0);
  input3[kBottomOmniboxIsEnabledIdx] = 1;
  input3[kBottomOmniboxWasEverUsedIdx] = 1;
  input3[kGoogleLensNewTabPageUseCountIdx] = 1;
  input3[kGoogleLensMobileOmniboxUseCountIdx] = 1;
  input3[kGoogleLensTasksSurfaceUseCountIdx] = 1;
  ExpectClassifierResults(input3, {kQuickDelete});

  // Test ESB with GoogleLens, BottomOmnibox and QuickDelete being used.
  std::vector<float> input4(kTipsNotificationsRankerFeaturesCount, 1);
  input4[kEnhancedSafeBrowsingUseCountIdx] = 0;
  input4[kEnhancedSafeBrowsingIsEnabledIdx] = 0;
  ExpectClassifierResults(input4, {kEnhancedSafeBrowsing});
}

}  // namespace segmentation_platform
