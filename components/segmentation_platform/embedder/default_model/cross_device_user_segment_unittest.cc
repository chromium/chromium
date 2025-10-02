// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

using Feature = CrossDeviceUserSegment::Feature;
using Label = CrossDeviceUserSegment::Label;

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

  ModelProvider::Request input(Feature::kFeatureCount, 0);

  ExpectClassifierResults(input, {kNoCrossDeviceUsage});

  input[Feature::kFeatureDeviceCount] = 2;
  ExpectClassifierResults(input, {kCrossDeviceOther});

  input[Feature::kFeatureDeviceCountPhone] = 2;
  ExpectClassifierResults(input, {kCrossDeviceMobile});

  input[Feature::kFeatureDeviceCountPhone] = 0;
  input[Feature::kFeatureDeviceCountDesktop] = 2;
  ExpectClassifierResults(input, {kCrossDeviceDesktop});

  input[Feature::kFeatureDeviceCountDesktop] = 0;
  input[Feature::kFeatureDeviceCountTablet] = 2;
  ExpectClassifierResults(input, {kCrossDeviceTablet});

  input[Feature::kFeatureDeviceCountPhone] = 2;
  input[Feature::kFeatureDeviceCountDesktop] = 2;
  input[Feature::kFeatureDeviceCountTablet] = 0;
  ExpectClassifierResults(input, {kCrossDeviceMobileAndDesktop});

  input[Feature::kFeatureDeviceCountDesktop] = 0;
  input[Feature::kFeatureDeviceCountTablet] = 2;
  ExpectClassifierResults(input, {kCrossDeviceMobileAndTablet});

  input[Feature::kFeatureDeviceCountPhone] = 0;
  input[Feature::kFeatureDeviceCountDesktop] = 2;
  ExpectClassifierResults(input, {kCrossDeviceDesktopAndTablet});

  input[Feature::kFeatureDeviceCountPhone] = 2;
  ExpectClassifierResults(input, {kCrossDeviceAllDeviceTypes});

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform
