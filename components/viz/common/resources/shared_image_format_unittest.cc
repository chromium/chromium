// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format.h"

#include <limits>
#include <vector>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr gfx::Size kDefaultSize(100, 100);
constexpr gfx::Size kOddSize(9, 9);

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

TEST_F(SharedImageFormatTest, MultiPlaneNV12) {
  // 8-bit 4:2:0 Y_UV biplanar format (YUV_420_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV12;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  // UV: 100 bytes per row (2 channel * 1 byte * 50 width) * 50 rows = 5000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 15000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // UV: 10 bytes per row (2 channels * 1 byte * 5 width) * 5 rows = 50 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 131u);
}

TEST_F(SharedImageFormatTest, MultiPlaneNV16) {
  // 8-bit 4:2:2 Y_UV biplanar format (YUV_422_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV16;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  // UV: 100 bytes per row (2 channel * 1 byte * 50 width) * 100 rows = 10000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 20000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // UV: 10 bytes per row (2 channels * 1 byte * 5 width) * 9 rows = 90 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 171u);
}

TEST_F(SharedImageFormatTest, MultiPlaneNV24) {
  // 8-bit 4:4:4 Y_UV biplanar format (YUV_444_BIPLANAR)
  SharedImageFormat format = MultiPlaneFormat::kNV24;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  // UV: 200 bytes per row (2 channel * 1 byte * 100 width) * 100 rows = 20000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 30000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // UV: 20 bytes per row (2 channels * 1 byte * 9 width) * 9 rows = 162
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 243u);
}

TEST_F(SharedImageFormatTest, MultiPlaneYVU) {
  // 8-bit 4:2:0 Y_V_U format (YVU_420)
  SharedImageFormat format = MultiPlaneFormat::kYV12;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 15000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // V: 5 bytes per row (1 channels * 1 byte * 5 width) * 5 rows = 25 bytes.
  // U: 5 bytes per row (1 channels * 1 byte * 5 width) * 5 rows = 25 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 131u);
}

TEST_F(SharedImageFormatTest, MultiPlaneI420) {
  // 8-bit 4:2:0 Y_U_V format (I420)
  SharedImageFormat format = MultiPlaneFormat::kI420;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 15000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // U: 5 bytes per row (1 channels * 1 byte * 5 width) * 5 rows = 25 bytes.
  // V: 5 bytes per row (1 channels * 1 byte * 5 width) * 5 rows = 25 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 131u);
}

TEST_F(SharedImageFormatTest, MultiPlaneI422) {
  // 8-bit 4:2:2 Y_U_V format (I422)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V,
                                    SharedImageFormat::Subsampling::k422,
                                    SharedImageFormat::ChannelFormat::k8);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  // U: 50 bytes per row (1 channel * 1 byte * 50 width) * 100 rows = 5000
  // bytes.
  // V: 50 bytes per row (1 channel * 1 byte * 50 width) * 100 rows = 5000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 20000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // U: 5 bytes per row (1 channel * 1 byte * 5 width) * 9 rows = 45 bytes.
  // V: 5 bytes per row (1 channel * 1 byte * 5 width) * 9 rows = 45 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 171u);
}

TEST_F(SharedImageFormatTest, MultiPlaneI444) {
  // 8-bit 4:2:2 Y_U_V format (I444)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V,
                                    SharedImageFormat::Subsampling::k444,
                                    SharedImageFormat::ChannelFormat::k8);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  // U: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  // V: 100 bytes per row (1 channel * 1 byte * 100 width) * 100 rows = 10000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 30000u);

  // Y: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // U: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  // V: 9 bytes per row (1 channel * 1 byte * 9 width) * 9 rows = 81 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 243u);
}

TEST_F(SharedImageFormatTest, MultiPlaneP010) {
  // 10-bit 4:2:0 Y_UV biplanar format (P010)
  SharedImageFormat format = MultiPlaneFormat::kP010;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 200 bytes per row (1 channel * 2 byte * 100 width) * 100 rows = 20000
  // bytes.
  // UV: 200 bytes per row (2 channel * 2 byte * 50 width) * 50 rows = 10000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 30000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // UV: 20 bytes per row (2 channels * 2 bytes * 5 width) * 5 rows = 100 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 262u);
}

TEST_F(SharedImageFormatTest, MultiPlaneP210) {
  // 10-bit 4:2:2 Y_UV biplanar format (P210)
  SharedImageFormat format = MultiPlaneFormat::kP210;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 200 bytes per row (1 channel * 2 byte * 100 width) * 100 rows = 20000
  // bytes.
  // UV: 200 bytes per row (2 channel * 2 byte * 50 width) * 100 rows = 20000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 40000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // UV: 20 bytes per row (2 channels * 2 bytes * 5 width) * 9 rows = 180
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 342u);
}

TEST_F(SharedImageFormatTest, MultiPlaneP410) {
  // 10-bit 4:4:4 Y_UV biplanar format (P410)
  SharedImageFormat format = MultiPlaneFormat::kP410;
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 2};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 200 bytes per row (1 channel * 2 byte * 100 width) * 100 rows = 20000
  // bytes.
  // UV: 400 bytes per row (2 channel * 2 byte * 100 width) * 100 rows = 40000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 60000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // UV: 36 bytes per row (2 channels * 2 bytes * 9 width) * 9 rows = 324
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 486u);
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

  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 30000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // UV: 20 bytes per row (2 channels * 2 bytes * 5 width) * 5 rows = 100 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 262u);
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

  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 50000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // UV: 20 bytes per row (2 channels * 2 bytes * 5 width) * 5 rows = 100 bytes.
  // A: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 424u);
}

