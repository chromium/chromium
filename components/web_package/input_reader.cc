// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/input_reader.h"

#include "base/strings/string_util.h"

namespace web_package {

absl::optional<uint8_t> InputReader::ReadByte() {
  if (buf_.empty())
    return absl::nullopt;
  uint8_t byte = buf_[0];
  Advance(1);
  return byte;
}

absl::optional<base::span<const uint8_t>> InputReader::ReadBytes(size_t n) {
  if (buf_.size() < n)
    return absl::nullopt;
  auto result = buf_.subspan(0, n);
  Advance(n);
  return result;
}

absl::optional<base::StringPiece> InputReader::ReadString(size_t n) {
  auto bytes = ReadBytes(n);
  if (!bytes)
    return absl::nullopt;
  base::StringPiece str(reinterpret_cast<const char*>(bytes->data()),
                        bytes->size());
  if (!base::IsStringUTF8(str))
    return absl::nullopt;
  return str;
}

absl::optional<uint64_t> InputReader::ReadCBORHeader(CBORType expected_type) {
  auto pair = ReadTypeAndArgument();
  if (!pair || pair->first != expected_type)
    return absl::nullopt;
  return pair->second;
}

// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3
absl::optional<std::pair<CBORType, uint64_t>>
InputReader::ReadTypeAndArgument() {
  absl::optional<uint8_t> first_byte = ReadByte();
  if (!first_byte)
    return absl::nullopt;

  CBORType type = static_cast<CBORType>((*first_byte & 0xE0) / 0x20);
  uint8_t b = *first_byte & 0x1F;

  if (b <= 23)
    return std::make_pair(type, b);
  if (b == 24) {
    auto content = ReadByte();
    if (!content || *content < 24)
      return absl::nullopt;
    return std::make_pair(type, *content);
  }
  if (b == 25) {
    uint16_t content;
    if (!ReadBigEndian(&content) || content >> 8 == 0)
      return absl::nullopt;
    return std::make_pair(type, content);
  }
  if (b == 26) {
    uint32_t content;
    if (!ReadBigEndian(&content) || content >> 16 == 0)
      return absl::nullopt;
    return std::make_pair(type, content);
  }
  if (b == 27) {
    uint64_t content;
    if (!ReadBigEndian(&content) || content >> 32 == 0)
      return absl::nullopt;
    return std::make_pair(type, content);
  }
  return absl::nullopt;
}

void InputReader::Advance(size_t n) {
  DCHECK_LE(n, buf_.size());
  buf_ = buf_.subspan(n);
  current_offset_ += n;
}

}  // namespace web_package
