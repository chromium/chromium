// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_tier_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

using Feature = DeviceTierSegment::Feature;

class DeviceTierSegmentTest : public DefaultModelTestBase {
 public:
  DeviceTierSegmentTest()
      : DefaultModelTestBase(std::make_unique<DeviceTierSegment>()) {}
  ~DeviceTierSegmentTest() override = default;
};

TEST_F(DeviceTierSegmentTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);
}

TEST_F(DeviceTierSegmentTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*input=*/{}));

  // High-end devices
  ModelProvider::Request input1(Feature::kFeatureCount, 0);
  input1[Feature::kFeatureDeviceRamInMb] = 8192;
  input1[Feature::kFeatureDeviceOsVersionNumber] = 10;
  input1[Feature::kFeatureDevicePpi] = 371;
  ExpectClassifierResults(input1, {kDeviceTierSegmentLabelHigh});

  ModelProvider::Request input2(Feature::kFeatureCount, 0);
  input2[Feature::kFeatureDeviceRamInMb] = 6144;
  input2[Feature::kFeatureDeviceOsVersionNumber] = 11;
  input2[Feature::kFeatureDevicePpi] = 374;
  ExpectClassifierResults(input2, {kDeviceTierSegmentLabelHigh});

  // Medium-end devices
  ModelProvider::Request input3(Feature::kFeatureCount, 0);
  input3[Feature::kFeatureDeviceRamInMb] = 3072;
  input3[Feature::kFeatureDeviceOsVersionNumber] = 10;
  input3[Feature::kFeatureDevicePpi] = 10;
  ExpectClassifierResults(input3, {kDeviceTierSegmentLabelMedium});

  ModelProvider::Request input4(Feature::kFeatureCount, 0);
  input4[Feature::kFeatureDeviceRamInMb] = 6144;
  input4[Feature::kFeatureDeviceOsVersionNumber] = 10;
  input4[Feature::kFeatureDevicePpi] = 670;
  ExpectClassifierResults(input4, {kDeviceTierSegmentLabelMedium});

  ModelProvider::Request input5(Feature::kFeatureCount, 0);
  input5[Feature::kFeatureDeviceRamInMb] = 6144;
  input5[Feature::kFeatureDeviceOsVersionNumber] = 11;
  input5[Feature::kFeatureDevicePpi] = 370;
  ExpectClassifierResults(input5, {kDeviceTierSegmentLabelMedium});

  // All other devices
  ModelProvider::Request input6(Feature::kFeatureCount, 0);
  input6[Feature::kFeatureDeviceRamInMb] = 2048;
  input6[Feature::kFeatureDeviceOsVersionNumber] = 9;
  input6[Feature::kFeatureDevicePpi] = 10;
  ExpectClassifierResults(input6, {kDeviceTierSegmentLabelLow});

  ModelProvider::Request input7(Feature::kFeatureCount, 0);
  input7[Feature::kFeatureDeviceRamInMb] = 5120;
  input7[Feature::kFeatureDeviceOsVersionNumber] = 9;
  input7[Feature::kFeatureDevicePpi] = 10;
  ExpectClassifierResults(input7, {kDeviceTierSegmentLabelLow});

  // Not a device.
  ExpectClassifierResults(ModelProvider::Request(Feature::kFeatureCount, 0),
                          {kDeviceTierSegmentLabelNone});
}

}  // namespace segmentation_platform
