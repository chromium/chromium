// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/optimization_target_segmentation_dummy.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class OptimizationTargetSegmentationDummyTest : public DefaultModelTestBase {
 public:
  OptimizationTargetSegmentationDummyTest()
      : DefaultModelTestBase(
            std::make_unique<OptimizationTargetSegmentationDummy>()) {}
  ~OptimizationTargetSegmentationDummyTest() override = default;

  void SetUp() override { DefaultModelTestBase::SetUp(); }

  void TearDown() override { DefaultModelTestBase::TearDown(); }
};

TEST_F(OptimizationTargetSegmentationDummyTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(OptimizationTargetSegmentationDummyTest, ExecuteModelWithInput) {
  ExpectExecutionWithInput(/*inputs=*/{1}, /*expected_error=*/false,
                           /*expected_result=*/{1});
}

}  // namespace segmentation_platform
