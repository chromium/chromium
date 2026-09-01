// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/download_obfuscator.h"

namespace enterprise_obfuscation {

namespace {

// Calculate the overhead introduced by the obfuscation process for a given data
// source. ReadFunc is a callable type that reads a chunk of data from the
// source.
template <typename ReadFunc>
base::expected<int64_t, Error> CalculateDeobfuscationOverheadImpl(
    int64_t total_size,
    ReadFunc read_func) {
  if (total_size < static_cast<int64_t>(kHeaderSize + kChunkSizePrefixSize)) {
    return RecordAndReturn<int64_t>(
        base::unexpected(Error::kDeobfuscationFailed));
  }
  int64_t offset = kHeaderSize;
  int64_t num_chunks = 0;
  while (offset < total_size) {
    // Read the chunk size prefix.
    auto size_data_result = read_func(offset, kChunkSizePrefixSize);
    if (!size_data_result.has_value()) {
      return RecordAndReturn<int64_t>(
          base::unexpected(Error::kDeobfuscationFailed));
    }
    offset += kChunkSizePrefixSize;

    // Calculate the chunk size.
    auto chunk_size = enterprise_obfuscation::GetObfuscatedChunkSize(
        size_data_result.value());
    if (!chunk_size.has_value()) {
      return RecordAndReturn<int64_t>(base::unexpected(chunk_size.error()));
    }
    offset += chunk_size.value();
    if (offset > total_size) {
      return RecordAndReturn<int64_t>(
          base::unexpected(Error::kDeobfuscationFailed));
    }
    num_chunks++;
  }
  int64_t chunk_overhead = kAuthTagSize + kChunkSizePrefixSize;
  return num_chunks * chunk_overhead + kHeaderSize;
}

}  // namespace

const char DownloadObfuscationData::kUserDataKey[] =
    "enterprise_obfuscation.download_obfuscation_data_key";
DownloadObfuscationData::DownloadObfuscationData(bool is_obfuscated)
    : is_obfuscated(is_obfuscated) {}
DownloadObfuscationData::~DownloadObfuscationData() = default;

DownloadObfuscator::DownloadObfuscator()
    : unobfuscated_hash_(crypto::hash::Hasher(crypto::hash::kSha256)) {}

DownloadObfuscator::~DownloadObfuscator() = default;

base::expected<std::vector<uint8_t>, Error> DownloadObfuscator::ObfuscateChunk(
    base::span<const uint8_t> data,
    bool is_last_chunk) {
  std::vector<uint8_t> result;

  // Update the hash with the original data
  unobfuscated_hash_->Update(data);

  // If it's the first chunk, create and prepend the header.
  if (chunk_counter_ == 0) {
    auto header = CreateHeader(&derived_key_, &nonce_prefix_);
    if (!header.has_value()) {
      return RecordAndReturn<std::vector<uint8_t>>(
          base::unexpected(header.error()));
    }
    result = std::move(header.value());
  }

  auto obfuscated_chunk = ObfuscateDataChunk(data, derived_key_, nonce_prefix_,
                                             chunk_counter_++, is_last_chunk);
  if (!obfuscated_chunk.has_value()) {
    return RecordAndReturn<std::vector<uint8_t>>(
        base::unexpected(obfuscated_chunk.error()));
  }

  result.insert(result.end(), obfuscated_chunk->begin(),
                obfuscated_chunk->end());
  total_overhead_ += result.size() - data.size();
  return result;
}

base::expected<base::span<const uint8_t>, Error>
DownloadObfuscator::GetNextDeobfuscatedChunk(
    base::span<const uint8_t> obfuscated_data) {
  if (deobfuscated_chunk_position_ == deobfuscated_chunk_.size()) {
    // Deobfuscate the next chunk, as we are at the start or we've reached the
    // end of the current deobfuscated chunk.
    next_chunk_offset_ = 0;
    auto deobfuscated_result =
        DeobfuscateChunk(obfuscated_data, next_chunk_offset_);
    if (!deobfuscated_result.has_value()) {
      return RecordAndReturn<base::span<const uint8_t>>(
          base::unexpected(deobfuscated_result.error()));
    }
    deobfuscated_chunk_ = std::move(deobfuscated_result.value());
    deobfuscated_chunk_position_ = 0;
  }

  return base::span<const uint8_t>(deobfuscated_chunk_)
      .subspan(deobfuscated_chunk_position_);
}

base::expected<std::vector<uint8_t>, Error>
DownloadObfuscator::DeobfuscateChunk(base::span<const uint8_t> data,
                                     size_t& obfuscated_file_offset) {
  // Check if data is unobfuscated or corrupted.
  size_t per_chunk_overhead = kChunkSizePrefixSize + kAuthTagSize;
  if (data.size() < per_chunk_overhead) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }

  // If it's the first chunk, get obfuscation data from header.
  if (chunk_counter_ == 0) {
    if (data.size() < kHeaderSize + per_chunk_overhead) {
      return base::unexpected(Error::kDeobfuscationFailed);
    }
    auto header_data = GetHeaderData(data.first(kHeaderSize));
    if (!header_data.has_value()) {
      return base::unexpected(header_data.error());
    }
    derived_key_ = std::move(header_data->derived_key);
    nonce_prefix_ = std::move(header_data->nonce_prefix);
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
      data.subspan(obfuscated_file_offset, chunk_size.value()), derived_key_,
      nonce_prefix_, chunk_counter_++, is_last_chunk);

  if (!result.has_value()) {
    return base::unexpected(result.error());
  }

  obfuscated_file_offset += chunk_size.value();
  return result.value();
}

base::expected<int64_t, Error>
DownloadObfuscator::CalculateDeobfuscationOverhead(
    base::span<const uint8_t> data) {
  return CalculateDeobfuscationOverheadImpl(
      static_cast<int64_t>(data.size()),
      [&data](int64_t offset,
              size_t size) -> base::expected<base::span<const uint8_t>, Error> {
        size_t u_offset = base::checked_cast<size_t>(offset);
        if (u_offset + size > data.size()) {
          return RecordAndReturn<base::span<const uint8_t>>(
              base::unexpected(Error::kDeobfuscationFailed));
        }
        return data.subspan(u_offset, size);
      });
}

base::expected<int64_t, Error>
DownloadObfuscator::CalculateDeobfuscationOverhead(base::File& file) {
  if (!file.IsValid()) {
    return RecordAndReturn<int64_t>(
        base::unexpected(Error::kFileOperationError));
  }
  int64_t file_size = file.GetLength();
  if (file_size < 0) {
    return RecordAndReturn<int64_t>(
        base::unexpected(Error::kFileOperationError));
  }
  std::array<char, kChunkSizePrefixSize> size_buffer;
  return CalculateDeobfuscationOverheadImpl(
      file_size,
      [&file, &size_buffer](int64_t offset, size_t size)
          -> base::expected<base::span<const uint8_t>, Error> {
        if (file.Seek(base::File::FROM_BEGIN, offset) != offset) {
          return RecordAndReturn<base::span<const uint8_t>>(
              base::unexpected(Error::kDeobfuscationFailed));
        }
        std::optional<size_t> bytes_read =
            file.ReadAtCurrentPos(base::as_writable_byte_span(size_buffer));
        if (!bytes_read.has_value() || *bytes_read != size) {
          return RecordAndReturn<base::span<const uint8_t>>(
              base::unexpected(Error::kDeobfuscationFailed));
        }
        return base::as_byte_span(size_buffer);
      });
}

std::optional<crypto::hash::Hasher> DownloadObfuscator::GetUnobfuscatedHash() {
  return std::exchange(unobfuscated_hash_, std::nullopt);
}

void DownloadObfuscator::UpdateDeobfuscatedChunkPosition(size_t bytes_written) {
  deobfuscated_chunk_position_ += bytes_written;
}

}  // namespace enterprise_obfuscation
