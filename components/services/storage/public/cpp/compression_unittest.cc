// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/compression.h"

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/test/gmock_expected_support.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/snappy/src/snappy.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
#include "third_party/zstd/src/lib/zstd.h"
#endif

namespace storage {
namespace {

std::vector<uint8_t> ToBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// Verifies that a large value is compressed and decompressed correctly through
// a round-trip of `Compress()` and `Decompress()`.
TEST(CompressionTest, LargeCompressibleValueRoundTrip) {
  std::vector<uint8_t> original(1024, 'A');

  CompressedValue compressed = Compress(original);
  EXPECT_NE(compressed.type, CompressionType::kUncompressed);
  EXPECT_LT(compressed.data.size(), original.size());

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> decompressed,
                       Decompress(std::move(compressed)));
  EXPECT_EQ(decompressed, original);
}

// Verifies that small values below the compression threshold are returned
// uncompressed.
TEST(CompressionTest, SmallValueNotCompressed) {
  std::vector<uint8_t> small_value = ToBytes("small");

  CompressedValue compressed = Compress(small_value);
  EXPECT_EQ(compressed.type, CompressionType::kUncompressed);
  EXPECT_EQ(compressed.data, small_value);
}

// Verifies that a large value that doesn't compress well is returned
// uncompressed.
TEST(CompressionTest, LargeIncompressibleValueNotCompressed) {
  std::vector<uint8_t> incompressible(256);
  for (size_t i = 0; i < incompressible.size(); ++i) {
    incompressible[i] = static_cast<uint8_t>(i);
  }

  CompressedValue compressed = Compress(incompressible);
  EXPECT_EQ(compressed.type, CompressionType::kUncompressed);
  EXPECT_EQ(compressed.data, incompressible);
}

// Verifies that uncompressed data passes through `Decompress()` unchanged.
TEST(CompressionTest, DecompressUncompressedData) {
  std::vector<uint8_t> original = ToBytes("hello world");

  ASSERT_OK_AND_ASSIGN(std::vector<uint8_t> result,
                       Decompress({CompressionType::kUncompressed, original}));
  EXPECT_EQ(result, original);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

// Verifies that ZSTD-compressed data can be decompressed correctly.
TEST(CompressionTest, DecompressZstdData) {
  std::vector<uint8_t> original(256, 'Z');

  size_t max_compressed = ZSTD_compressBound(original.size());
  std::vector<uint8_t> compressed(max_compressed);
  size_t compressed_size =
      ZSTD_compress(compressed.data(), compressed.size(), original.data(),
                    original.size(), /*compressionLevel=*/-4);
  ASSERT_FALSE(ZSTD_isError(compressed_size));
  compressed.resize(compressed_size);

  ASSERT_OK_AND_ASSIGN(
      std::vector<uint8_t> result,
      Decompress({CompressionType::kZstd, std::move(compressed)}));
  EXPECT_EQ(result, original);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

// Verifies that Snappy-compressed data can be decompressed correctly.
TEST(CompressionTest, DecompressSnappyData) {
  std::vector<uint8_t> original(256, 'S');

  size_t max_compressed = snappy::MaxCompressedLength(original.size());
  std::vector<uint8_t> compressed(max_compressed);
  size_t compressed_size = 0;
  base::span<const char> src = base::as_chars(base::span(original));
  base::span<char> dest = base::as_writable_chars(base::span(compressed));
  snappy::RawCompress(src.data(), src.size(), dest.data(), &compressed_size);
  compressed.resize(compressed_size);

  ASSERT_OK_AND_ASSIGN(
      std::vector<uint8_t> result,
      Decompress({CompressionType::kSnappy, std::move(compressed)}));
  EXPECT_EQ(result, original);
}

// Verifies that corrupt ZSTD data returns a corruption error.
TEST(CompressionTest, CorruptZstdDataReturnsError) {
  std::vector<uint8_t> garbage = ToBytes("this is not valid zstd data!");

  std::optional<std::vector<uint8_t>> result =
      Decompress({CompressionType::kZstd, garbage});
  EXPECT_FALSE(result.has_value());
}

// Verifies that corrupt Snappy data returns a corruption error.
TEST(CompressionTest, CorruptSnappyDataReturnsError) {
  std::vector<uint8_t> garbage = ToBytes("this is not valid snappy data!");

  std::optional<std::vector<uint8_t>> result =
      Decompress({CompressionType::kSnappy, garbage});
  EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace storage
