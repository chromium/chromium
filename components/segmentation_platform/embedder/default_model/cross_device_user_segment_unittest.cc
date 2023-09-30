// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

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

  ExpectClassifierResults(input, {kNoCrossDeviceUsage});

  input[0] = 2;
  ExpectClassifierResults(input, {kCrossDeviceOther});

  input[1] = 2;
  ExpectClassifierResults(input, {kCrossDeviceMobile});

  input[1] = 0;
  input[2] = 2;
  ExpectClassifierResults(input, {kCrossDeviceDesktop});

  input[2] = 0;
  input[3] = 2;
  ExpectClassifierResults(input, {kCrossDeviceTablet});

  input[1] = 2;
  input[2] = 2;
  input[3] = 0;
  ExpectClassifierResults(input, {kCrossDeviceMobileAndDesktop});

  input[2] = 0;
  input[3] = 2;
  ExpectClassifierResults(input, {kCrossDeviceMobileAndTablet});

  input[1] = 0;
  input[2] = 2;
  ExpectClassifierResults(input, {kCrossDeviceDesktopAndTablet});

  input[1] = 2;
  ExpectClassifierResults(input, {kCrossDeviceAllDeviceTypes});

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform