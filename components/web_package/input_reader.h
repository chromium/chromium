// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_INPUT_READER_H_
#define COMPONENTS_WEB_PACKAGE_INPUT_READER_H_

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  uint64_t CurrentOffset() const { return current_offset_; }
  size_t Size() const { return buf_.size(); }

  absl::optional<uint8_t> ReadByte();

  template <typename T>
  bool ReadBigEndian(T* out) {
    auto bytes = ReadBytes(sizeof(T));
    if (!bytes) {
      return false;
    }
    base::ReadBigEndian(bytes->data(), out);
    return true;
  }

  absl::optional<base::span<const uint8_t>> ReadBytes(size_t n);

  absl::optional<base::StringPiece> ReadString(size_t n);

  // Parses the type and argument of a CBOR item from the input head. If parsed
  // successfully and the type matches |expected_type|, returns the argument.
  // Otherwise returns nullopt.
  absl::optional<uint64_t> ReadCBORHeader(CBORType expected_type);

 private:
  absl::optional<std::pair<CBORType, uint64_t>> ReadTypeAndArgument();

  void Advance(size_t n);

  base::span<const uint8_t> buf_;
  uint64_t current_offset_ = 0;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_INPUT_READER_H_
