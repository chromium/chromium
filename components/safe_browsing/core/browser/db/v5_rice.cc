// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_rice.h"

#include <algorithm>
#include <array>
#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/sys_byteorder.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace safe_browsing {

namespace v5_rice_utils {

// =============================================================================
// Uint256 Implementation
// =============================================================================

Uint256& Uint256::operator+=(const Uint256& other) {
  absl::uint128 old_low = low;
  low += other.low;
  high += other.high;
  if (low < old_low) {
    high += 1;
  }
  return *this;
}

Uint256 Uint256::operator+(const Uint256& other) const {
  Uint256 result = *this;
  result += other;
  return result;
}

std::ostream& operator<<(std::ostream& os, const Uint256& v) {
  return os << "(" << v.high << ", " << v.low << ")";
}

Uint256& Uint256::operator<<=(int shift) {
  CHECK_GE(shift, 0);
  if (shift == 0) {
    return *this;
  }
  if (shift >= 256) {
    high = 0;
    low = 0;
  } else if (shift >= 128) {
    high = low << (shift - 128);
    low = 0;
  } else {
    high = (high << shift) | (low >> (128 - shift));
    low = low << shift;
  }
  return *this;
}

Uint256 Uint256::operator<<(int shift) const {
  Uint256 result = *this;
  result <<= shift;
  return result;
}

Uint256& Uint256::operator>>=(int shift) {
  CHECK_GE(shift, 0);
  if (shift == 0) {
    return *this;
  }
  if (shift >= 256) {
    high = 0;
    low = 0;
  } else if (shift >= 128) {
    low = high >> (shift - 128);
    high = 0;
  } else {
    low = (low >> shift) | (high << (128 - shift));
    high = high >> shift;
  }
  return *this;
}

Uint256 Uint256::operator>>(int shift) const {
  Uint256 result = *this;
  result >>= shift;
  return result;
}

Uint256& Uint256::operator|=(const Uint256& other) {
  high |= other.high;
  low |= other.low;
  return *this;
}

Uint256 Uint256::operator|(const Uint256& other) const {
  Uint256 result = *this;
  result |= other;
  return result;
}

// =============================================================================
// V5BitReader Implementation
// =============================================================================

V5BitReader::V5BitReader(base::span<const uint8_t> data) : data_(data) {}

bool V5BitReader::HasMore() const {
  return byte_index_ < data_.size();
}

// TODO(crbug.com/362791941): Replace this implementation with uint32 buffer
// optimization.
bool V5BitReader::ReadSingleBit(bool* bit) {
  if (!HasMore()) {
    return false;
  }
  // Right shift the byte by the bit index we want to read, and then `& 1` to
  // read the least significant bit. e.g. 00000100 would read a 1 into `bit`
  // only for `bit_index_` = 2.
  *bit = (data_[byte_index_] >> bit_index_) & 1;
  bit_index_++;
  if (bit_index_ == 8) {
    bit_index_ = 0;
    byte_index_++;
  }
  return true;
}

// TODO(crbug.com/362791941): Replace this implementation with uint32 buffer
// optimization.
template <typename T>
bool V5BitReader::ReadMultipleBits(int num_bits, T* out) {
  T result{};
  for (int i = 0; i < num_bits; ++i) {
    bool bit;
    if (!ReadSingleBit(&bit)) {
      return false;
    }
    // Fill in the next least significant bit.
    if (bit) {
      result |= (static_cast<T>(1) << i);
    }
  }
  *out = result;
  return true;
}

// =============================================================================
// TryAdd Implementation & Specializations
// =============================================================================

template <typename T>
bool TryAdd(T a, T b, T* result) {
  *result = a + b;
  return *result >= a;
}

bool TryAdd(Uint256 a, Uint256 b, Uint256* result) {
  absl::uint128 low = a.low + b.low;
  bool carry = (low < a.low);
  absl::uint128 high = a.high + b.high;
  bool overflow = (high < a.high);
  if (carry) {
    high += 1;
    if (high == 0) {
      overflow = true;
    }
  }
  *result = Uint256(high, low);
  return !overflow;
}

// =============================================================================
// SerializeToBigEndianBytes Implementation
// =============================================================================

namespace {

template <typename T>
struct SerializationTraits;

template <>
struct SerializationTraits<uint32_t> {
  using ArrayType = std::array<uint32_t, 1>;
  static ArrayType SerializeToBigEndian(uint32_t val) {
    return {base::HostToNet32(val)};
  }
};

template <>
struct SerializationTraits<uint64_t> {
  using ArrayType = std::array<uint64_t, 1>;
  static ArrayType SerializeToBigEndian(uint64_t val) {
    return {base::HostToNet64(val)};
  }
};

template <>
struct SerializationTraits<absl::uint128> {
  using ArrayType = std::array<uint64_t, 2>;
  static ArrayType SerializeToBigEndian(absl::uint128 val) {
    return {base::HostToNet64(absl::Uint128High64(val)),
            base::HostToNet64(absl::Uint128Low64(val))};
  }
};

template <>
struct SerializationTraits<Uint256> {
  using ArrayType = std::array<uint64_t, 4>;
  static ArrayType SerializeToBigEndian(Uint256 val) {
    return {base::HostToNet64(absl::Uint128High64(val.high)),
            base::HostToNet64(absl::Uint128Low64(val.high)),
            base::HostToNet64(absl::Uint128High64(val.low)),
            base::HostToNet64(absl::Uint128Low64(val.low))};
  }
};

}  // namespace

template <typename T>
std::string SerializeToBigEndianBytes(std::vector<T> decoded) {
  // Allocate a contiguous buffer of byte blocks.
  std::vector<typename SerializationTraits<T>::ArrayType> serialized_blocks(
      decoded.size());

  // Serialize each value to big-endian in the buffer.
  std::transform(decoded.begin(), decoded.end(), serialized_blocks.begin(),
                 &SerializationTraits<T>::SerializeToBigEndian);

  // Reinterpret the buffer as raw characters.
  base::span<const char> char_span =
      base::as_chars(base::span(serialized_blocks));

  // Copy the bytes into the final string.
  return std::string(char_span.data(), char_span.size());
}

// =============================================================================
// Explicit Template Instantiations
// =============================================================================
// These explicit template instantiations tell the compiler which concrete types
// we will use with these templates. This allows us to keep the template
// definitions in this .cc file without causing linker errors for external
// callers.
//
// If an external caller tries to use these templates with an unsupported type,
// they will get a compile or linker error.
template bool V5BitReader::ReadMultipleBits<uint32_t>(int, uint32_t*);
template bool V5BitReader::ReadMultipleBits<uint64_t>(int, uint64_t*);
template bool V5BitReader::ReadMultipleBits<absl::uint128>(int, absl::uint128*);
template bool V5BitReader::ReadMultipleBits<Uint256>(int, Uint256*);
template bool TryAdd<uint32_t>(uint32_t, uint32_t, uint32_t*);
template bool TryAdd<uint64_t>(uint64_t, uint64_t, uint64_t*);
template bool TryAdd<absl::uint128>(absl::uint128,
                                    absl::uint128,
                                    absl::uint128*);
template std::string SerializeToBigEndianBytes<uint32_t>(std::vector<uint32_t>);
template std::string SerializeToBigEndianBytes<uint64_t>(std::vector<uint64_t>);
template std::string SerializeToBigEndianBytes<absl::uint128>(
    std::vector<absl::uint128>);
template std::string SerializeToBigEndianBytes<Uint256>(std::vector<Uint256>);

}  // namespace v5_rice_utils

// =============================================================================
// V5RiceDecoder Implementation
// =============================================================================

// static
template <typename T>
V5DecodeResult V5RiceDecoder::DecodePrefixes(
    T first_value,
    int rice_parameter,
    int num_entries,
    base::span<const uint8_t> encoded_data,
    std::string* out) {
  // Decode the Rice-encoded bitstream into host-order integers.
  std::vector<T> decoded_values;
  V5DecodeResult result = V5RiceDecoder::DecodeIntegers<T>(
      first_value, rice_parameter, num_entries, encoded_data, &decoded_values);
  if (result != V5DecodeResult::kSuccess) {
    return result;
  }

  // Convert the decoded integers into to big-endian bytes and write to the
  // output.
  *out = v5_rice_utils::SerializeToBigEndianBytes(std::move(decoded_values));
  return V5DecodeResult::kSuccess;
}

template <typename T>
V5DecodeResult V5RiceDecoder::DecodeIntegers(
    T first_value,
    int rice_parameter,
    int num_entries,
    base::span<const uint8_t> encoded_data,
    std::vector<T>* out) {
  CHECK(out);
  CHECK(out->empty());
  CHECK_GE(num_entries, 0);
  CHECK_GE(rice_parameter, v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter);
  CHECK_LE(rice_parameter, v5_rice_utils::V5TypeTraits<T>::kMaxRiceParameter);

  // Initialize the output vector and store the first value.
  out->reserve(num_entries + 1);
  out->push_back(first_value);

  v5_rice_utils::V5BitReader reader(encoded_data);
  T last_value = first_value;

  // Decode each delta offset and add them up sequentially to reconstruct the
  // hash values.
  //
  // EXAMPLE:
  //  - first value: 123
  //  - decoded offsets: [12, 15, 10]
  //  - result: [123, 135, 150, 160]
  for (int i = 0; i < num_entries; ++i) {
    T offset;
    V5DecodeResult result = DecodeNextValue(&reader, rice_parameter, &offset);
    if (result != V5DecodeResult::kSuccess) {
      return result;
    }
    T next_value;
    if (!v5_rice_utils::TryAdd(last_value, offset, &next_value)) {
      return V5DecodeResult::kPrefixAccumulationOverflow;
    }
    out->push_back(next_value);
    last_value = next_value;
  }

  return V5DecodeResult::kSuccess;
}

