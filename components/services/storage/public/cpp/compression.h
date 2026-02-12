// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_COMPRESSION_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_COMPRESSION_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/component_export.h"

namespace storage {

// Persisted to disk; do not reuse or change values.
enum class CompressionType : uint8_t {
  kUncompressed = 0,  // Not compressed.
  kZstd = 1,          // Standalone ZSTD with no dictionary.
  kSnappy = 2,        // Snappy.
};

struct COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC) CompressedValue {
  CompressedValue(CompressionType type, std::vector<uint8_t> data);
  ~CompressedValue();

  // Support move-only.
  CompressedValue(CompressedValue&&);
  CompressedValue& operator=(CompressedValue&&);

  CompressedValue(const CompressedValue&) = delete;
  const CompressedValue& operator=(const CompressedValue&) = delete;

  CompressionType type;
  std::vector<uint8_t> data;
};

// Compresses `uncompressed` using the platform-appropriate algorithm (ZSTD on
// desktop, Snappy on Android/Fuchsia). Intended for SQLite. SQLite does not
// have builtin compression. Putting compressed values into SQLite can save
// space. However, other databases like LevelDB, automatically compress, making
// value compression redundant.
//
// ZSTD is preferred because prior testing has shown that its performance is
// comparable or slightly better than Snappy. Additionally, ZSTD's performance
// is tunable with parameters like compression level. To cut down on binary
// size, ZSTD compression is not available on mobile platforms (Android and
// Fuchsia). However, ZSTD decompression is available on all platforms.
//
// Returns `uncompressed` if the input is below the minimum size threshold or if
// compression does not achieve a sufficient ratio.
COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
CompressedValue Compress(std::vector<uint8_t> uncompressed);

// Decompresses `compressed` according to its `type` field. Returns the
// decompressed data, or `nullopt` if decompression fails.
COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC)
std::optional<std::vector<uint8_t>> Decompress(CompressedValue compressed);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_COMPRESSION_H_
