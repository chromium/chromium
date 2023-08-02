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

TEST_F(IosModuleRankerTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  std::vector<float> input(25, 0);

  ExpectClassifierResults(input, {kMostVisitedTiles, kShortcuts, kSafetyCheck});
}

}  // namespace segmentation_platform