// EXAMPLE (T is uint32_t):
//  - `rice_parameter`: 26 (decimal)
//  - `reader` contents: 1111001110111110000000000000000...
// Decoding steps:
// 11110 01110111110000000000000000 ...
//  > q = 11110 (unary)
//      = 4 (decimal)
//      = 100 (binary)
//  > r = 26 bits LSB-first 0 1 1 1 0 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
//      = 00000000000000001111101110 (binary, MSB-first)
//      = 00000000000000000000001111101110 (pad to 32 bits)
//      = 1006 (decimal)
//  > q_shifted
//      = q << rice_parameter
//      = 100 (binary) << 26 (decimal)
//      = 10000000000000000000000000000 (binary)
//      = 00010000000000000000000000000000 (pad to 32 bits)
//  > q_shifted + r
//      = 00010000000000000000000000000000 +
//        00000000000000000000001111101110
//        --------------------------------
//      = 00010000000000000000001111101110
// Final output = 00010000 00000000 00000011 11101110 = 268,436,462 (decimal)
template <typename T>
V5DecodeResult V5RiceDecoder::DecodeNextValue(
    v5_rice_utils::V5BitReader* reader,
    int rice_parameter,
    T* out) {
  // Decode `q` quotient (unary). For example:
  //  - 0 => q = 0
  //  - 1110 => q = 3
  //  - 111110 => q = 5
  const uint32_t max_valid_q =
      (1u << (v5_rice_utils::V5TypeTraits<T>::kBitWidth - rice_parameter)) - 1;
  uint32_t q = 0;
  while (true) {
    bool bit;
    if (!reader->ReadSingleBit(&bit)) {
      return V5DecodeResult::kRanOutOfBits;
    }
    if (!bit) {
      break;
    }
    q++;
    if (q > max_valid_q) {
      return V5DecodeResult::kQuotientTooLarge;
    }
  }

  // Decode `r` remainder (binary). This is the next `rice_parameter` number of
  // bits.
  T r;
  if (!reader->ReadMultipleBits(rice_parameter, &r)) {
    return V5DecodeResult::kRanOutOfBits;
  }

  // Combine: value = (q << rice_parameter) + r.
  // Because we checked `q > max_valid_q` above, we are mathematically
  // guaranteed that `q << rice_parameter` will not overflow type `T`.
  T q_shifted = static_cast<T>(q) << rice_parameter;
  bool add_succeeded = v5_rice_utils::TryAdd(q_shifted, r, out);
  CHECK(add_succeeded);

  return V5DecodeResult::kSuccess;
}

// Explicit template instantiation for decoder methods. This allows us to keep
// the template definition in this .cc file without causing linker errors for
// external callers.
template V5DecodeResult V5RiceDecoder::DecodeIntegers<uint32_t>(
    uint32_t,
    int,
    int,
    base::span<const uint8_t>,
    std::vector<uint32_t>*);
template V5DecodeResult V5RiceDecoder::DecodeIntegers<uint64_t>(
    uint64_t,
    int,
    int,
    base::span<const uint8_t>,
    std::vector<uint64_t>*);
template V5DecodeResult V5RiceDecoder::DecodeIntegers<absl::uint128>(
    absl::uint128,
    int,
    int,
    base::span<const uint8_t>,
    std::vector<absl::uint128>*);
template V5DecodeResult V5RiceDecoder::DecodeIntegers<v5_rice_utils::Uint256>(
    v5_rice_utils::Uint256,
    int,
    int,
    base::span<const uint8_t>,
    std::vector<v5_rice_utils::Uint256>*);
template V5DecodeResult V5RiceDecoder::DecodePrefixes<uint32_t>(
    uint32_t,
    int,
    int,
    base::span<const uint8_t>,
    std::string*);
template V5DecodeResult V5RiceDecoder::DecodePrefixes<uint64_t>(
    uint64_t,
    int,
    int,
    base::span<const uint8_t>,
    std::string*);
template V5DecodeResult V5RiceDecoder::DecodePrefixes<absl::uint128>(
    absl::uint128,
    int,
    int,
    base::span<const uint8_t>,
    std::string*);
template V5DecodeResult V5RiceDecoder::DecodePrefixes<v5_rice_utils::Uint256>(
    v5_rice_utils::Uint256,
    int,
    int,
    base::span<const uint8_t>,
    std::string*);

}  // namespace safe_browsing
