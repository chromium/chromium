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

base::expected<std::vector<uint8_t>, Error>
DownloadObfuscator::DeobfuscateChunk(base::span<const uint8_t> data,
                                     size_t& obfuscated_file_offset) {
  if (data.size() < kHeaderSize + kChunkSizePrefixSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  // If it's the first chunk, get obfuscation data from header.
  if (chunk_counter_ == 0) {
    std::vector<uint8_t> header(data.begin(), data.begin() + kHeaderSize);
    auto header_data = GetHeaderData(header);
    if (!header_data.has_value()) {
      return base::unexpected(header_data.error());
    }
    derived_key_ = std::move(header_data->first);
    nonce_prefix_ = std::move(header_data->second);
    obfuscated_file_offset = kHeaderSize;
  }

  // Read the size of the next chunk.
  auto chunk_size = GetObfuscatedChunkSize(
      data.subspan(obfuscated_file_offset, kChunkSizePrefixSize));
  if (!chunk_size.has_value()) {
    return base::unexpected(chunk_size.error());
  }
  obfuscated_file_offset += kChunkSizePrefixSize;

  // Deobfuscate the next data chunk.
  bool is_last_chunk =
      (obfuscated_file_offset + chunk_size.value() >= data.size());
  auto result = DeobfuscateDataChunk(
      base::make_span(data).subspan(obfuscated_file_offset, chunk_size.value()),
      derived_key_, nonce_prefix_, chunk_counter_++, is_last_chunk);

  if (!result.has_value()) {
    return base::unexpected(result.error());
  }

  obfuscated_file_offset += chunk_size.value();
  return result.value();
}

base::expected<int64_t, Error>
DownloadObfuscator::CalculateDeobfuscationOverhead(
    base::span<const uint8_t> data) {
  if (data.size() < kHeaderSize + kChunkSizePrefixSize) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  size_t offset = kHeaderSize;
  int num_chunks = 0;

  while (offset < data.size()) {
    auto chunk_size = enterprise_obfuscation::GetObfuscatedChunkSize(
        data.subspan(offset, kChunkSizePrefixSize));
    offset += kChunkSizePrefixSize;
    if (!chunk_size.has_value()) {
      return base::unexpected(chunk_size.error());
    }

    offset += chunk_size.value();
    num_chunks++;
  }
  size_t chunk_overhead = kAuthTagSize + kChunkSizePrefixSize;
  return num_chunks * chunk_overhead + kHeaderSize;
}

std::unique_ptr<crypto::SecureHash> DownloadObfuscator::GetUnobfuscatedHash() {
  return std::move(unobfuscated_hash_);
}

}  // namespace enterprise_obfuscation
