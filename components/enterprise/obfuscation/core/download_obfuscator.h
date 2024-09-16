// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_
#define COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_

#include "base/types/expected.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "crypto/secure_hash.h"

namespace enterprise_obfuscation {

// DownloadObfuscator handles obfuscation or deobfuscation of download data.
// It is designed to allow using separate instances for obfuscation and
// deobfuscation of the same file. For deobfuscation with the same instance,
// reset chunk_counter_ to 0 before starting.
class DownloadObfuscator {
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

  // Deobfuscates the next obfuscated chunk of data. If it's the first chunk,
  // retrieves the header and extracts derived key and nonce prefix. If
  // successful, returns the deobfuscated data, and updates the obfuscated file
  // offset position to the position of the next obfuscated chunk to read.
  base::expected<std::vector<uint8_t>, Error> DeobfuscateChunk(
      base::span<const uint8_t> data,
      size_t& obfuscated_file_offset);

  // Calculates file overhead that should have been added during obfuscation.
  base::expected<int64_t, Error> CalculateDeobfuscationOverhead(
      base::span<const uint8_t> data);

  // Returns the total overhead added by obfuscation.
  int64_t GetTotalOverhead() const { return total_overhead_; }

  // Returns the hash of the original data. Call only after completing
  // obfuscation as it invalidates the obfuscator.
  std::unique_ptr<crypto::SecureHash> GetUnobfuscatedHash();

 private:
  std::vector<uint8_t> nonce_prefix_;
  std::vector<uint8_t> derived_key_;
  uint32_t chunk_counter_ = 0;
  int64_t total_overhead_ = 0;
  std::unique_ptr<crypto::SecureHash> unobfuscated_hash_;
};

}  // namespace enterprise_obfuscation

#endif  // COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_
