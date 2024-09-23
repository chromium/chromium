// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format_utils.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorType.h"

namespace viz {
namespace {

class SharedImageFormatUtilsTest : public testing::Test {
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

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneNV12) {
  // 8-bit 4:2:0 Y_UV biplanar format (YUV_420_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV12;
  std::vector<SkColorType> expected_types = {kAlpha_8_SkColorType,
                                             kR8G8_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneNV16) {
  // 8-bit 4:2:2 Y_UV biplanar format (YUV_422_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV16;
  std::vector<SkColorType> expected_types = {kAlpha_8_SkColorType,
                                             kR8G8_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneNV24) {
  // 8-bit 4:4:4 Y_UV biplanar format (YUV_444_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV24;
  std::vector<SkColorType> expected_types = {kAlpha_8_SkColorType,
                                             kR8G8_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneYVU) {
  // 8-bit 4:2:0 Y_V_U format (YVU_420)
  SharedImageFormat format = MultiPlaneFormat::kYV12;
  std::vector<SkColorType> expected_types = {
      kAlpha_8_SkColorType, kAlpha_8_SkColorType, kAlpha_8_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneI420) {
  // 8-bit 4:2:0 Y_U_V format (I420)
  SharedImageFormat format = MultiPlaneFormat::kI420;
  std::vector<SkColorType> expected_types = {
      kAlpha_8_SkColorType, kAlpha_8_SkColorType, kAlpha_8_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneP010) {
  // 10-bit 4:2:0 Y_UV biplanar format (P010)
  SharedImageFormat format = MultiPlaneFormat::kP010;
  std::vector<SkColorType> expected_types = {kA16_unorm_SkColorType,
                                             kR16G16_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneP210) {
  // 10-bit 4:2:2 Y_UV biplanar format (P210)
  SharedImageFormat format = MultiPlaneFormat::kP210;
  std::vector<SkColorType> expected_types = {kA16_unorm_SkColorType,
                                             kR16G16_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeMultiPlaneP410) {
  // 10-bit 4:4:4 Y_UV biplanar format (P410)
  SharedImageFormat format = MultiPlaneFormat::kP410;
  std::vector<SkColorType> expected_types = {kA16_unorm_SkColorType,
                                             kR16G16_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest,
       ToClosestSkColorTypeMultiPlaneYUVBiplanar16bit) {
  // 16-bit 4:2:0 Y_UV biplanar format
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k16);
  std::vector<SkColorType> expected_types = {kA16_unorm_SkColorType,
                                             kR16G16_unorm_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest,
       ToClosestSkColorTypeMultiPlaneYUVATriplanar) {
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

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeSinglePlaneRGBX) {
  // Single planar RGBX_8888
  SharedImageFormat format = SinglePlaneFormat::kRGBX_8888;
  std::vector<SkColorType> expected_types = {kRGB_888x_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeSinglePlaneAlpha) {
  // Single planar ALPHA_8
  SharedImageFormat format = SinglePlaneFormat::kALPHA_8;
  std::vector<SkColorType> expected_types = {kAlpha_8_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/true);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeSoftwareRGBX) {
  // Software Compositing.
  // Single planar RGBX_8888
  SharedImageFormat format = SinglePlaneFormat::kRGBX_8888;
  std::vector<SkColorType> expected_types = {kN32_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/false);
}

TEST_F(SharedImageFormatUtilsTest, ToClosestSkColorTypeSoftwareYUV) {
  // Software Compositing.
  // 10-bit 4:2:0 Y_UV biplanar format (P010)
  SharedImageFormat format = MultiPlaneFormat::kP010;
  std::vector<SkColorType> expected_types = {kN32_SkColorType,
                                             kN32_SkColorType};
  TestToClosestSkColorType(expected_types, format, /*gpu_compositing=*/false);
}

}  // namespace
}  // namespace viz
