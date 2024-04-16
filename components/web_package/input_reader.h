// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_INPUT_READER_H_
#define COMPONENTS_WEB_PACKAGE_INPUT_READER_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"

namespace web_package {

// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3.1
enum class CBORType {
  // kUnsignedInt = 0,
  // kNegativeInt = 1,
  kByteString = 2,
  kTextString = 3,
  kArray = 4,
  kMap = 5,
};

// The maximum length of the CBOR item header (type and argument).
// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3
// When the additional information (the low-order 5 bits of the first
// byte) is 27, the argument's value is held in the following 8 bytes.
constexpr uint64_t kMaxCBORItemHeaderSize = 9;

// A utility class for reading various values from input buffer.
class InputReader {
 public:
  explicit InputReader(base::span<const uint8_t> buf) : buf_(buf) {}

  InputReader(const InputReader&) = delete;
  InputReader& operator=(const InputReader&) = delete;

  size_t CurrentOffset() const { return buf_.num_read(); }
  size_t Size() const { return buf_.remaining(); }

  std::optional<uint8_t> ReadByte();

  template <typename T>
    requires(std::is_integral_v<T> && std::is_unsigned_v<T>)
  bool ReadBigEndian(T* out) {
    if constexpr (sizeof(T) == 1) {
      return buf_.ReadU8BigEndian(*out);
    } else if constexpr (sizeof(T) == 2) {
      return buf_.ReadU16BigEndian(*out);
    } else if constexpr (sizeof(T) == 4) {
      return buf_.ReadU32BigEndian(*out);
    } else {
      static_assert(sizeof(T) == 8);
      return buf_.ReadU64BigEndian(*out);
    }
  }

  std::optional<base::span<const uint8_t>> ReadBytes(size_t n);

  std::optional<std::string_view> ReadString(size_t n);

  // Parses the type and argument of a CBOR item from the input head. If parsed
  // successfully and the type matches |expected_type|, returns the argument.
  // Otherwise returns nullopt.
  std::optional<uint64_t> ReadCBORHeader(CBORType expected_type);

 private:
  std::optional<std::pair<CBORType, uint64_t>> ReadTypeAndArgument();

  base::SpanReader<const uint8_t> buf_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_INPUT_READER_H_
