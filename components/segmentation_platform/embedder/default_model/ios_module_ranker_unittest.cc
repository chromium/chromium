// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/ios_module_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

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

  std::vector<float> input(35, 0);
  input[30] = -1;  // mvt_freshness
  input[31] = -1;  // shortcuts_freshness
  input[32] = -1;  // safety_check_freshness
  input[33] = -1;  // tab_resumption_freshness
  input[34] = -1;  // parcel_tracking_freshness
  ExpectClassifierResults(input, {kMostVisitedTiles, kShortcuts, kSafetyCheck,
                                  kTabResumption, kParcelTracking});
}

TEST_F(IosModuleRankerTest, ExecuteModelWithInputForAllModules) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(35, 0);
  input[6] = 3.0;    // mvt_engagement
  input[7] = 11.0;   // mvt_impression
  input[8] = 4.0;    // shortcuts_engagement
  input[9] = 2.0;    // shortcuts_impression
  input[10] = 1.0;   // safety_check_engagement
  input[11] = 1.0;   // safety_check_impression
  input[24] = 3.0;   // tab_resumption_engagement
  input[25] = 11.0;  // tab_resumption_impression
  input[28] = 3.0;   // parcel_tracking_engagement
  input[29] = 11.0;  // parcel_tracking_impression

  input[30] = -1;  // mvt_freshness
  input[31] = -1;  // shortcuts_freshness
  input[32] = -1;  // safety_check_freshness
  input[33] = -1;  // tab_resumption_freshness
  input[34] = -1;  // parcel_tracking_freshness
  ExpectClassifierResults(input, {kMostVisitedTiles, kShortcuts, kTabResumption,
                                  kSafetyCheck, kParcelTracking});
}

TEST_F(IosModuleRankerTest, ExecuteModelWithFreshnessInputOnly) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(35, 0);
  input[30] = 0;  // mvt_freshness
  input[31] = 0;  // shortcuts_freshness
  input[32] = 0;  // safety_check_freshness
  input[33] = 0;  // tab_resumption_freshness
  input[34] = 0;  // parcel_tracking_freshness
  ExpectClassifierResults(input, {kParcelTracking, kSafetyCheck, kShortcuts,
                                  kMostVisitedTiles, kTabResumption});

  input[30] = 1;  // mvt_freshness
  input[31] = 1;  // shortcuts_freshness
  input[32] = 2;  // safety_check_freshness
  input[33] = 2;  // tab_resumption_freshness
  input[34] = 1;  // parcel_tracking_freshness
  ExpectClassifierResults(input, {kParcelTracking, kSafetyCheck, kShortcuts,
                                  kMostVisitedTiles, kTabResumption});
}

}  // namespace segmentation_platform
