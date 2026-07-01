// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_RICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_RICE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace safe_browsing {

namespace v5_rice_utils {

// A lightweight 256-bit unsigned integer struct for V5 Rice decoding.
struct Uint256 {
  Uint256() = default;
  explicit Uint256(uint64_t v) : low(v) {}
  Uint256(absl::uint128 high, absl::uint128 low) : low(low), high(high) {}

  Uint256& operator+=(const Uint256& other);
  Uint256 operator+(const Uint256& other) const;
  Uint256& operator<<=(int shift);
  Uint256 operator<<(int shift) const;
  Uint256& operator>>=(int shift);
  Uint256 operator>>(int shift) const;
  Uint256& operator|=(const Uint256& other);
  Uint256 operator|(const Uint256& other) const;

  bool operator==(const Uint256& other) const {
    return high == other.high && low == other.low;
  }

  bool operator!=(const Uint256& other) const { return !(*this == other); }

  // The least significant 128 bits.
  absl::uint128 low = 0;
  // The most significant 128 bits.
  absl::uint128 high = 0;
};

std::ostream& operator<<(std::ostream& os, const Uint256& v);

// Ensure that Uint256 has no padding and is exactly 32 bytes (256 bits).
// This is required for `SerializeToBigEndianBytes` to work correctly.
static_assert(sizeof(Uint256) == 32, "Uint256 must be exactly 32 bytes");

// Type traits to get the bit width of types.
// The bit width is the total number of bits in the representation of type `T`.
// In Golomb-Rice decoding, the Rice parameter `rice_parameter` defines the
// number of bits used to encode the remainder. The min and max values are from
// components/safe_browsing/core/common/proto/safebrowsingv5.proto guarantees.
template <typename T>
struct V5TypeTraits;

template <>
struct V5TypeTraits<uint32_t> {
  static constexpr int kBitWidth = 32;
  static constexpr int kMinRiceParameter = 3;
  static constexpr int kMaxRiceParameter = 30;
};
template <>
struct V5TypeTraits<uint64_t> {
  static constexpr int kBitWidth = 64;
  static constexpr int kMinRiceParameter = 35;
  static constexpr int kMaxRiceParameter = 62;
};
template <>
struct V5TypeTraits<absl::uint128> {
  static constexpr int kBitWidth = 128;
  static constexpr int kMinRiceParameter = 99;
  static constexpr int kMaxRiceParameter = 126;
};
template <>
struct V5TypeTraits<Uint256> {
  static constexpr int kBitWidth = 256;
  static constexpr int kMinRiceParameter = 227;
  static constexpr int kMaxRiceParameter = 254;
};

// Safe addition with overflow detection.
// Returns true on success (no overflow), false on failure (overflow).
template <typename T>
bool TryAdd(T a, T b, T* result);
bool TryAdd(Uint256 a, Uint256 b, Uint256* result);

// BitReader that reads bits from a byte stream, least significant bit first
// within each byte.
class V5BitReader {
 public:
  // Constructs a reader that reads from the given `data` byte span.
  explicit V5BitReader(base::span<const uint8_t> data);

  // Returns true if there is any more data to read.
  bool HasMore() const;

  // Reads a single bit from the stream and writes it to `bit` (output
  // parameter, always non-null).
  // Returns true on success, or false if we ran out of bits.
  bool ReadSingleBit(bool* bit);

  // Reads `num_bits` from the stream and stores the result in `out`.
  // The bits are read least significant bit first.
  // Returns true on success, or false if we ran out of bits.
  template <typename T>
  bool ReadMultipleBits(int num_bits, T* out);

 private:
  // The underlying data stream.
  base::raw_span<const uint8_t> data_;
  // The index of the next byte to read from `data_`.
  size_t byte_index_ = 0;
  // The index of the next bit to read within the current byte (0 to 7).
  int bit_index_ = 0;
};

// Converts a vector of decoded values (in host-endianness) back to a raw
// byte string in big-endianness (network byte order).
//
// Safe Browsing hash prefixes are stored on disk and compared as big-endian
// byte sequences. However, during Rice decoding, arithmetic operations (delta
// additions) must be performed in host byte order. This function is used after
// decoding to convert the host-order integers back to network byte order
// before they are serialized and written to disk.
template <typename T>
std::string SerializeToBigEndianBytes(std::vector<T> decoded);

}  // namespace v5_rice_utils

// Enumerate different results while decoding the Rice-encoded data.
enum class V5DecodeResult {
  // Decoding was successful.
  kSuccess = 0,
  // Adding the deltas caused one of the accumulated prefix values to overflow.
  kPrefixAccumulationOverflow = 1,
  // The bitstream ran out of bits before decoding completed.
  kRanOutOfBits = 2,
  // The decoded quotient was too large and would lose some bits once shifted by
  // the rice parameter.
  kQuotientTooLarge = 3,

  kMaxValue = kQuotientTooLarge
};

// Decoder for Golomb-Rice encoded Safe Browsing V5 database updates.
// See https://en.wikipedia.org/wiki/Golomb_coding.
class V5RiceDecoder {
 public:
  // Decodes the Rice-encoded data in `encoded_data` as a sequence of hash
  // prefixes and writes them as raw big-endian bytes into `out`.
  // `first_value` is the first value in the sequence, in host byte order.
  // `rice_parameter` is the Golomb-Rice parameter used for encoding.
  // `num_entries` is the number of delta-encoded entries to decode.
  // `encoded_data` is the Rice-encoded bitstream.
  // `out` is the output string to write the decoded raw big-endian bytes into.
  // Returns `V5DecodeResult::kSuccess` on success, or an error code on failure.
  template <typename T>
  static V5DecodeResult DecodePrefixes(T first_value,
                                       int rice_parameter,
                                       int num_entries,
                                       base::span<const uint8_t> encoded_data,
                                       std::string* out);

  // Decodes the Rice-encoded data in `encoded_data` and writes the decoded
  // values to `out`.
  // `first_value` is the starting value of the sequence.
  // `rice_parameter` is the Golomb-Rice parameter used for encoding.
  // `num_entries` is the number of delta-encoded entries to decode.
  // `encoded_data` is the Rice-encoded bitstream.
  // `out` is the output vector to write the decoded values to. Must be empty.
  // Returns `V5DecodeResult::kSuccess` on success, or an error code on failure.
  template <typename T>
  static V5DecodeResult DecodeIntegers(T first_value,
                                       int rice_parameter,
                                       int num_entries,
                                       base::span<const uint8_t> encoded_data,
                                       std::vector<T>* out);

 private:
  // Decodes the next single value from `reader` using `rice_parameter` and
  // stores it in `out`.
  // Returns `V5DecodeResult::kSuccess` on success, or an error code on failure.
  template <typename T>
  static V5DecodeResult DecodeNextValue(v5_rice_utils::V5BitReader* reader,
                                        int rice_parameter,
                                        T* out);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_RICE_H_
