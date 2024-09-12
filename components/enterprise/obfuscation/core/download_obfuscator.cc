// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/download_obfuscator.h"

namespace enterprise_obfuscation {

DownloadObfuscator::DownloadObfuscator()
    : unobfuscated_hash_(
          crypto::SecureHash::Create(crypto::SecureHash::SHA256)) {}

DownloadObfuscator::~DownloadObfuscator() = default;

base::expected<std::vector<uint8_t>, Error> DownloadObfuscator::ObfuscateChunk(
    base::span<const uint8_t> data,
    bool is_last_chunk) {
  std::vector<uint8_t> result;

  // Update the hash with the original data
  unobfuscated_hash_->Update(data.data(), data.size());

  // If it's the first chunk, create and prepend the header.
  if (chunk_counter_ == 0) {
    auto header = CreateHeader(&derived_key_, &nonce_prefix_);
    if (!header.has_value()) {
      return base::unexpected(header.error());
    }
    result = std::move(header.value());
  }

  auto obfuscated_chunk = ObfuscateDataChunk(data, derived_key_, nonce_prefix_,
                                             chunk_counter_++, is_last_chunk);
  if (!obfuscated_chunk.has_value()) {
    return base::unexpected(obfuscated_chunk.error());
  }

  result.insert(result.end(), obfuscated_chunk->begin(),
                obfuscated_chunk->end());
  total_overhead_ += result.size() - data.size();
  return result;
}

std::unique_ptr<crypto::SecureHash> DownloadObfuscator::GetUnobfuscatedHash() {
  return std::move(unobfuscated_hash_);
}

}  // namespace enterprise_obfuscation
