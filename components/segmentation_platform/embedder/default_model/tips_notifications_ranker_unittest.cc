// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/tips_notifications_ranker.h"

#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

using TipsFeature = TipsNotificationsRanker::Feature;
using TipsLabel = TipsNotificationsRanker::Label;

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
  std::vector<float> input(TipsFeature::kFeatureCount, 1);
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
  std::vector<float> input1(TipsFeature::kFeatureCount, 0);
  ExpectClassifierResults(input1, {kEnhancedSafeBrowsing});

  // Test QuickDelete with ESB being used.
  std::vector<float> input2(TipsFeature::kFeatureCount, 0);
  input2[TipsFeature::kEnhancedSafeBrowsingUseCountIdx] = 1;
  input2[TipsFeature::kEnhancedSafeBrowsingIsEnabledIdx] = 1;
  ExpectClassifierResults(input2, {kQuickDelete});

  // Test GoogleLens with ESB and QuickDelete being used.
  std::vector<float> input3(TipsFeature::kFeatureCount, 0);
  input3[TipsFeature::kEnhancedSafeBrowsingUseCountIdx] = 1;
  input3[TipsFeature::kEnhancedSafeBrowsingIsEnabledIdx] = 1;
  input3[TipsFeature::kQuickDeleteMagicStackShownCountIdx] = 1;
  input3[TipsFeature::kQuickDeleteWasEverUsedIdx] = 1;
  ExpectClassifierResults(input3, {kGoogleLens});

  // Test BottomOmnibox with ESB, QuickDelete and Google Lens being used.
  std::vector<float> input4(TipsFeature::kFeatureCount, 1);
  input4[TipsFeature::kAllFeatureTipsShownCountIdx] = 0;
  input4[TipsFeature::kBottomOmniboxIsEnabledIdx] = 0;
  input4[TipsFeature::kBottomOmniboxWasEverUsedIdx] = 0;
  input4[TipsFeature::kBottomOmniboxTipShownIdx] = 0;
  ExpectClassifierResults(input4, {kBottomOmnibox});

  // Test AllFeatureTipsShownCount blocks scheduling notifications.
  std::vector<float> input5(TipsFeature::kFeatureCount, 0);
  input5[TipsFeature::kAllFeatureTipsShownCountIdx] = 1;
  ExpectClassifierResults(input5, {});

  // Test TipShown blocks scheduling ESB as first eligible.
  std::vector<float> input6(TipsFeature::kFeatureCount, 0);
  input6[TipsFeature::kEnhancedSafeBrowsingTipShownIdx] = 1;
  ExpectClassifierResults(input6, {kQuickDelete});
}

TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputForEssentials) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAndroidTipsNotifications, {{"essential", "true"}});

  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // Test QuickDelete with all features not being used.
  std::vector<float> input1(TipsFeature::kFeatureCount, 0);
  ExpectClassifierResults(input1, {kQuickDelete});

  // Test BottomOmnibox with QuickDelete being used.
  std::vector<float> input2(TipsFeature::kFeatureCount, 0);
  input2[TipsFeature::kQuickDeleteMagicStackShownCountIdx] = 1;
  input2[TipsFeature::kQuickDeleteWasEverUsedIdx] = 1;
  ExpectClassifierResults(input2, {kBottomOmnibox});

  // Test ESB with QuickDelete and BottomOmnibox being used.
  std::vector<float> input3(TipsFeature::kFeatureCount, 0);
  input3[TipsFeature::kQuickDeleteMagicStackShownCountIdx] = 1;
  input3[TipsFeature::kQuickDeleteWasEverUsedIdx] = 1;
  input3[TipsFeature::kBottomOmniboxIsEnabledIdx] = 1;
  input3[TipsFeature::kBottomOmniboxWasEverUsedIdx] = 1;
  ExpectClassifierResults(input3, {kEnhancedSafeBrowsing});

  // Test GoogleLens with QuickDelete, BottomOmnibox and ESB being used.
  std::vector<float> input4(TipsFeature::kFeatureCount, 1);
  input4[TipsFeature::kAllFeatureTipsShownCountIdx] = 0;
  input4[TipsFeature::kGoogleLensNewTabPageUseCountIdx] = 0;
  input4[TipsFeature::kGoogleLensMobileOmniboxUseCountIdx] = 0;
  input4[TipsFeature::kGoogleLensTasksSurfaceUseCountIdx] = 0;
  input4[TipsFeature::kGoogleLensTipsNotificationsUseCountIdx] = 0;
  input4[TipsFeature::kGoogleLensTipShownIdx] = 0;
  ExpectClassifierResults(input4, {kGoogleLens});

  // Test AllFeatureTipsShownCount blocks scheduling notifications.
  std::vector<float> input5(TipsFeature::kFeatureCount, 0);
  input5[TipsFeature::kAllFeatureTipsShownCountIdx] = 1;
  ExpectClassifierResults(input5, {});

  // Test TipShown blocks scheduling Quick Delete as first eligible.
  std::vector<float> input6(TipsFeature::kFeatureCount, 0);
  input6[TipsFeature::kQuickDeleteTipShownIdx] = 1;
  ExpectClassifierResults(input6, {kBottomOmnibox});
}

TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputForNewFeatures) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAndroidTipsNotifications, {{"new_features", "true"}});

  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // Test GoogleLens with all features not being used.
  std::vector<float> input1(TipsFeature::kFeatureCount, 0);
  ExpectClassifierResults(input1, {kGoogleLens});

  // Test BottomOmnibox with GoogleLens being used.
  std::vector<float> input2(TipsFeature::kFeatureCount, 0);
  input2[TipsFeature::kGoogleLensNewTabPageUseCountIdx] = 1;
  input2[TipsFeature::kGoogleLensMobileOmniboxUseCountIdx] = 1;
  input2[TipsFeature::kGoogleLensTasksSurfaceUseCountIdx] = 1;
  input2[TipsFeature::kGoogleLensTipsNotificationsUseCountIdx] = 1;
  ExpectClassifierResults(input2, {kBottomOmnibox});

  // Test QuickDelete with GoogleLens and BottomOmnibox being used.
  std::vector<float> input3(TipsFeature::kFeatureCount, 0);
  input3[TipsFeature::kBottomOmniboxIsEnabledIdx] = 1;
  input3[TipsFeature::kBottomOmniboxWasEverUsedIdx] = 1;
  input3[TipsFeature::kGoogleLensNewTabPageUseCountIdx] = 1;
  input3[TipsFeature::kGoogleLensMobileOmniboxUseCountIdx] = 1;
  input3[TipsFeature::kGoogleLensTasksSurfaceUseCountIdx] = 1;
  input3[TipsFeature::kGoogleLensTipsNotificationsUseCountIdx] = 1;
  ExpectClassifierResults(input3, {kQuickDelete});

  // Test ESB with GoogleLens, BottomOmnibox and QuickDelete being used.
  std::vector<float> input4(TipsFeature::kFeatureCount, 1);
  input4[TipsFeature::kAllFeatureTipsShownCountIdx] = 0;
  input4[TipsFeature::kEnhancedSafeBrowsingUseCountIdx] = 0;
  input4[TipsFeature::kEnhancedSafeBrowsingIsEnabledIdx] = 0;
  input4[TipsFeature::kEnhancedSafeBrowsingTipShownIdx] = 0;
  ExpectClassifierResults(input4, {kEnhancedSafeBrowsing});

  // Test AllFeatureTipsShownCount blocks scheduling notifications.
  std::vector<float> input5(TipsFeature::kFeatureCount, 0);
  input5[TipsFeature::kAllFeatureTipsShownCountIdx] = 1;
  ExpectClassifierResults(input5, {});

  // Test TipShown blocks scheduling Google Lens as first eligible.
  std::vector<float> input6(TipsFeature::kFeatureCount, 0);
  input6[TipsFeature::kGoogleLensTipShownIdx] = 1;
  ExpectClassifierResults(input6, {kBottomOmnibox});
}

}  // namespace segmentation_platform
