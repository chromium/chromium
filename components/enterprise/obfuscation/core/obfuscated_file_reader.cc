// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/obfuscation/core/obfuscated_file_reader.h"

#include "base/files/file.h"
#include "base/types/expected.h"
#include "components/enterprise/obfuscation/core/utils.h"

namespace enterprise_obfuscation {

// static
base::expected<ObfuscatedFileReader, Error> ObfuscatedFileReader::Create(
    HeaderData header_data,
    base::File file) {
  ObfuscatedFileReader reader(std::move(header_data), std::move(file));
  if (auto init_result = reader.Initialize(); !init_result.has_value()) {
    return base::unexpected(init_result.error());
  }
  return reader;
}

ObfuscatedFileReader::ObfuscatedFileReader(HeaderData header_data,
                                           base::File file)
    : file_(std::move(file)),
      derived_key_(std::move(header_data.derived_key)),
      nonce_prefix_(std::move(header_data.nonce_prefix)) {}

ObfuscatedFileReader::~ObfuscatedFileReader() = default;

ObfuscatedFileReader::ObfuscatedFileReader(ObfuscatedFileReader&& other) =
    default;
ObfuscatedFileReader& ObfuscatedFileReader::operator=(
    ObfuscatedFileReader&& other) = default;

base::expected<void, Error> ObfuscatedFileReader::Initialize() {
  if (!file_.IsValid()) {
    return base::unexpected(Error::kFileOperationError);
  }
  if (!BuildChunkIndex()) {
    return base::unexpected(Error::kDeobfuscationFailed);
  }
  return base::ok();
}

// static
base::expected<HeaderData, Error> ObfuscatedFileReader::ReadHeaderData(
    base::File& file) {
  if (!file.IsValid()) {
    return base::unexpected(Error::kFileOperationError);
  }

  std::array<uint8_t, kHeaderSize> header_buffer;
  base::span<uint8_t> buffer = base::span(header_buffer);

  if (file.Read(0u, buffer) != kHeaderSize) {
    return base::unexpected(Error::kFileOperationError);
  }

  return GetHeaderData(header_buffer);
}

int64_t ObfuscatedFileReader::Read(base::span<uint8_t> buffer) {
  uint64_t bytes_to_read = std::min(static_cast<uint64_t>(buffer.size_bytes()),
                                    deobfuscated_size_ - current_offset_);
  if (bytes_to_read == 0) {
    return 0;
  }

  uint64_t read_end_offset = current_offset_ + bytes_to_read;
  uint64_t total_bytes_copied = 0;

  // Find the first chunk that might overlap with the read range.
  auto it = std::lower_bound(
      chunk_info_.begin(), chunk_info_.end(), current_offset_,
      [](const ChunkInfo& chunk, uint64_t offset) {
        return (chunk.deobfuscated_offset + chunk.deobfuscated_size) <= offset;
      });

  for (; it != chunk_info_.end(); ++it) {
    const auto& chunk = *it;
    uint64_t chunk_start_deobfuscated = chunk.deobfuscated_offset;
    uint64_t chunk_end_deobfuscated =
        chunk.deobfuscated_offset + chunk.deobfuscated_size;

    if (current_offset_ < chunk_end_deobfuscated &&
        read_end_offset > chunk_start_deobfuscated) {
      // This chunk overlaps with the read range.
      uint64_t obfuscated_chunk_size = chunk.deobfuscated_size + kAuthTagSize;
      std::vector<uint8_t> obfuscated_data(obfuscated_chunk_size);

      std::optional<size_t> bytes_read =
          file_.Read(chunk.obfuscated_offset,
                     base::as_writable_bytes(base::span(obfuscated_data)));
      if (!bytes_read.has_value() || *bytes_read != obfuscated_chunk_size) {
        return -1;
      }

      size_t chunk_index = std::distance(chunk_info_.begin(), it);
      bool is_last_chunk = (chunk_index == chunk_info_.size() - 1);
      // TODO(crbug.com/378490429): Maybe add cache for chunk deobfuscations.
      auto deobfuscated_result =
          DeobfuscateDataChunk(obfuscated_data, derived_key_, nonce_prefix_,
                               chunk_index, is_last_chunk);

      if (!deobfuscated_result.has_value()) {
        return -1;
      }

      const std::vector<uint8_t>& deobfuscated_chunk =
          deobfuscated_result.value();

      uint64_t read_start_in_chunk = 0;
      if (current_offset_ > chunk_start_deobfuscated) {
        read_start_in_chunk = current_offset_ - chunk_start_deobfuscated;
      }

      uint64_t read_end_in_chunk = chunk.deobfuscated_size;
      if (read_end_offset < chunk_end_deobfuscated) {
        read_end_in_chunk = read_end_offset - chunk_start_deobfuscated;
      }

      uint64_t bytes_to_copy_from_chunk =
          read_end_in_chunk - read_start_in_chunk;

      buffer.copy_prefix_from(
          base::span(deobfuscated_chunk)
              .subspan(static_cast<size_t>(read_start_in_chunk))
              .take_first(static_cast<size_t>(bytes_to_copy_from_chunk)));
      buffer = buffer.subspan(static_cast<size_t>(bytes_to_copy_from_chunk));
      total_bytes_copied += bytes_to_copy_from_chunk;
    } else if (chunk_start_deobfuscated >= read_end_offset) {
      // This chunk is past the read range.
      break;
    }
  }

  current_offset_ += total_bytes_copied;
  return total_bytes_copied;
}

int64_t ObfuscatedFileReader::Seek(int64_t offset, base::File::Whence whence) {
  int64_t base = 0;
  switch (whence) {
    case base::File::Whence::FROM_BEGIN:
      base = 0;
      break;
    case base::File::Whence::FROM_CURRENT:
      base = current_offset_;
      break;
    case base::File::Whence::FROM_END:
      base = deobfuscated_size_;
      break;
  }

  int64_t target_offset = base + offset;
  if (target_offset < 0 ||
      target_offset > static_cast<int64_t>(deobfuscated_size_)) {
    return -1;
  }

  current_offset_ = target_offset;
  return current_offset_;
}

int64_t ObfuscatedFileReader::Tell() {
  return current_offset_;
}

int64_t ObfuscatedFileReader::GetSize() {
  return deobfuscated_size_;
}

bool ObfuscatedFileReader::BuildChunkIndex() {
  int64_t file_length = file_.GetLength();
  if (file_length < 0) {
    return false;
  }

  uint64_t total_size = static_cast<uint64_t>(file_length);
  if (total_size < kHeaderSize + kChunkSizePrefixSize) {
    return false;  // Not even enough space for header and one chunk prefix.
  }

  uint64_t current_obfuscated_offset = kHeaderSize;
  uint64_t current_deobfuscated_offset = 0;
  deobfuscated_size_ = 0;

  while (current_obfuscated_offset < total_size) {
    if (total_size - current_obfuscated_offset < kChunkSizePrefixSize) {
      return false;  // Incomplete prefix.
    }

    std::array<uint8_t, kChunkSizePrefixSize> size_buffer;
    std::optional<size_t> bytes_read =
        file_.Read(current_obfuscated_offset, size_buffer);
    if (!bytes_read.has_value() || *bytes_read != kChunkSizePrefixSize) {
      return false;  // Failed to read chunk size prefix.
    }

    auto chunk_size_result = GetObfuscatedChunkSize(size_buffer);
    if (!chunk_size_result.has_value()) {
      return false;  // Invalid chunk size prefix.
    }
    uint64_t obfuscated_chunk_size = chunk_size_result.value();

    if (obfuscated_chunk_size < kAuthTagSize) {
      return false;  // Obfuscated chunk size smaller than auth tag.
    }

    uint64_t deobfuscated_chunk_size = obfuscated_chunk_size - kAuthTagSize;
    uint64_t chunk_data_offset =
        current_obfuscated_offset + kChunkSizePrefixSize;

    chunk_info_.push_back({current_deobfuscated_offset, deobfuscated_chunk_size,
                           chunk_data_offset});
    current_deobfuscated_offset += deobfuscated_chunk_size;
    deobfuscated_size_ += deobfuscated_chunk_size;

    current_obfuscated_offset += kChunkSizePrefixSize + obfuscated_chunk_size;

    if (current_obfuscated_offset > total_size) {
      return false;  // Chunk extends beyond file size.
    }
  }

  return true;
}

}  // namespace enterprise_obfuscation
