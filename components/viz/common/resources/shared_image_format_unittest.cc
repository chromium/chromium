// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class SharedImageFormatTest : public testing::Test {
 public:
  void TestNumChannelsInPlane(std::vector<int> expected_channels,
                              SharedImageFormat format) {
    ASSERT_EQ(format.NumberOfPlanes(),
              static_cast<int>(expected_channels.size()));
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      EXPECT_TRUE(format.IsValidPlaneIndex(plane_index));
      EXPECT_EQ(expected_channels[plane_index],
                format.NumChannelsInPlane(plane_index));
    }
  }
};

TEST_F(SharedImageFormatTest, MultiPlaneYUVBiplanar8bit) {
  // 8-bit 4:2:0 Y_UV biplanar format (YUV_420_BIPLANAR)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k8);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);
}

TEST_F(SharedImageFormatTest, MultiPlaneYVU) {
  // 8-bit 4:2:0 Y_V_U format (YVU_420)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_V_U,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k8);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);
}

TEST_F(SharedImageFormatTest, MultiPlaneP010) {
  // 10-bit 4:2:0 Y_UV biplanar format (P010)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k10);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);
}

TEST_F(SharedImageFormatTest, MultiPlaneYUVBiplanar16bit) {
  // 16-bit 4:2:0 Y_UV biplanar format
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k16);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);
}

TEST_F(SharedImageFormatTest, MultiPlaneYUVATriplanar) {
  // 16-bit float 4:2:0 Y_UV_A triplanar format
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV_A,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k16F);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2, 1};
  TestNumChannelsInPlane(expected_channels, format);
}

TEST_F(SharedImageFormatTest, SinglePlaneETC1) {
  // Single planar ETC1
  SharedImageFormat format =
      SharedImageFormat::SinglePlane(ResourceFormat::ETC1);
  // Test for NumberOfPlanes.
  EXPECT_EQ(1, format.NumberOfPlanes());
}

}  // namespace
}  // namespace viz
