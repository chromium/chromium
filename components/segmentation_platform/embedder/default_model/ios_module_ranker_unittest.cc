// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/ios_module_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

using Feature = IosModuleRanker::Feature;

class IosModuleRankerTest : public DefaultModelTestBase {
 public:
  IosModuleRankerTest()
      : DefaultModelTestBase(std::make_unique<IosModuleRanker>()) {}
  ~IosModuleRankerTest() override = default;

  void SetUp() override { DefaultModelTestBase::SetUp(); }

  void TearDown() override { DefaultModelTestBase::TearDown(); }
};

TEST_F(IosModuleRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(IosModuleRankerTest, ExecuteModelWithInputForDefaultOrder) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(Feature::kFeatureCount, 0);

  input[Feature::kFeatureMostVisitedTilesFreshness] = -1;
  input[Feature::kFeatureShortcutsFreshness] = -1;
  input[Feature::kFeatureSafetyCheckFreshness] = -1;
  input[Feature::kFeatureTabResumptionFreshness] = -1;
  input[Feature::kFeatureParcelTrackingFreshness] = -1;
  input[Feature::kFeatureShopCardFreshness] = -1;

  ExpectClassifierResults(input, {kMostVisitedTiles, kShortcuts, kSafetyCheck,
                                  kTabResumption, kParcelTracking, kShopCard});
}

TEST_F(IosModuleRankerTest, ExecuteModelWithInputForAllModules) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(Feature::kFeatureCount, 0);
  input[Feature::kFeatureMVTClick28Days] = 3.0;
  input[Feature::kFeatureMVTImpression28Days] = 11.0;
  input[Feature::kFeatureShortcutsClick28Days] = 4.0;
  input[Feature::kFeatureShortcutsImpression28Days] = 2.0;
  input[Feature::kFeatureSafetyCheckClick28Days] = 1.0;
  input[Feature::kFeatureSafetyCheckImpression28Days] = 1.0;
  input[Feature::kFeatureTabResumptionClick28Days] = 3.0;
  input[Feature::kFeatureTabResumptionImpression28Days] = 11.0;
  input[Feature::kFeatureParcelTrackingClick28Days] = 3.0;
  input[Feature::kFeatureParcelTrackingImpression28Days] = 11.0;
  input[Feature::kFeatureShopCardClick28Days] = 3.0;
  input[Feature::kFeatureShopCardImpression28Days] = 11.0;

  input[Feature::kFeatureMostVisitedTilesFreshness] = -1;
  input[Feature::kFeatureShortcutsFreshness] = -1;
  input[Feature::kFeatureSafetyCheckFreshness] = -1;
  input[Feature::kFeatureTabResumptionFreshness] = -1;
  input[Feature::kFeatureParcelTrackingFreshness] = -1;
  input[Feature::kFeatureShopCardFreshness] = -1;

  ExpectClassifierResults(input, {kMostVisitedTiles, kShortcuts, kTabResumption,
                                  kSafetyCheck, kShopCard, kParcelTracking});
}

TEST_F(IosModuleRankerTest, ExecuteModelWithFreshnessInputOnly) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(Feature::kFeatureCount, 0);
  input[Feature::kFeatureMostVisitedTilesFreshness] = 0;
  input[Feature::kFeatureShortcutsFreshness] = 0;
  input[Feature::kFeatureSafetyCheckFreshness] = 0;
  input[Feature::kFeatureTabResumptionFreshness] = 0;
  input[Feature::kFeatureParcelTrackingFreshness] = 0;
  input[Feature::kFeatureShopCardFreshness] = 0;

  ExpectClassifierResults(input,
                          {kParcelTracking, kSafetyCheck, kShopCard, kShortcuts,
                           kMostVisitedTiles, kTabResumption});

  input[Feature::kFeatureMostVisitedTilesFreshness] = 1;
  input[Feature::kFeatureShortcutsFreshness] = 1;
  input[Feature::kFeatureSafetyCheckFreshness] = 2;
  input[Feature::kFeatureTabResumptionFreshness] = 2;
  input[Feature::kFeatureParcelTrackingFreshness] = 1;
  input[Feature::kFeatureShopCardFreshness] = 1;

  ExpectClassifierResults(input,
                          {kParcelTracking, kSafetyCheck, kShopCard, kShortcuts,
                           kMostVisitedTiles, kTabResumption});
}

}  // namespace segmentation_platform
