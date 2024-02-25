// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/single_file_tar_reader.h"

#include <optional>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"

namespace {
// https://www.gnu.org/software/tar/manual/html_node/Standard.html
constexpr size_t kHeaderSize = 512;
constexpr size_t kFileSizeFieldOffset = 124;
constexpr size_t kFileSizeFieldLength = 12;
}  // namespace

SingleFileTarReader::SingleFileTarReader() = default;
SingleFileTarReader::~SingleFileTarReader() = default;

bool SingleFileTarReader::ExtractChunk(base::span<const uint8_t> src_buffer,
                                       base::span<const uint8_t>& dst_buffer) {
  dst_buffer = src_buffer;
  // We haven't read the header.
  if (!tar_content_size_.has_value()) {
    if (src_buffer.size() < kHeaderSize)
      return false;

    // TODO(tetsui): check the file header checksum

    tar_content_size_ = ReadOctalNumber(
        src_buffer.subspan(kFileSizeFieldOffset, kFileSizeFieldLength));
    if (!tar_content_size_.has_value())
      return false;

    // Drop the header.
    dst_buffer = dst_buffer.subspan(kHeaderSize);
  }

  if (bytes_processed_ > tar_content_size_.value())
    return false;

  const uint64_t bytes_remaining = tar_content_size_.value() - bytes_processed_;
  // A tar file always has a padding at the end of the file. If `dst_buffer`
  // contains the padding, drop it.
  if (dst_buffer.size() > bytes_remaining) {
    // The comparison above guarantees that `checked_cast` will succeed:
    dst_buffer = dst_buffer.first(base::checked_cast<size_t>(bytes_remaining));
  }

  bytes_processed_ += dst_buffer.size();

  return true;
}

bool SingleFileTarReader::IsComplete() const {
  if (!tar_content_size_.has_value())
    return false;
  return tar_content_size_.value() <= bytes_processed_;
}

// static
std::optional<uint64_t> SingleFileTarReader::ReadOctalNumber(
    base::span<const uint8_t> buffer) {
  const size_t length = buffer.size();
  if (length < 8u)
    return std::nullopt;

  uint64_t num = 0;

  // In GNU tar extension, when the number starts with an invalid ASCII
  // character 0x80, then non-leading 8 bytes of the field should be interpreted
  // as a big-endian integer.
  // https://www.gnu.org/software/tar/manual/html_node/Extensions.html
  if (buffer[0] == 0x80) {
    for (size_t i = length - 8; i < length; ++i) {
      num <<= 8;
      num += buffer[i];
    }
    return num;
  }

  for (size_t i = 0; i < length; ++i) {
    const char as_char = static_cast<char>(buffer[i]);
    if (as_char == '\0')
      break;
    num *= 8;
    num += as_char - '0';
  }
  return num;
}
