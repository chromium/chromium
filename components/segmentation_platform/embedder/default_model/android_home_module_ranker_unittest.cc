// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/android_home_module_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class AndroidHomeModuleRankerTest : public DefaultModelTestBase {
 public:
  AndroidHomeModuleRankerTest()
      : DefaultModelTestBase(std::make_unique<AndroidHomeModuleRanker>()) {}
  ~AndroidHomeModuleRankerTest() override = default;

  void SetUp() override { DefaultModelTestBase::SetUp(); }

  void TearDown() override { DefaultModelTestBase::TearDown(); }
};

TEST_F(AndroidHomeModuleRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(AndroidHomeModuleRankerTest, ExecuteModelWithInputForAllModules) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(27, 0);
  ExpectClassifierResults(
      input, {kPriceChange, kSingleTab, kTabResumptionForAndroidHome});
}

}  // namespace segmentation_platform
