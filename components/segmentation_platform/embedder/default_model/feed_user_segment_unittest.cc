// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

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

  std::string subsegment_key = GetSubsegmentKey(kFeedUserSegmentationKey);
  ModelProvider::Request input(11, 0);
  ExecuteWithInputAndCheckSubsegmentName<FeedUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"NoNTPOrHomeOpened");

  input[1] = 3;
  input[2] = 2;
  ExecuteWithInputAndCheckSubsegmentName<FeedUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"UsedNtpWithoutModules");

  input[0] = 3;
  ExecuteWithInputAndCheckSubsegmentName<FeedUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"MvtOnly");

  input[8] = 3;
  ExecuteWithInputAndCheckSubsegmentName<FeedUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"NtpAndFeedEngagedSimple");

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform
