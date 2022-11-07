// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class PowerUserModelTest : public DefaultModelTestBase {
 public:
  PowerUserModelTest()
      : DefaultModelTestBase(std::make_unique<PowerUserSegment>()) {}
  ~PowerUserModelTest() override = default;
};

TEST_F(PowerUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(PowerUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::string subsegment_key = GetSubsegmentKey(kPowerUserKey);
  ModelProvider::Request input(27, 0);
  ExecuteWithInputAndCheckSubsegmentName<PowerUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"None");

  input[1] = 3;    // download
  input[8] = 4;    // share
  input[10] = 4;   // bookmarks
  input[11] = 20;  // voice
  ExecuteWithInputAndCheckSubsegmentName<PowerUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"Low");

  input[12] = 2;  // cast
  input[15] = 5;  // autofill
  input[22] = 6;  // media picker
  ExecuteWithInputAndCheckSubsegmentName<PowerUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"Medium");

  input[26] = 20 * 60 * 1000;  // 20 min session
  input[17] = 60000;           // 60 sec audio output
  input[23] = 50000;           // 50KB upload
  ExecuteWithInputAndCheckSubsegmentName<PowerUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"High");

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform
