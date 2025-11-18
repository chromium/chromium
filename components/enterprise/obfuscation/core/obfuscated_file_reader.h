// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_OBFUSCATED_FILE_READER_H_
#define COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_OBFUSCATED_FILE_READER_H_

#include "base/files/file.h"
#include "base/types/expected.h"
#include "components/enterprise/obfuscation/core/utils.h"

namespace enterprise_obfuscation {

// Used in tests.
class ObfuscatedFileReaderPeer;

// Manages reading and deobfuscating a file that was obfuscated with
// DownloadObfuscator. This class supports seek operations and on-demand
// deobfuscation of file chunks.
class COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION) ObfuscatedFileReader {
 public:
  static base::expected<ObfuscatedFileReader, Error> Create(
      HeaderData header_data,
      base::File file);

  ~ObfuscatedFileReader();

  static base::expected<HeaderData, Error> ReadHeaderData(base::File& file);

  ObfuscatedFileReader(const ObfuscatedFileReader&) = delete;
  ObfuscatedFileReader& operator=(const ObfuscatedFileReader&) = delete;
  ObfuscatedFileReader(ObfuscatedFileReader&& other);
  ObfuscatedFileReader& operator=(ObfuscatedFileReader&& other);

  int64_t Read(base::span<uint8_t> buffer);
  int64_t Seek(int64_t offset, base::File::Whence whence);
  int64_t Tell();
  int64_t GetSize();

 private:
  friend class ObfuscatedFileReaderPeer;

  ObfuscatedFileReader(HeaderData header_data, base::File file);

  base::expected<void, Error> Initialize();

  bool BuildChunkIndex();

  base::File file_;
  uint64_t current_offset_ = 0;  // Offset in the deobfuscated stream.
  uint64_t deobfuscated_size_ = 0;

  struct ChunkInfo {
    uint64_t deobfuscated_offset = 0;  // Offset in the deobfuscated stream.
    uint64_t deobfuscated_size = 0;    // Size of the deobfuscated chunk.
    uint64_t obfuscated_offset = 0;    // Offset in the obfuscated file.
  };
  // Stores information about each chunk, sorted by deobfuscated_offset.
  std::vector<ChunkInfo> chunk_info_;

  // Obfuscation parameters from the header.
  std::array<uint8_t, kKeySize> derived_key_;
  std::vector<uint8_t> nonce_prefix_;
};

}  // namespace enterprise_obfuscation

#endif  // COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_OBFUSCATED_FILE_READER_H_
