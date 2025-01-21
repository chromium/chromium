// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/android_home_module_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class AndroidHomeModuleRankerTest : public DefaultModelTestBase {
 public:
  AndroidHomeModuleRankerTest()
      : DefaultModelTestBase(std::make_unique<AndroidHomeModuleRanker>()) {}
  ~AndroidHomeModuleRankerTest() override = default;

  void SetUp() override {
    DefaultModelTestBase::SetUp();
    bool isAndroidHomeModuleRankerV2Enabled = base::FeatureList::IsEnabled(
        features::kSegmentationPlatformAndroidHomeModuleRankerV2);
    input_size = isAndroidHomeModuleRankerV2Enabled ? 12 : 8;
  }

  void TearDown() override { DefaultModelTestBase::TearDown(); }

 protected:
  size_t input_size;
};

TEST_F(AndroidHomeModuleRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(AndroidHomeModuleRankerTest, ExecuteModelWithInputForAllModules) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  std::vector<float> input(input_size, 0);
  ExpectClassifierResults(input, {kPriceChange, kSingleTab,
                                  kTabResumptionForAndroidHome, kSafetyHub});
}

TEST_F(AndroidHomeModuleRankerTest,
       ExecuteModelWithInputForAllModulesWithEngagement) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  std::vector<float> input(input_size, 0);
  input[0] = 1;
  input[2] = 1;
  input[4] = 1;
  input[6] = 1;
  ExpectClassifierResults(input, {kSafetyHub, kPriceChange, kSingleTab,
                                  kTabResumptionForAndroidHome});
}

TEST_F(AndroidHomeModuleRankerTest,
       ExecuteModelWithInputForAllModulesWithImpressions) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  std::vector<float> input(input_size, 0);
  input[1] = 1;
  input[3] = 1;
  input[5] = 1;
  input[7] = 1;
  ExpectClassifierResults(input, {kSingleTab, kTabResumptionForAndroidHome,
                                  kPriceChange, kSafetyHub});
}

}  // namespace segmentation_platform
