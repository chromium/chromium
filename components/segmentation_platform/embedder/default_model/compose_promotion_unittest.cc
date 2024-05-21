// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/compose_promotion.h"

#include "components/compose/core/browser/config.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class ComposePromotionTest : public DefaultModelTestBase {
 public:
  ComposePromotionTest()
      : DefaultModelTestBase(std::make_unique<ComposePromotion>()) {}
  ~ComposePromotionTest() override = default;

  void SetUp() override {
    DefaultModelTestBase::SetUp();
    // Force probability (which is compared againsta a random number in the
    // heuristic.
    compose::GetMutableConfigForTesting().proactive_nudge_show_probability =
        0.5;
  }

  void TearDown() override {
    compose::ResetConfigForTesting();
    DefaultModelTestBase::TearDown();
  }
};

TEST_F(ComposePromotionTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(ComposePromotionTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ExpectClassifierResults(/*inputs=*/{0.49},
                          {segmentation_platform::kComposePrmotionLabelShow});
  ExpectClassifierResults(
      /*inputs=*/{0.51},
      {segmentation_platform::kComposePrmotionLabelDontShow});
}

}  // namespace segmentation_platform
