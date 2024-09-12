// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_
#define COMPONENTS_ENTERPRISE_OBFUSCATION_CORE_DOWNLOAD_OBFUSCATOR_H_

#include "base/types/expected.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "crypto/secure_hash.h"

namespace enterprise_obfuscation {

class DownloadObfuscator {
 public:
  DownloadObfuscator();
  ~DownloadObfuscator();

  DownloadObfuscator(const DownloadObfuscator&) = delete;
  DownloadObfuscator& operator=(const DownloadObfuscator&) = delete;

  // Obfuscates a chunk of data and updates the hash. Returns the obfuscated
  // data if successful.
  base::expected<std::vector<uint8_t>, Error> ObfuscateChunk(
      base::span<const uint8_t> data,
      bool is_last_chunk);

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
