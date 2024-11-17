// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_
#define COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_

#include "base/files/file.h"
#include "base/supports_user_data.h"
#include "base/types/expected.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "crypto/secure_hash.h"

namespace enterprise_obfuscation {

// User data to persist download file obfuscation data for the deobfuscation
// process.
struct COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION) DownloadObfuscationData
    : public base::SupportsUserData::Data {
  explicit DownloadObfuscationData(bool is_obfuscated);
  ~DownloadObfuscationData() override;
  static const char kUserDataKey[];

  bool is_obfuscated = false;
};

// DownloadObfuscator handles obfuscation or deobfuscation of download data.
// It is designed to allow using separate instances for obfuscation and
// deobfuscation of the same file. For deobfuscation with the same instance,
// reset chunk_counter_ to 0 before starting.
class COMPONENT_EXPORT(ENTERPRISE_OBFUSCATION) DownloadObfuscator {
 public:
  DownloadObfuscator();
  ~DownloadObfuscator();

  DownloadObfuscator(const DownloadObfuscator&) = delete;
  DownloadObfuscator& operator=(const DownloadObfuscator&) = delete;

  // Obfuscates a chunk of data and updates the hash. If it's the first chunk,
  // also creates the header and initializes derived key and nonce prefix.
  // Returns the obfuscated data if successful.
  base::expected<std::vector<uint8_t>, Error> ObfuscateChunk(
      base::span<const uint8_t> data,
      bool is_last_chunk);

  // Returns a span of the next available deobfuscated data, allowing for
  // incremental processing of obfuscated data. It is used when there is no
  // guarantee that the current deobfuscated chunk will be entirely written and
  // avoids deobfuscating the same chunk multiple times. For cases without
  // partial writes, prefer using DeobfuscateChunk function for performance.
  //
  // Note: This method maintains internal state across calls. Subsequent calls
  // will continue from where the previous call left off.
  base::expected<base::span<const uint8_t>, Error> GetNextDeobfuscatedChunk(
      base::span<const uint8_t> obfuscated_data);

  // Deobfuscates the next obfuscated chunk of data. If it's the first chunk,
  // retrieves the header and extracts derived key and nonce prefix. If
  // successful, returns the deobfuscated data, and updates the obfuscated file
  // offset position to the position of the next obfuscated chunk to read.
  base::expected<std::vector<uint8_t>, Error> DeobfuscateChunk(
      base::span<const uint8_t> data,
      size_t& obfuscated_file_offset);

  // Calculates file overhead that should have been added during obfuscation.
  // This version works with in-memory data.
  base::expected<int64_t, Error> CalculateDeobfuscationOverhead(
      base::span<const uint8_t> data);

  // Calculates file overhead that should have been added during obfuscation.
  // This version works with file-based data.
  base::expected<int64_t, Error> CalculateDeobfuscationOverhead(
      base::File& file);

  // Returns the total overhead added by obfuscation.
  int64_t GetTotalOverhead() const { return total_overhead_; }

  // Returns the hash of the original data. Call only after completing
  // obfuscation as it invalidates the obfuscator.
  std::unique_ptr<crypto::SecureHash> GetUnobfuscatedHash();

  // Updates the offset within the current obfuscated chunk with the bytes that
  // were actually processed.
  void UpdateDeobfuscatedChunkPosition(size_t bytes_written);

  // Returns the offset of the next obfuscated chunk from the start of the
  // current one.
  size_t GetNextChunkOffset() const { return next_chunk_offset_; }

 private:
  // Shared members.
  std::vector<uint8_t> nonce_prefix_;
  std::vector<uint8_t> derived_key_;
  uint32_t chunk_counter_ = 0;
  int64_t total_overhead_ = 0;
  std::unique_ptr<crypto::SecureHash> unobfuscated_hash_;

  // Members used for partial deobfuscation.
  std::vector<uint8_t> deobfuscated_chunk_;
  size_t deobfuscated_chunk_position_ = 0;
  size_t next_chunk_offset_ = 0;
};

}  // namespace enterprise_obfuscation

#endif  // COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_
