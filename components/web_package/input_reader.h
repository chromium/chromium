// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_INPUT_READER_H_
#define COMPONENTS_WEB_PACKAGE_INPUT_READER_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/memory/stack_allocated.h"
#include "base/types/id_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {

// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3.1
enum class CBORType {
  kUnsignedInt = 0,
  kNegativeInt = 1,
  kByteString = 2,
  kTextString = 3,
  kArray = 4,
  kMap = 5,
  // kTag = 6,
  kSimpleValue = 7,
  // kFloatValue = 7,
};

struct CBORHeader {
  struct StringInfo {
    enum class StringType {
      kByteString,
      kTextString,
    } type;
    uint64_t byte_length;
  };
  struct ContainerInfo {
    enum class ContainerType {
      kArray,
      kMap,
    } type;
    uint64_t size;
  };

  const absl::variant<bool, int64_t, StringInfo, ContainerInfo> data;
};

// The maximum length of the CBOR item header (type and argument).
// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3
// When the additional information (the low-order 5 bits of the first
// byte) is 27, the argument's value is held in the following 8 bytes.
constexpr uint64_t kMaxCBORItemHeaderSize = 9;

// A utility class for reading various values from input buffer.
class InputReader {
  STACK_ALLOCATED();

 public:
  explicit InputReader(base::span<const uint8_t> buf);

  InputReader(const InputReader&) = delete;
  InputReader& operator=(const InputReader&) = delete;

  ~InputReader();

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
  // successfully and the type matches `expected_type`, returns the argument.
  // Otherwise returns nullopt.
  std::optional<uint64_t> ReadCBORHeader(CBORType expected_type);

  // Parses the type and argument of a CBOR item from the input head. If parsed
  // successfully, returns type and:
  //  * value for kUnsignedInt/kNegativeInt;
  //  * value_size for kTextString/kByteString/kMap/kArray.
  // Otherwise returns nullopt.
  std::optional<CBORHeader> ReadCBORHeader();

 private:
  std::optional<std::pair<CBORType, uint64_t>> ReadTypeAndArgument();

  base::SpanReader<const uint8_t> buf_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_INPUT_READER_H_
