// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/input_reader.h"

#include "base/strings/string_util.h"

namespace web_package {

std::optional<uint8_t> InputReader::ReadByte() {
  uint8_t b;
  if (!buf_.ReadU8(&b)) {
    return std::nullopt;
  }
  return {b};
}

std::optional<base::span<const uint8_t>> InputReader::ReadBytes(size_t n) {
  return buf_.ReadSpan(n);
}

std::optional<std::string_view> InputReader::ReadString(size_t n) {
  auto bytes = buf_.ReadSpan(n);
  if (!bytes) {
    return std::nullopt;
  }
  std::string_view str(base::as_chars(*bytes).begin(),
                       base::as_chars(*bytes).end());
  if (!base::IsStringUTF8(str)) {
    return std::nullopt;
  }
  return str;
}

std::optional<uint64_t> InputReader::ReadCBORHeader(CBORType expected_type) {
  auto pair = ReadTypeAndArgument();
  if (!pair || pair->first != expected_type) {
    return std::nullopt;
  }
  return pair->second;
}

// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3
std::optional<std::pair<CBORType, uint64_t>>
InputReader::ReadTypeAndArgument() {
  std::optional<uint8_t> first_byte = ReadByte();
  if (!first_byte) {
    return std::nullopt;
  }

  CBORType type = static_cast<CBORType>((*first_byte & 0xE0) / 0x20);
  uint8_t b = *first_byte & 0x1F;

  if (b <= 23) {
    return std::make_pair(type, b);
  }
  if (b == 24) {
    auto content = ReadByte();
    if (!content || *content < 24) {
      return std::nullopt;
    }
    return std::make_pair(type, *content);
  }
  if (b == 25) {
    uint16_t content;
    if (!ReadBigEndian(&content) || content >> 8 == 0) {
      return std::nullopt;
    }
    return std::make_pair(type, content);
  }
  if (b == 26) {
    uint32_t content;
    if (!ReadBigEndian(&content) || content >> 16 == 0) {
      return std::nullopt;
    }
    return std::make_pair(type, content);
  }
  if (b == 27) {
    uint64_t content;
    if (!ReadBigEndian(&content) || content >> 32 == 0) {
      return std::nullopt;
    }
    return std::make_pair(type, content);
  }
  return std::nullopt;
}

}  // namespace web_package
