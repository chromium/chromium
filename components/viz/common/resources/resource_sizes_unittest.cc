// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/logging.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

struct TestFormat {
  ResourceFormat format;
  size_t expected_bytes;
  size_t expected_bytes_aligned;
};

// Modify this constant as per TestFormat variables defined in following tests.
const int kTestFormats = 4;

class ResourceUtilTest : public testing::Test {
 public:
  void TestVerifyWidthInBytes(int width, const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      EXPECT_TRUE(ResourceSizes::VerifyWidthInBytes<size_t>(
          width, test_formats[i].format));
    }
  }

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

  void TestUncheckedWidthInBytesAligned(int width,
                                        const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      size_t bytes = ResourceSizes::UncheckedWidthInBytesAligned<size_t>(
          width, test_formats[i].format);
      EXPECT_EQ(bytes, test_formats[i].expected_bytes_aligned);
    }
  }

  void TestVerifySizeInBytes(const gfx::Size& size,
                             const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      EXPECT_TRUE(ResourceSizes::VerifySizeInBytes<size_t>(
          size, test_formats[i].format));
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

  void TestUncheckedSizeInBytes(const gfx::Size& size,
                                const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      size_t bytes = ResourceSizes::UncheckedSizeInBytes<size_t>(
          size, test_formats[i].format);
      EXPECT_EQ(bytes, test_formats[i].expected_bytes);
    }
  }

  void TestUncheckedSizeInBytesAligned(const gfx::Size& size,
                                       const TestFormat* test_formats) {
    for (int i = 0; i < kTestFormats; ++i) {
      size_t bytes = ResourceSizes::UncheckedSizeInBytesAligned<size_t>(
          size, test_formats[i].format);
      EXPECT_EQ(bytes, test_formats[i].expected_bytes_aligned);
    }
  }
};

TEST_F(ResourceUtilTest, WidthInBytes) {
  // Check bytes for even width.
  int width = 10;
  TestFormat test_formats[] = {
      {RGBA_8888, 40, 40},  // for 32 bits
      {RGBA_4444, 20, 20},  // for 16 bits
      {ALPHA_8, 10, 12},    // for 8 bits
      {ETC1, 5, 8}          // for 4 bits
  };

  TestVerifyWidthInBytes(width, test_formats);
  TestCheckedWidthInBytes(width, test_formats);
  TestUncheckedWidthInBytes(width, test_formats);
  TestUncheckedWidthInBytesAligned(width, test_formats);

  // Check bytes for odd width.
  int width_odd = 11;
  TestFormat test_formats_odd[] = {
      {RGBA_8888, 44, 44},  // for 32 bits
      {RGBA_4444, 22, 24},  // for 16 bits
      {ALPHA_8, 11, 12},    // for 8 bits
      {ETC1, 6, 8}          // for 4 bits
  };

  TestVerifyWidthInBytes(width_odd, test_formats_odd);
  TestCheckedWidthInBytes(width_odd, test_formats_odd);
  TestUncheckedWidthInBytes(width_odd, test_formats_odd);
  TestUncheckedWidthInBytesAligned(width_odd, test_formats_odd);
}

TEST_F(ResourceUtilTest, SizeInBytes) {
  // Check bytes for even size.
  gfx::Size size(10, 10);
  TestFormat test_formats[] = {
      {RGBA_8888, 400, 400},  // for 32 bits
      {RGBA_4444, 200, 200},  // for 16 bits
      {ALPHA_8, 100, 120},    // for 8 bits
      {ETC1, 50, 80}          // for 4 bits
  };

  TestVerifySizeInBytes(size, test_formats);
  TestCheckedSizeInBytes(size, test_formats);
  TestUncheckedSizeInBytes(size, test_formats);
  TestUncheckedSizeInBytesAligned(size, test_formats);

  // Check bytes for odd size.
  gfx::Size size_odd(11, 11);
  TestFormat test_formats_odd[] = {
      {RGBA_8888, 484, 484},  // for 32 bits
      {RGBA_4444, 242, 264},  // for 16 bits
      {ALPHA_8, 121, 132},    // for 8 bits
      {ETC1, 66, 88}          // for 4 bits
  };

  TestVerifySizeInBytes(size_odd, test_formats_odd);
  TestCheckedSizeInBytes(size_odd, test_formats_odd);
  TestUncheckedSizeInBytes(size_odd, test_formats_odd);
  TestUncheckedSizeInBytesAligned(size_odd, test_formats_odd);
}

TEST_F(ResourceUtilTest, WidthInBytesOverflow) {
  int width = 10;
  // 10 * 16 = 160 bits, overflows in char, but fits in unsigned char.
  EXPECT_FALSE(
      ResourceSizes::VerifyWidthInBytes<signed char>(width, RGBA_4444));
  EXPECT_TRUE(
      ResourceSizes::VerifyWidthInBytes<unsigned char>(width, RGBA_4444));
}

TEST_F(ResourceUtilTest, SizeInBytesOverflow) {
  gfx::Size size(10, 10);
  // 10 * 16 * 10 = 1600 bits, overflows in char, but fits in int.
  EXPECT_FALSE(ResourceSizes::VerifySizeInBytes<signed char>(size, RGBA_4444));
  EXPECT_TRUE(ResourceSizes::VerifySizeInBytes<int>(size, RGBA_4444));
}

TEST_F(ResourceUtilTest, WidthOverflowDoesNotCrash) {
  gfx::Size size(0x20000000, 1);
  // 0x20000000 * 4 = 0x80000000 which overflows int. Should return false, not
  // crash.
  int bytes;
  EXPECT_FALSE(
      ResourceSizes::MaybeWidthInBytes<int>(size.width(), BGRA_8888, &bytes));
  EXPECT_FALSE(ResourceSizes::MaybeSizeInBytes<int>(size, BGRA_8888, &bytes));
}

// Checks that we do not incorrectly indicate that a size has overflowed when
// only the size in bits overflows, but not the size in bytes.
TEST_F(ResourceUtilTest, SizeInBitsOverflowBytesOk) {
  gfx::Size size(10000, 10000);
  // 8192 * 8192 * 32 = 0x80000000, overflows int.
  // Bytes are /8 and do not overflow.
  EXPECT_TRUE(ResourceSizes::VerifySizeInBytes<int>(size, BGRA_8888));
}

// Checks that we correctly identify overflow in cases caused by rounding.
TEST_F(ResourceUtilTest, RoundingOverflows) {
  gfx::Size size(0x1FFFFFFF, 1);
  // 0x1FFFFFFF * 4 = 0x7FFFFFFC. Will overflow when rounded up.
  EXPECT_FALSE(ResourceSizes::VerifySizeInBytes<int>(size, ETC1));
}

}  // namespace
}  // namespace viz
