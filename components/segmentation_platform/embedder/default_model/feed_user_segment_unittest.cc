// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

class FeedUserModelTest : public DefaultModelTestBase {
 public:
  FeedUserModelTest()
      : DefaultModelTestBase(std::make_unique<FeedUserSegment>()) {}
  ~FeedUserModelTest() override = default;
};

TEST_F(FeedUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FeedUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ModelProvider::Request input(11, 0);
  ExpectClassifierResults(input, {kLegacyNegativeLabel});

  input[8] = 3;
  ExpectClassifierResults(
      input, {SegmentIdToHistogramVariant(
                 SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER)});

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform
