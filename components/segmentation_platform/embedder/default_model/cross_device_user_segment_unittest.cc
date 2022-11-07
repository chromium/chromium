// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

class CrossDeviceUserModelTest : public DefaultModelTestBase {
 public:
  CrossDeviceUserModelTest()
      : DefaultModelTestBase(std::make_unique<CrossDeviceUserSegment>()) {}
  ~CrossDeviceUserModelTest() override = default;
};

TEST_F(CrossDeviceUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(CrossDeviceUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  ModelProvider::Request input(4, 0);
  std::string subsegment_key = GetSubsegmentKey(kCrossDeviceUserKey);

  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"NoCrossDeviceUsage");

  input[0] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"CrossDeviceOther");

  input[1] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"CrossDeviceMobile");

  input[1] = 0;
  input[2] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"CrossDeviceDesktop");

  input[2] = 0;
  input[3] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"CrossDeviceTablet");

  input[1] = 2;
  input[2] = 2;
  input[3] = 0;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key,
      /*sub_segment_name=*/"CrossDeviceMobileAndDesktop");

  input[2] = 0;
  input[3] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"CrossDeviceMobileAndTablet");

  input[1] = 0;
  input[2] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key,
      /*sub_segment_name=*/"CrossDeviceDesktopAndTablet");

  input[1] = 2;
  ExecuteWithInputAndCheckSubsegmentName<CrossDeviceUserSegment>(
      input, subsegment_key, /*sub_segment_name=*/"CrossDeviceAllDeviceTypes");

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform