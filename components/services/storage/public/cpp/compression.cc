// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/compression.h"

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/snappy/src/snappy.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace storage {

namespace {

std::optional<std::vector<uint8_t>> DecompressZstd(
    base::span<const uint8_t> compressed) {
  uint64_t decompressed_size =
      ZSTD_getFrameContentSize(compressed.data(), compressed.size());
  if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ||
      decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
    return std::nullopt;
  }

  std::vector<uint8_t> decompressed(decompressed_size);
  if (ZSTD_isError(ZSTD_decompress(decompressed.data(), decompressed.size(),
                                   compressed.data(), compressed.size()))) {
    return std::nullopt;
  }
  return decompressed;
}

std::optional<std::vector<uint8_t>> DecompressSnappy(
    base::span<const uint8_t> compressed) {
  size_t decompressed_length;
  base::span<const char> src = base::as_chars(compressed);
  if (!snappy::GetUncompressedLength(src.data(), src.size(),
                                     &decompressed_length)) {
    return std::nullopt;
  }

  std::vector<uint8_t> decompressed(decompressed_length);
  base::span<char> dest = base::as_writable_chars(base::span(decompressed));
  if (!snappy::RawUncompress(src.data(), src.size(), dest.data())) {
    return std::nullopt;
  }
  return decompressed;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

CompressedValue CompressZstd(base::span<const uint8_t> uncompressed) {
  // Compression level of -4 yields compression output similar to Snappy.
  constexpr int kCompressionLevel = -4;

  size_t max_compressed_size = ZSTD_compressBound(uncompressed.size());
  std::vector<uint8_t> compressed(max_compressed_size);

  size_t compressed_length =
      ZSTD_compress(compressed.data(), compressed.size(), uncompressed.data(),
                    uncompressed.size(), kCompressionLevel);

  compressed.resize(compressed_length);
  return {CompressionType::kZstd, std::move(compressed)};
}

#else

CompressedValue CompressSnappy(base::span<const uint8_t> uncompressed) {
  size_t max_compressed_size = snappy::MaxCompressedLength(uncompressed.size());
  std::vector<uint8_t> compressed(max_compressed_size);

  size_t compressed_length = 0;
  base::span<const char> src = base::as_chars(uncompressed);
  base::span<char> dest = base::as_writable_chars(base::span(compressed));
  snappy::RawCompress(src.data(), src.size(), dest.data(), &compressed_length);

  compressed.resize(compressed_length);
  return {CompressionType::kSnappy, std::move(compressed)};
}

#endif

}  // namespace

CompressedValue::CompressedValue(CompressionType type,
                                 std::vector<uint8_t> data)
    : type(type), data(std::move(data)) {}

CompressedValue::CompressedValue(CompressedValue&&) = default;

CompressedValue& CompressedValue::operator=(CompressedValue&&) = default;

CompressedValue::~CompressedValue() = default;

CompressedValue Compress(std::vector<uint8_t> uncompressed) {
  constexpr base::ByteSize kMinimumCompressionSize(64);
  constexpr float kMinimumCompressionRatio = 0.8f;

  if (uncompressed.size() < kMinimumCompressionSize.InBytes()) {
    return {CompressionType::kUncompressed, std::move(uncompressed)};
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  CompressedValue result = CompressZstd(uncompressed);
#else
  CompressedValue result = CompressSnappy(uncompressed);
#endif

  if (result.data.size() <= uncompressed.size() * kMinimumCompressionRatio) {
    return result;
  }
  return {CompressionType::kUncompressed, std::move(uncompressed)};
}

std::optional<std::vector<uint8_t>> Decompress(CompressedValue compressed) {
  switch (compressed.type) {
    case CompressionType::kUncompressed:
      return std::move(compressed.data);

    case CompressionType::kZstd:
      return DecompressZstd(compressed.data);

    case CompressionType::kSnappy:
      return DecompressSnappy(compressed.data);
  }
  NOTREACHED();
}

}  // namespace storage
