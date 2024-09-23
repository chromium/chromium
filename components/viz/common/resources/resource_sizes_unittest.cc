// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

struct TestFormat {
  SharedImageFormat format;
  size_t expected_bytes;
  size_t expected_bytes_aligned;
};

// Modify this constant as per TestFormat variables defined in following tests.
const int kTestFormats = 4;

class ResourceUtilTest : public testing::Test {
 public:
  void TestCheckedWidthInBytes(int width, const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      size_t bytes = ResourceSizes::CheckedWidthInBytes<size_t>(
          width, test_formats[i].format);
      EXPECT_EQ(bytes, test_formats[i].expected_bytes);
    }
  }

  void TestUncheckedWidthInBytes(int width, const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      size_t bytes = ResourceSizes::UncheckedWidthInBytes<size_t>(
          width, test_formats[i].format);
      EXPECT_EQ(bytes, test_formats[i].expected_bytes);
    }
  }

  void TestCheckedSizeInBytes(const gfx::Size& size,
                              const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      size_t bytes = ResourceSizes::CheckedSizeInBytes<size_t>(
          size, test_formats[i].format);
      EXPECT_EQ(bytes, test_formats[i].expected_bytes);
    }
  }
};

TEST_F(ResourceUtilTest, WidthInBytes) {
  // Check bytes for even width.
  int width = 10;
  TestFormat test_formats[] = {
      {SinglePlaneFormat::kRGBA_8888, 40, 40},  // for 32 bits
      {SinglePlaneFormat::kRGBA_4444, 20, 20},  // for 16 bits
      {SinglePlaneFormat::kALPHA_8, 10, 12},    // for 8 bits
      {SinglePlaneFormat::kETC1, 5, 8}          // for 4 bits
  };

  TestCheckedWidthInBytes(width, test_formats);
  TestUncheckedWidthInBytes(width, test_formats);

  // Check bytes for odd width.
  int width_odd = 11;
  TestFormat test_formats_odd[] = {
      {SinglePlaneFormat::kRGBA_8888, 44, 44},  // for 32 bits
      {SinglePlaneFormat::kRGBA_4444, 22, 24},  // for 16 bits
      {SinglePlaneFormat::kALPHA_8, 11, 12},    // for 8 bits
      {SinglePlaneFormat::kETC1, 6, 8}          // for 4 bits
  };

  TestCheckedWidthInBytes(width_odd, test_formats_odd);
  TestUncheckedWidthInBytes(width_odd, test_formats_odd);
}

TEST_F(ResourceUtilTest, SizeInBytes) {
  // Check bytes for even size.
  gfx::Size size(10, 10);
  TestFormat test_formats[] = {
      {SinglePlaneFormat::kRGBA_8888, 400, 400},  // for 32 bits
      {SinglePlaneFormat::kRGBA_4444, 200, 200},  // for 16 bits
      {SinglePlaneFormat::kALPHA_8, 100, 120},    // for 8 bits
      {SinglePlaneFormat::kETC1, 50, 80}          // for 4 bits
  };

  TestCheckedSizeInBytes(size, test_formats);

  // Check bytes for odd size.
  gfx::Size size_odd(11, 11);
  TestFormat test_formats_odd[] = {
      {SinglePlaneFormat::kRGBA_8888, 484, 484},  // for 32 bits
      {SinglePlaneFormat::kRGBA_4444, 242, 264},  // for 16 bits
      {SinglePlaneFormat::kALPHA_8, 121, 132},    // for 8 bits
      {SinglePlaneFormat::kETC1, 66, 88}          // for 4 bits
  };

  TestCheckedSizeInBytes(size_odd, test_formats_odd);
}

TEST_F(ResourceUtilTest, SizeInBytesOverflow) {
  gfx::Size size(10, 10);
  // 10 * 16 * 10 = 1600 bits, overflows in char, but fits in int.
  signed char ignored_char;
  EXPECT_FALSE(ResourceSizes::MaybeSizeInBytes<signed char>(
      size, SinglePlaneFormat::kRGBA_4444, &ignored_char));
  int ignored_int;
  EXPECT_TRUE(ResourceSizes::MaybeSizeInBytes<int>(
      size, SinglePlaneFormat::kRGBA_4444, &ignored_int));
}

TEST_F(ResourceUtilTest, WidthOverflowDoesNotCrash) {
  gfx::Size size(0x20000000, 1);
  // 0x20000000 * 4 = 0x80000000 which overflows int. Should return false, not
  // crash.
  int bytes;
  EXPECT_FALSE(ResourceSizes::MaybeSizeInBytes<int>(
      size, SinglePlaneFormat::kBGRA_8888, &bytes));
}

// Checks that we do not incorrectly indicate that a size has overflowed when
// only the size in bits overflows, but not the size in bytes.
TEST_F(ResourceUtilTest, SizeInBitsOverflowBytesOk) {
  gfx::Size size(10000, 10000);
  // 8192 * 8192 * 32 = 0x80000000, overflows int.
  // Bytes are /8 and do not overflow.
  int ignored;
  EXPECT_TRUE(ResourceSizes::MaybeSizeInBytes<int>(
      size, SinglePlaneFormat::kBGRA_8888, &ignored));
}

// Checks that we correctly identify overflow in cases caused by rounding.
TEST_F(ResourceUtilTest, RoundingOverflows) {
  gfx::Size size(0x1FFFFFFF, 1);
  // 0x1FFFFFFF * 4 = 0x7FFFFFFC. Will overflow when rounded up.
  int ignored;
  EXPECT_FALSE(ResourceSizes::MaybeSizeInBytes<int>(
      size, SinglePlaneFormat::kETC1, &ignored));
}

}  // namespace
}  // namespace viz
