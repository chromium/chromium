// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_tier_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

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
  ExpectClassifierResults(/*input=*/{/*8GB=*/8192, 10, 371},
                          {kDeviceTierSegmentLabelHigh});
  ExpectClassifierResults(/*input=*/{/*6GB=*/6144, 11, 374},
                          {kDeviceTierSegmentLabelHigh});

  // Medium-end devices
  ExpectClassifierResults(/*input=*/{/*3GB=*/3072, 10, 10},
                          {kDeviceTierSegmentLabelMedium});
  ExpectClassifierResults(/*input=*/{/*6GB=*/6144, 10, 670},
                          {kDeviceTierSegmentLabelMedium});
  ExpectClassifierResults(/*input=*/{/*6GB=*/6144, 11, 370},
                          {kDeviceTierSegmentLabelMedium});

  // All other devices
  ExpectClassifierResults(/*input=*/{/*2GB=*/2048, 9, 10},
                          {kDeviceTierSegmentLabelLow});
  ExpectClassifierResults(/*input=*/{/*5GB=*/5120, 9, 10},
                          {kDeviceTierSegmentLabelLow});

  // Not a device.
  ExpectClassifierResults(/*input=*/{0, 0, 0}, {kDeviceTierSegmentLabelNone});
}

}  // namespace segmentation_platform