TEST_F(SharedImageFormatTest, MultiPlaneYUV420P10) {
  // 10-bit float 4:2:0 Y_U_V format (YUV420P10)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V,
                                    SharedImageFormat::Subsampling::k420,
                                    SharedImageFormat::ChannelFormat::k10);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 200 bytes per row (1 channel * 2 bytes * 100 width) * 100 rows = 20000
  // bytes.
  // U: 100 bytes per row (1 channel * 2 bytes * 50 width) * 50 rows = 5000
  // bytes.
  // V: 100 bytes per row (1 channel * 2 bytes * 50 width) * 50 rows = 5000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 30000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // U: 10 bytes per row (1 channel * 2 bytes * 5 width) * 5 rows = 50 bytes.
  // V: 10 bytes per row (1 channel * 2 bytes * 5 width) * 5 rows = 50 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 262u);
}

TEST_F(SharedImageFormatTest, MultiPlaneYUV422P10) {
  // 10-bit float 4:2:2 Y_U_V format (YUV422P10)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V,
                                    SharedImageFormat::Subsampling::k422,
                                    SharedImageFormat::ChannelFormat::k10);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 200 bytes per row (1 channel * 2 bytes * 100 width) * 100 rows = 20000
  // bytes.
  // U: 100 bytes per row (1 channel * 2 bytes * 50 width) * 100 rows = 10000
  // bytes.
  // V: 100 bytes per row (1 channel * 2 bytes * 50 width) * 100 rows = 10000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 40000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // U: 10 bytes per row (1 channel * 2 bytes * 5 width) * 9 rows = 90 bytes.
  // V: 10 bytes per row (1 channel * 2 bytes * 5 width) * 9 rows = 90 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 342u);
}

TEST_F(SharedImageFormatTest, MultiPlaneYUV444P10) {
  // 10-bit float 4:4:4 Y_U_V format (YUV444P10)
  SharedImageFormat format =
      SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V,
                                    SharedImageFormat::Subsampling::k444,
                                    SharedImageFormat::ChannelFormat::k10);
  // Test for NumChannelsInPlane
  std::vector<int> expected_channels = {1, 1, 1};
  TestNumChannelsInPlane(expected_channels, format);

  // Y: 200 bytes per row (1 channel * 2 bytes * 100 width) * 100 rows = 20000
  // bytes.
  // U: 200 bytes per row (1 channel * 2 bytes * 100 width) * 100 rows = 20000
  // bytes.
  // V: 200 bytes per row (1 channel * 2 bytes * 100 width) * 100 rows = 20000
  // bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 60000u);

  // Y: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // U: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  // V: 18 bytes per row (1 channel * 2 bytes * 9 width) * 9 rows = 162 bytes.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 486u);
}

TEST_F(SharedImageFormatTest, SinglePlaneRGBA_8888) {
  auto format = SinglePlaneFormat::kRGBA_8888;
  EXPECT_EQ(1, format.NumberOfPlanes());

  // 4 bytes per pixel.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 40000u);
}

TEST_F(SharedImageFormatTest, SinglePlaneRED_8) {
  auto format = SinglePlaneFormat::kR_8;
  EXPECT_EQ(1, format.NumberOfPlanes());

  // 1 byte per pixel.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 10000u);

  // 9 bytes per row * 9 rows.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 81u);
}

TEST_F(SharedImageFormatTest, SinglePlaneRG_88) {
  auto format = SinglePlaneFormat::kRG_88;
  EXPECT_EQ(1, format.NumberOfPlanes());

  // 2 bytes per pixel.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 20000u);

  // 18 bytes per row * 9 rows.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 162u);
}

TEST_F(SharedImageFormatTest, SinglePlaneETC1) {
  auto format = SinglePlaneFormat::kETC1;
  EXPECT_EQ(1, format.NumberOfPlanes());

  // 4 bits (not bytes) per pixel.
  EXPECT_EQ(format.EstimatedSizeInBytes(kDefaultSize), 5000u);

  // 5 bytes per row (rounded up) * 9 rows.
  EXPECT_EQ(format.EstimatedSizeInBytes(kOddSize), 45u);
}

TEST_F(SharedImageFormatTest, EstimatedSizeInBytesOverflow) {
  auto format = SinglePlaneFormat::kRGBA_F16;

  constexpr gfx::Size max_size(std::numeric_limits<int>::max(),
                               std::numeric_limits<int>::max());

  // MaybeEstimatedSizeInBytes() will return nullopt on overflow.
  EXPECT_FALSE(format.MaybeEstimatedSizeInBytes(max_size).has_value());

  // EstimatedSizeInBytes() will return 0 on overflow.
  EXPECT_EQ(format.EstimatedSizeInBytes(max_size), 0u);

  // VerifySizeInBytes() should return false on overflow.
  EXPECT_FALSE(format.VerifySizeInBytes(max_size));
}

TEST_F(SharedImageFormatTest, PrefersExternalSampler) {
  auto singleplanar_format = SinglePlaneFormat::kRGBA_F16;
  auto multiplanar_format = MultiPlaneFormat::kNV12;
  EXPECT_FALSE(singleplanar_format.PrefersExternalSampler());
  EXPECT_FALSE(multiplanar_format.PrefersExternalSampler());

#if BUILDFLAG(IS_OZONE)
  multiplanar_format.SetPrefersExternalSampler();
  EXPECT_TRUE(multiplanar_format.PrefersExternalSampler());
#endif
}

}  // namespace
}  // namespace viz
