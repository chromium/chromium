// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class ResourceFormatUtilTest : public testing::Test {
 public:
  void TestToClosestSkColorType(std::vector<SkColorType> expected_types,
                                SharedImageFormat format,
                                bool gpu_compositing) {
    ASSERT_EQ(format.NumberOfPlanes(), static_cast<int>(expected_types.size()));
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      EXPECT_EQ(expected_types[plane_index],
                ToClosestSkColorType(gpu_compositing, format, plane_index));
    }
  }
};

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeMultiPlaneYUVBiplanar8bit) {
  // 8-bit 4:2:0 Y_UV biplanar format (YUV_420_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV12;
  std::vector<SkColorType> expected_types = {kAlpha_8_SkColorType,
                                             kR8G8_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeMultiPlaneYVU) {
  // 8-bit 4:2:0 Y_V_U format (YVU_420)
  SharedImageFormat format = MultiPlaneFormat::kYV12;
  std::vector<SkColorType> expected_types = {
      kAlpha_8_SkColorType, kAlpha_8_SkColorType, kAlpha_8_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeMultiPlaneP010) {
  // 10-bit 4:2:0 Y_UV biplanar format (P010)
  SharedImageFormat format = MultiPlaneFormat::kP010;
  std::vector<SkColorType> expected_types = {kA16_unorm_SkColorType,
                                             kR16G16_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeMultiPlaneYUVBiplanar16bit) {
  // 16-bit 4:2:0 Y_UV biplanar format
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k16);
  std::vector<SkColorType> expected_types = {kA16_unorm_SkColorType,
                                             kR16G16_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeMultiPlaneYUVATriplanar) {
  // 16-bit float 4:2:0 Y_UV_A triplanar format
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV_A,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k16F);
  std::vector<SkColorType> expected_types = {kA16_float_SkColorType,
                                             kR16G16_float_SkColorType,
                                             kA16_float_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeSinglePlaneRGBX) {
  // Single planar RGBX_8888
  SharedImageFormat format = SinglePlaneFormat::kRGBX_8888;
  std::vector<SkColorType> expected_types = {kRGB_888x_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeSinglePlaneAlpha) {
  // Single planar ALPHA_8
  SharedImageFormat format = SinglePlaneFormat::kALPHA_8;
  std::vector<SkColorType> expected_types = {kAlpha_8_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeSoftwareRGBX) {
  // Software Compositing.
  // Single planar RGBX_8888
  SharedImageFormat format = SinglePlaneFormat::kRGBX_8888;
  std::vector<SkColorType> expected_types = {kN32_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/false);
}

TEST_F(ResourceFormatUtilTest, ToClosestSkColorTypeSoftwareYUV) {
  // Software Compositing.
  // 10-bit 4:2:0 Y_UV biplanar format (P010)
  SharedImageFormat format = MultiPlaneFormat::kP010;
  std::vector<SkColorType> expected_types = {kN32_SkColorType,
                                             kN32_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/false);
}

}  // namespace
}  // namespace viz
