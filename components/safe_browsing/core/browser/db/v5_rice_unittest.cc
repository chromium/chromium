// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_rice.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace safe_browsing {

using v5_rice_utils::SerializeToBigEndianBytes;
using v5_rice_utils::TryAdd;
using v5_rice_utils::Uint256;
using v5_rice_utils::V5BitReader;

namespace {

// =============================================================================
// Helper Methods to Support V5 Rice Decoder Tests
// =============================================================================

// Appends a single bit to the byte stream, LSB-first.
void WriteBit(bool bit, std::vector<uint8_t>& stream, size_t& bit_index) {
  size_t byte_idx = bit_index / 8;
  size_t bit_offset = bit_index % 8;
  if (byte_idx >= stream.size()) {
    stream.resize(byte_idx + 1, 0);
  }
  if (bit) {
    stream[byte_idx] |= (1 << bit_offset);
  }
  bit_index++;
}

// Returns the bit at the given index from the value (LSB is index 0).
template <typename T>
bool GetBit(const T& val, int bit_idx) {
  return ((val >> bit_idx) & 1) != 0;
}
template <>
bool GetBit(const Uint256& val, int bit_idx) {
  Uint256 shifted = val >> bit_idx;
  return (shifted.low & 1) != 0;
}

// Helper to cast or truncate a value to uint32_t.
template <typename T>
uint32_t ToUint32(const T& val) {
  return static_cast<uint32_t>(val);
}
template <>
uint32_t ToUint32(const Uint256& val) {
  return static_cast<uint32_t>(val.low);
}

// Returns the maximum representable value for type T.
template <typename T>
T GetMaxVal() {
  return std::numeric_limits<T>::max();
}
template <>
absl::uint128 GetMaxVal<absl::uint128>() {
  return ~absl::uint128(0);
}
template <>
Uint256 GetMaxVal<Uint256>() {
  return Uint256(GetMaxVal<absl::uint128>(), GetMaxVal<absl::uint128>());
}

// Subtraction (only used for tests).
template <typename T>
T SubtractDelta(T val, T delta) {
  return val - delta;
}
template <>
Uint256 SubtractDelta<Uint256>(Uint256 val, Uint256 delta) {
  absl::uint128 low = val.low - delta.low;
  absl::uint128 high = val.high - delta.high;
  if (val.low < delta.low) {
    high -= 1;
  }
  return Uint256(high, low);
}

// Conversion helpers.
template <typename T>
T ConvertTo(uint64_t v) {
  return static_cast<T>(v);
}
template <>
Uint256 ConvertTo<Uint256>(uint64_t v) {
  return Uint256(v);
}

// Encodes a single delta value using Rice-Golomb coding.
// Example: delta = 23 (binary '10111'), rice_parameter = 4
//    -> quotient = 1 (unary '10'), remainder = 7 (LSB '1110').
// Writes bits '101110'.
template <typename T>
void EncodeRiceDelta(T delta,
                     int rice_parameter,
                     std::vector<uint8_t>& stream,
                     size_t& bit_index) {
  T q_val = delta >> rice_parameter;
  uint32_t q = ToUint32(q_val);
  // Fill in the unary q.
  for (uint32_t i = 0; i < q; ++i) {
    WriteBit(/*bit=*/true, stream, bit_index);
  }
  WriteBit(/*bit=*/false, stream, bit_index);
  // Fill in the rest.
  for (int i = 0; i < rice_parameter; ++i) {
    WriteBit(GetBit(delta, i), stream, bit_index);
  }
}

// Verifies that DecodeIntegers decodes the stream and compares to expected
// values.
template <typename T>
void VerifyDecode(T first_value,
                  int rice_parameter,
                  size_t num_entries,
                  const std::vector<uint8_t>& stream,
                  V5DecodeResult expected_result,
                  const std::vector<T>& expected_values = {}) {
  std::vector<T> decoded_ints;
  V5DecodeResult res = V5RiceDecoder::DecodeIntegers(
      first_value, rice_parameter, num_entries, stream, &decoded_ints);
  EXPECT_EQ(res, expected_result);
  if (expected_result == V5DecodeResult::kSuccess) {
    EXPECT_EQ(decoded_ints, expected_values);
  }
}

// Verifies that DecodePrefixes decodes and compares to expected bytes.
template <typename T>
void VerifyDecodePrefixes(T first_value,
                          int rice_parameter,
                          size_t num_entries,
                          base::span<const uint8_t> stream,
                          V5DecodeResult expected_result,
                          base::span<const uint8_t> expected_bytes = {}) {
  std::string buffer;
  V5DecodeResult res = V5RiceDecoder::DecodePrefixes(
      first_value, rice_parameter, num_entries, stream, &buffer);
  EXPECT_EQ(res, expected_result);
  if (expected_result == V5DecodeResult::kSuccess) {
    EXPECT_EQ(expected_bytes, base::as_byte_span(buffer));
  }
}

}  // namespace

// =============================================================================
// Helper Class / Method Unit Tests
// =============================================================================

TEST(V5RiceUint256Test, ConstructorsAndEquality) {
  // Test constructor and equality
  Uint256 a1(/*high=*/absl::MakeUint128(/*high=*/1, /*low=*/2),
             /*low=*/absl::MakeUint128(/*high=*/3, /*low=*/4));
  EXPECT_EQ(a1.high, absl::MakeUint128(/*high=*/1, /*low=*/2));
  EXPECT_EQ(a1.low, absl::MakeUint128(/*high=*/3, /*low=*/4));

  Uint256 a2(/*high=*/absl::MakeUint128(/*high=*/1, /*low=*/2),
             /*low=*/absl::MakeUint128(/*high=*/3, /*low=*/4));
  EXPECT_EQ(a1, a2);

  Uint256 b(/*high=*/absl::MakeUint128(/*high=*/1, /*low=*/2),
            /*low=*/absl::MakeUint128(/*high=*/3, /*low=*/5));
  // Explicitly test comparison operators on unequal objects.
  EXPECT_TRUE(a1 != b);
  EXPECT_FALSE(a1 == b);

  // Test copy constructor
  Uint256 a3(a1);
  EXPECT_EQ(a1, a3);

  // Test default constructor
  Uint256 c;
  EXPECT_EQ(c.high, absl::uint128(0));
  EXPECT_EQ(c.low, absl::uint128(0));
}

TEST(V5RiceUint256Test, AdditionOperators) {
  Uint256 a(/*high=*/1, /*low=*/2);
  Uint256 b(/*high=*/3, /*low=*/4);

  Uint256 c = a + b;
  EXPECT_EQ(c.high, absl::uint128(4));
  EXPECT_EQ(c.low, absl::uint128(6));

  c += b;
  EXPECT_EQ(c.high, absl::uint128(7));
  EXPECT_EQ(c.low, absl::uint128(10));
}

TEST(V5RiceUint256Test, AdditionCarryLogic) {
  absl::uint128 max_uint128 = GetMaxVal<absl::uint128>();
  Uint256 a(/*high=*/0, /*low=*/max_uint128);
  Uint256 one(/*high=*/0, /*low=*/1);
  Uint256 b = a + one;
  EXPECT_EQ(b.high, absl::uint128(1));
  EXPECT_EQ(b.low, absl::uint128(0));

  Uint256 c(/*high=*/5, /*low=*/max_uint128);
  Uint256 d = c + one;
  EXPECT_EQ(d.high, absl::uint128(6));
  EXPECT_EQ(d.low, absl::uint128(0));

  Uint256 max_u256 = GetMaxVal<Uint256>();
  Uint256 e = max_u256 + one;
  EXPECT_EQ(e.high, absl::uint128(0));
  EXPECT_EQ(e.low, absl::uint128(0));
}

TEST(V5RiceUint256Test, BitwiseOr) {
  Uint256 a(/*high=*/absl::MakeUint128(/*high=*/0xF0F0, /*low=*/0),
            /*low=*/absl::MakeUint128(/*high=*/0, /*low=*/0xF0F0));
  Uint256 b(/*high=*/absl::MakeUint128(/*high=*/0x0F0F, /*low=*/0x0F0F),
            /*low=*/absl::MakeUint128(/*high=*/0x0F0F, /*low=*/0));
  Uint256 c1 = a | b;
  EXPECT_EQ(c1.high, absl::MakeUint128(/*high=*/0xFFFF, /*low=*/0x0F0F));
  EXPECT_EQ(c1.low, absl::MakeUint128(/*high=*/0x0F0F, /*low=*/0xF0F0));

  Uint256 c2 = a;
  c2 |= b;
  EXPECT_EQ(c2, c1);
}

TEST(V5RiceUint256Test, LeftShift) {
  const uint64_t p1 = 0x9988776655443322ULL;
  const uint64_t p2 = 0xAABBCCDDEEFF0011ULL;
  const uint64_t p3 = 0x1234567890ABCDEFULL;
  const uint64_t p4 = 0x1122334455667788ULL;

  Uint256 a(/*high=*/absl::MakeUint128(/*high=*/p1, /*low=*/p2),
            /*low=*/absl::MakeUint128(/*high=*/p3, /*low=*/p4));

  // shift by 0
  EXPECT_EQ(a << 0, a);

  // shift by non-multiple of 64 (8 bits)
  Uint256 b = a << 8;
  EXPECT_EQ(b.high,
            absl::MakeUint128((p1 << 8) | (p2 >> 56), (p2 << 8) | (p3 >> 56)));
  EXPECT_EQ(b.low, absl::MakeUint128((p3 << 8) | (p4 >> 56), p4 << 8));

  // shift by < 128
  Uint256 c = a << 64;
  EXPECT_EQ(c.high, absl::MakeUint128(/*high=*/p2, /*low=*/p3));
  EXPECT_EQ(c.low, absl::MakeUint128(/*high=*/p4, /*low=*/0));

  // shift by 128
  Uint256 d = a << 128;
  EXPECT_EQ(d.high, absl::MakeUint128(/*high=*/p3, /*low=*/p4));
  EXPECT_EQ(d.low, absl::uint128(0));

  // shift by > 128
  Uint256 e = a << 192;
  EXPECT_EQ(e.high, absl::MakeUint128(/*high=*/p4, /*low=*/0));
  EXPECT_EQ(e.low, absl::uint128(0));

  // shift by >= 256
  Uint256 f = a << 256;
  EXPECT_EQ(f.high, absl::uint128(0));
  EXPECT_EQ(f.low, absl::uint128(0));

  EXPECT_EQ(a << 300, Uint256(/*high=*/0, /*low=*/0));
}

TEST(V5RiceUint256Test, RightShift) {
  const uint64_t p1 = 0x1234567890ABCDEFULL;
  const uint64_t p2 = 0x1122334455667788ULL;
  const uint64_t p3 = 0x9988776655443322ULL;
  const uint64_t p4 = 0xAABBCCDDEEFF0011ULL;

  Uint256 a(/*high=*/absl::MakeUint128(/*high=*/p1, /*low=*/p2),
            /*low=*/absl::MakeUint128(/*high=*/p3, /*low=*/p4));

  // shift by 0
  EXPECT_EQ(a >> 0, a);

  // shift by non-multiple of 64 (8 bits)
  Uint256 b = a >> 8;
  EXPECT_EQ(b.high, absl::MakeUint128(p1 >> 8, (p1 << 56) | (p2 >> 8)));
  EXPECT_EQ(b.low,
            absl::MakeUint128((p2 << 56) | (p3 >> 8), (p3 << 56) | (p4 >> 8)));

  // shift by < 128
  Uint256 c = a >> 64;
  EXPECT_EQ(c.high, absl::MakeUint128(/*high=*/0, /*low=*/p1));
  EXPECT_EQ(c.low, absl::MakeUint128(/*high=*/p2, /*low=*/p3));

  // shift by 128
  Uint256 d = a >> 128;
  EXPECT_EQ(d.high, absl::uint128(0));
  EXPECT_EQ(d.low, absl::MakeUint128(/*high=*/p1, /*low=*/p2));

  // shift by > 128
  Uint256 e = a >> 192;
  EXPECT_EQ(e.high, absl::uint128(0));
  EXPECT_EQ(e.low, absl::MakeUint128(/*high=*/0, /*low=*/p1));

  // shift by >= 256
  Uint256 f = a >> 256;
  EXPECT_EQ(f.high, absl::uint128(0));
  EXPECT_EQ(f.low, absl::uint128(0));

  EXPECT_EQ(a >> 300, Uint256(/*high=*/0, /*low=*/0));
}

TEST(V5RiceTryAddTest, TryAdd) {
  // uint32_t
  uint32_t result_u32;
  uint32_t a_u32 = 10;
  uint32_t b_u32 = 20;
  EXPECT_TRUE(TryAdd(a_u32, b_u32, &result_u32));
  EXPECT_EQ(result_u32, 30u);
  EXPECT_FALSE(TryAdd(0xFFFFFFFFu, 1u, &result_u32));

  // uint64_t
  uint64_t result_u64;
  uint64_t a_u64 = 10;
  uint64_t b_u64 = 20;
  EXPECT_TRUE(TryAdd(a_u64, b_u64, &result_u64));
  EXPECT_EQ(result_u64, 30ULL);
  uint64_t max_u64 = 0xFFFFFFFFFFFFFFFFULL;
  uint64_t one_u64 = 1ULL;
  EXPECT_FALSE(TryAdd(max_u64, one_u64, &result_u64));

  // absl::uint128
  absl::uint128 result_u128;
  absl::uint128 max_u128 = GetMaxVal<absl::uint128>();
  absl::uint128 a_u128 = 10;
  absl::uint128 b_u128 = 20;
  EXPECT_TRUE(TryAdd(a_u128, b_u128, &result_u128));
  EXPECT_EQ(result_u128, absl::uint128(30));
  EXPECT_FALSE(TryAdd(max_u128, absl::uint128(1), &result_u128));

  // Uint256
  Uint256 result_u256;
  Uint256 max_u256 = GetMaxVal<Uint256>();
  EXPECT_TRUE(TryAdd(Uint256(/*high=*/0, /*low=*/10),
                     Uint256(/*high=*/0, /*low=*/20), &result_u256));
  EXPECT_EQ(result_u256, Uint256(/*high=*/0, /*low=*/30));
  EXPECT_FALSE(TryAdd(max_u256, Uint256(/*high=*/0, /*low=*/1), &result_u256));

  // Test carry overflow where there is carrying but no overflow
  EXPECT_TRUE(TryAdd(Uint256(/*high=*/0, max_u128),
                     Uint256(/*high=*/0, /*low=*/1), &result_u256));
  EXPECT_EQ(result_u256, Uint256(/*high=*/1, /*low=*/0));
}

TEST(V5RiceBitReaderTest, ReadBits) {
  std::vector<uint8_t> data = {0xAC, 0x0F};
  V5BitReader reader(data);

  EXPECT_TRUE(reader.HasMore());

  // 0xAC = 10101100 -> LSB first: 0, 0, 1, 1, 0, 1, 0, 1
  // 0x0F = 00001111 -> LSB first: 1, 1, 1, 1, 0, 0, 0, 0
  const std::vector<int> expected_bits = {
      0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
  };

  for (size_t i = 0; i < expected_bits.size(); ++i) {
    bool bit;
    EXPECT_TRUE(reader.ReadSingleBit(&bit))
        << "Failed to read bit at index " << i;
    EXPECT_EQ(static_cast<int>(bit), expected_bits[i])
        << "Mismatch at bit index " << i;
  }

  EXPECT_FALSE(reader.HasMore());
  bool bit;
  EXPECT_FALSE(reader.ReadSingleBit(&bit));
}

TEST(V5RiceBitReaderTest, ReadMultipleBits) {
  std::vector<uint8_t> data = {0xAC, 0x0F};  // 00110101 11110000
  V5BitReader reader(data);

  uint32_t val32;
  // Read 4 bits: 0, 0, 1, 1 -> 12 (binary 1100)
  EXPECT_TRUE(reader.ReadMultipleBits(4, &val32));
  EXPECT_EQ(val32, 12u);

  // Read 6 bits: 0, 1, 0, 1 from Byte 0 (bits index 4-7) then 1, 1 from Byte 1
  // (bits 0-1) Value = 58
  EXPECT_TRUE(reader.ReadMultipleBits(6, &val32));
  EXPECT_EQ(val32, 58u);

  // Try to read 7 bits (only 6 left)
  EXPECT_FALSE(reader.ReadMultipleBits(7, &val32));
}

TEST(V5RiceBitReaderTest, EmptyReader) {
  V5BitReader reader({});
  EXPECT_FALSE(reader.HasMore());
  bool bit;
  EXPECT_FALSE(reader.ReadSingleBit(&bit));
  uint32_t val;
  EXPECT_FALSE(reader.ReadMultipleBits(1, &val));
}

TEST(V5RiceBitReaderTest, HasMoreWithRefill) {
  std::vector<uint8_t> data = {0xFF, 0xFF, 0xFF, 0xFF,
                               0xFF};  // 5 bytes (40 bits)
  V5BitReader reader(data);

  // Read 10 bits with single bit API.
  bool bit;
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(reader.HasMore());
    EXPECT_TRUE(reader.ReadSingleBit(&bit));
  }

  // Read 20 bits with multiple bits API.
  uint32_t val;
  EXPECT_TRUE(reader.HasMore());
  EXPECT_TRUE(reader.ReadMultipleBits(20, &val));
  EXPECT_EQ(val, static_cast<uint32_t>(0xFFFFF));

  // Read last 10 bits with single bit API (will trigger refill on 33rd bit).
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(reader.HasMore());
    EXPECT_TRUE(reader.ReadSingleBit(&bit));
  }

  EXPECT_FALSE(reader.HasMore());
  EXPECT_FALSE(reader.ReadSingleBit(&bit));
  uint32_t val_end;
  EXPECT_FALSE(reader.ReadMultipleBits(1, &val_end));
}

TEST(V5RiceBitReaderTest, ReadMultipleBitsCrossBoundary) {
  // 16 bytes of data (128 bits), all 1s except a few markers
  std::vector<uint8_t> data = {
      0xFF, 0xFF, 0xFF, 0xFF,  // All ones.
      0xFF, 0xFF, 0xFF, 0xAF,  // 60 ones, then 0101 (MSBs of byte 7).
      0xF6, 0xFF, 0xFF, 0xFF,  // 0110 (LSBs of byte 8), then all ones.
      0xFF, 0xFF, 0xFF, 0xFF   // All ones.
  };
  V5BitReader reader(data);

  uint64_t val64;
  // Consume 60 bits.
  EXPECT_TRUE(reader.ReadMultipleBits(60, &val64));

  // Request 12 bits. This should take 4 bits (0101) from first block,
  // and 8 bits (0110 + ones) from next block.
  // Result should be 0xF6A (binary 111101101010).
  uint32_t val32;
  EXPECT_TRUE(reader.ReadMultipleBits(12, &val32));
  EXPECT_EQ(val32, 0xF6Au);
}

TEST(V5RiceBitReaderTest, ReadMultipleBitsUint256) {
  // 32 bytes of 0xFF (256 bits of 1s)
  std::vector<uint8_t> data(32, 0xFF);
  V5BitReader reader(data);

  Uint256 val;
  // Read 250 bits.
  EXPECT_TRUE(reader.ReadMultipleBits(250, &val));

  // Verify that the lowest 250 bits are 1, and the top 6 bits are 0.
  Uint256 expected = GetMaxVal<Uint256>() >> 6;
  EXPECT_EQ(val, expected);
}

TEST(V5RiceBitReaderTest, TruncatedStreamUint256) {
  std::vector<uint8_t> data(4, 0xFF);  // Only 32 bits available
  V5BitReader reader(data);

  Uint256 val;
  // Try to read 250 bits. Should fail.
  EXPECT_FALSE(reader.ReadMultipleBits(250, &val));
}

TEST(V5RiceBitReaderTest, TruncatedStreamFirstRead) {
  std::vector<uint8_t> data = {0xFF, 0x00};  // Only 16 bits available
  V5BitReader reader(data);

  uint32_t val;
  // Try to read 20 bits immediately. It should fail.
  EXPECT_FALSE(reader.ReadMultipleBits(20, &val));
}

TEST(V5RiceBitReaderTest, TruncatedStreamEmptyRefill) {
  std::vector<uint8_t> data = {0xFF, 0x00};  // Only 16 bits available
  V5BitReader reader(data);

  uint32_t val;
  // 1. Read 10 bits. Should succeed.
  EXPECT_TRUE(reader.ReadMultipleBits(10, &val));

  // 2. Try to read 10 more bits. Only 6 left, so it should fail.
  EXPECT_FALSE(reader.ReadMultipleBits(10, &val));
}

TEST(V5RiceBitReaderTest, TruncatedStreamPartialRefill) {
  std::vector<uint8_t> data = {0xFF, 0xFF, 0xFF, 0xFF,
                               0xFF};  // 5 bytes (40 bits)
  V5BitReader reader(data);

  uint32_t val;
  // 1. Read 20 bits. Should succeed (leaves 12 in buffer, 8 in stream).
  EXPECT_TRUE(reader.ReadMultipleBits(20, &val));

  // 2. Try to read 22 bits. Should fail after a partial refill of 8 bits.
  EXPECT_FALSE(reader.ReadMultipleBits(22, &val));
}

TEST(V5RiceBitReaderTest, MixedReads) {
  std::vector<uint8_t> data = {0xAC, 0x0F};  // In LSB order: 00110101 11110000
  V5BitReader reader(data);

  bool bit;
  uint32_t val;

  // Read 2 single bits (0, 0)
  EXPECT_TRUE(reader.ReadSingleBit(&bit));
  EXPECT_FALSE(bit);
  EXPECT_TRUE(reader.ReadSingleBit(&bit));
  EXPECT_FALSE(bit);

  // Read 4 bits (1, 1, 0, 1) -> 11 (binary 1011)
  EXPECT_TRUE(reader.ReadMultipleBits(4, &val));
  EXPECT_EQ(val, 11u);

  // Read 2 single bits (0, 1)
  EXPECT_TRUE(reader.ReadSingleBit(&bit));
  EXPECT_FALSE(bit);
  EXPECT_TRUE(reader.ReadSingleBit(&bit));
  EXPECT_TRUE(bit);

  // Read 4 bits (1, 1, 1, 1) -> 15 (binary 1111)
  EXPECT_TRUE(reader.ReadMultipleBits(4, &val));
  EXPECT_EQ(val, 15u);

  // Read remaining 4 bits (0, 0, 0, 0) -> 0
  EXPECT_TRUE(reader.ReadMultipleBits(4, &val));
  EXPECT_EQ(val, 0u);

  EXPECT_FALSE(reader.HasMore());
}

TEST(V5RiceSerializeTest, SerializeToBigEndianBytes) {
  // Test converting decoded values to big-endian raw bytes string.
  // For uint32_t:
  std::vector<uint32_t> vec32 = {0x12345678, 0x9ABCDEF0};
  std::string res32 = SerializeToBigEndianBytes(vec32);
  std::string expected32 =
      std::string("\x12\x34\x56\x78\x9A\xBC\xDE\xF0", /*n=*/8);
  EXPECT_EQ(res32, expected32);

  // For uint64_t:
  std::vector<uint64_t> vec64 = {0x1234567890ABCDEFULL};
  std::string res64 = SerializeToBigEndianBytes(vec64);
  std::string expected64 =
      std::string("\x12\x34\x56\x78\x90\xAB\xCD\xEF", /*n=*/8);
  EXPECT_EQ(res64, expected64);

  // For absl::uint128:
  std::vector<absl::uint128> vec128 = {
      absl::MakeUint128(/*high=*/0x0102030405060708ULL,
                        /*low=*/0x090A0B0C0D0E0F10ULL)};
  std::string res128 = SerializeToBigEndianBytes(vec128);
  std::string expected128 = std::string(
      "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10",
      /*n=*/16);
  EXPECT_EQ(res128, expected128);

  // For Uint256:
  std::vector<Uint256> decoded256 = {
      Uint256(absl::MakeUint128(/*high=*/0x0102030405060708ULL,
                                /*low=*/0x090A0B0C0D0E0F10ULL),
              absl::MakeUint128(/*high=*/0x1112131415161718ULL,
                                /*low=*/0x191A1B1C1D1E1F20ULL))};
  std::string bytes256 = SerializeToBigEndianBytes(decoded256);
  std::string expected256 =
      "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
      "\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20";
  EXPECT_EQ(expected256, bytes256);
}

// =============================================================================
// V5RiceDecoderTest
// =============================================================================

TEST(V5RiceDecoderTest, DecodeUint256SerializationLayout) {
  // Test decoding where both high and low halves of Uint256 are non-zero.
  // This ensures we catch any swap of high/low halves during serialization.
  //
  // first_value = Uint256(high=1, low=2)
  // offsets = [1, 2]
  // Decoded values (logical):
  // 1. Uint256(1, 2)
  // 2. Uint256(1, 3)
  // 3. Uint256(1, 5)
  //
  // Expected big-endian bytes (32 bytes per entry, total 96 bytes):
  // Entry 1: [Big-endian of 1 (16 bytes)] [Big-endian of 2 (16 bytes)]
  //          Since big-endian stores the most significant bytes first, the
  //          value `1` (stored in the high 16 bytes of Uint256) will have its
  //          non-zero byte `0x01` at index 15. The value `2` (stored in the low
  //          16 bytes of Uint256) will have its non-zero byte `0x02` at
  //          index 31.
  // Entry 2: [Big-endian of 1 (16 bytes)] [Big-endian of 3 (16 bytes)]
  //          -> 0x01 at index 47, 0x03 at index 63
  // Entry 3: [Big-endian of 1 (16 bytes)] [Big-endian of 5 (16 bytes)]
  //          -> 0x01 at index 79, 0x05 at index 95
  std::vector<uint8_t> expected(96, 0);
  expected[15] = 0x01;
  expected[31] = 0x02;
  expected[47] = 0x01;
  expected[63] = 0x03;
  expected[79] = 0x01;
  expected[95] = 0x05;

  Uint256 first_value(/*high=*/1, /*low=*/2);

  // Rice-encoded bitstream for offsets [1, 2] with parameter 227.
  // - offset1 = 1 (q=0, r=1) -> bits: [q:0][r:1,0,0...] -> Byte 0 = 0x02.
  //   (takes 1 + 227 = 228 bits, occupying bits 0-227).
  // - offset2 = 2 (q=0, r=2) starts at bit 228 (Byte 28 bit 4)
  //   -> bits: [q:0][r:0,1,0...] -> Byte 28 = 0x40.
  //   (takes 1 + 227 = 228 bits, occupying bits 228-455).
  // - Total bits: 228 * 2 = 456 bits -> 57 bytes.
  std::string encoded_data(57, 0);
  encoded_data[0] = 0x02;
  encoded_data[28] = 0x40;
  auto encoded_span = base::as_byte_span(encoded_data);

  VerifyDecodePrefixes<Uint256>(first_value, /*rice_parameter=*/227,
                                /*num_entries=*/2, encoded_span,
                                /*expected_result=*/V5DecodeResult::kSuccess,
                                /*expected_bytes=*/expected);
}

TEST(V5RiceDecoderTest, V5InterestingValues) {
  // Test uint32_t cases.
  struct Uint32TestCase {
    uint32_t first_value;
    int rice_parameter;
    int num_entries;
    std::string stream;
    std::vector<uint8_t> expected_bytes;
  };

  // Most of these values match the unit test values used within Google to
  // test this code in other components, such as the SafeBrowsing service
  // itself.
  std::vector<Uint32TestCase> uint32_cases = {
      {/*first_value=*/489866504,
       /*rice_parameter=*/30,
       /*num_entries=*/2,
       /*stream=*/std::string("t\000\322\227\033\355It\000", /*n=*/9),
       /*expected_bytes=*/
       {0x1D, 0x32, 0xC5, 0x08, 0x29, 0x1B, 0xC5, 0x42, 0xF7, 0xA5, 0x02,
        0xE5}},

      {/*first_value=*/0x1d7d1d75,
       /*rice_parameter=*/30,
       /*num_entries=*/2,
       /*stream=*/std::string("\xf1\x4e\x08\x33\xeb\x16\xa6\x41\x00", /*n=*/9),
       /*expected_bytes=*/
       {0x1D, 0x7D, 0x1D, 0x75, 0x6A, 0x3F, 0x31, 0x31, 0xF2, 0x73, 0xF4,
        0x0E}},

      {/*first_value=*/0x30303030,
       /*rice_parameter=*/3,
       /*num_entries=*/2,
       /*stream=*/std::string("\x22", /*n=*/1),
       /*expected_bytes=*/
       {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x30, 0x30, 0x30,
        0x32}},

      {/*first_value=*/0x30303030,
       /*rice_parameter=*/7,
       /*num_entries=*/2,
       /*stream=*/std::string("\x02\x02", /*n=*/2),
       /*expected_bytes=*/
       {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x30, 0x30, 0x30,
        0x32}},

      {/*first_value=*/0x30303030,
       /*rice_parameter=*/8,
       /*num_entries=*/2,
       /*stream=*/std::string("\x02\x04\x00", /*n=*/3),
       /*expected_bytes=*/
       {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x30, 0x30, 0x30,
        0x32}},

      // This one is from the code-comment example in v5_rice.cc.
      {/*first_value=*/0, /*rice_parameter=*/26, /*num_entries=*/1,
       /*stream=*/std::string("\xcf\x7d\x00\x00", /*n=*/4),
       /*expected_bytes=*/{0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0xEE}}};

  for (const auto& tc : uint32_cases) {
    VerifyDecodePrefixes<uint32_t>(tc.first_value, tc.rice_parameter,
                                   tc.num_entries,
                                   base::as_byte_span(tc.stream),
                                   V5DecodeResult::kSuccess, tc.expected_bytes);
  }

  // Test uint64_t case.
  VerifyDecodePrefixes<uint64_t>(
      /*first_value=*/2103960615330909784ULL,
      /*rice_parameter=*/62,
      /*num_entries=*/2,
      /*stream=*/
      base::as_byte_span(std::string(
          "\352\215\315\251s\000\322\227\313cq{\032\355It\000", /*n=*/17)),
      V5DecodeResult::kSuccess,
      /*expected_bytes=*/
      {0x1D, 0x32, 0xC5, 0x08, 0x4A, 0x36, 0x0E, 0x58, 0x29, 0x1B, 0xC5, 0x42,
       0x1F, 0x1C, 0xD5, 0x4D, 0xF7, 0xA5, 0x02, 0xE5, 0x6E, 0x8B, 0x01, 0xC6});

  // Test absl::uint128 case.
  VerifyDecodePrefixes<absl::uint128>(
      /*first_value=*/absl::MakeUint128(2103960615330909784ULL,
                                        17417795843993004048ULL),
      /*rice_parameter=*/126,
      /*num_entries=*/2,
      /*stream=*/
      base::as_byte_span(std::string(
          "R\365\330\333\230\266\356O\351\215\315\251s\000\322\227\203\010\375"
          "\005\372\366\242\023\312cq{\032\355It\000",
          /*n=*/33)),
      V5DecodeResult::kSuccess,
      /*expected_bytes=*/
      {0x1D, 0x32, 0xC5, 0x08, 0x4A, 0x36, 0x0E, 0x58, 0xF1, 0xB8, 0x71, 0x09,
       0x63, 0x7A, 0x68, 0x10, 0x29, 0x1B, 0xC5, 0x42, 0x1F, 0x1C, 0xD5, 0x4D,
       0x99, 0xAF, 0xCC, 0x55, 0xD1, 0x66, 0xE2, 0xB9, 0xF7, 0xA5, 0x02, 0xE5,
       0x6E, 0x8B, 0x01, 0xC6, 0xDC, 0x24, 0x2B, 0x35, 0x12, 0x26, 0x83, 0xC9});

  // Test Uint256 case.
  VerifyDecodePrefixes<Uint256>(
      /*first_value=*/Uint256(
          absl::MakeUint128(2103960615330909784ULL, 17417795843993004048ULL),
          absl::MakeUint128(12442768094943213214ULL, 10311063094514325004ULL)),
      /*rice_parameter=*/254,
      /*num_entries=*/2,
      /*stream=*/
      base::as_byte_span(std::string(
          "\240\343\367\006\300\263w\035\244\312\303\207\217Y)\243R\365\330"
          "\333\230\266\356O\351\215\315\251s\000\322\227;9ft\227\236\267\260"
          "=\215N\316W\034\326\240~\010\375\005\372\366\242\023\312cq{\032"
          "\355It\000",
          /*n=*/65)),
      V5DecodeResult::kSuccess,
      /*expected_bytes=*/
      {0x1D, 0x32, 0xC5, 0x08, 0x4A, 0x36, 0x0E, 0x58, 0xF1, 0xB8, 0x71, 0x09,
       0x63, 0x7A, 0x68, 0x10, 0xAC, 0xAD, 0x97, 0xA8, 0x61, 0xA7, 0x76, 0x9E,
       0x8F, 0x18, 0x41, 0x41, 0x0D, 0x2A, 0x96, 0x0C, 0x29, 0x1B, 0xC5, 0x42,
       0x1F, 0x1C, 0xD5, 0x4D, 0x99, 0xAF, 0xCC, 0x55, 0xD1, 0x66, 0xE2, 0xB9,
       0xFE, 0x42, 0x44, 0x70, 0x25, 0x89, 0x5B, 0xF0, 0x9D, 0xD4, 0x1B, 0x21,
       0x10, 0xA6, 0x87, 0xDC, 0xF7, 0xA5, 0x02, 0xE5, 0x6E, 0x8B, 0x01, 0xC6,
       0xDC, 0x24, 0x2B, 0x35, 0x12, 0x26, 0x83, 0xC9, 0xD2, 0x5D, 0x07, 0xFB,
       0x1F, 0x53, 0x2D, 0x98, 0x53, 0xEB, 0x0E, 0xF3, 0xFF, 0x33, 0x4F, 0x03});
}

// =============================================================================
// V5RiceDecoderTypedTest (Typed Tests)
// =============================================================================

template <typename T>
class V5RiceDecoderTypedTest : public ::testing::Test {};

using V5RiceTypes =
    ::testing::Types<uint32_t, uint64_t, absl::uint128, Uint256>;
TYPED_TEST_SUITE(V5RiceDecoderTypedTest, V5RiceTypes);

TYPED_TEST(V5RiceDecoderTypedTest, NumEntriesZero) {
  using T = TypeParam;
  T first_value = ConvertTo<T>(100);
  VerifyDecode<T>(
      first_value,
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/0, /*stream=*/{},
      /*expected_result=*/V5DecodeResult::kSuccess,
      /*expected_values=*/{first_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, NumEntriesZeroAndRiceParameterZero) {
  using T = TypeParam;
  T first_value = ConvertTo<T>(100);
  VerifyDecode<T>(first_value,
                  /*rice_parameter=*/0,
                  /*num_entries=*/0, /*stream=*/{},
                  /*expected_result=*/V5DecodeResult::kSuccess,
                  /*expected_values=*/{first_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, NumEntriesOne) {
  using T = TypeParam;
  T first_value = ConvertTo<T>(100);
  T delta = ConvertTo<T>(50);
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
                  stream, bit_index);
  T expected_second_value = ConvertTo<T>(150);
  VerifyDecode<T>(
      first_value,
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/1, stream,
      /*expected_result=*/V5DecodeResult::kSuccess,
      /*expected_values=*/{first_value, expected_second_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, FirstValueZero) {
  using T = TypeParam;
  T first_value = ConvertTo<T>(0);
  T delta = ConvertTo<T>(10);
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
                  stream, bit_index);
  T expected_second_value = ConvertTo<T>(10);
  VerifyDecode<T>(
      first_value,
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/1, stream,
      /*expected_result=*/V5DecodeResult::kSuccess,
      /*expected_values=*/{first_value, expected_second_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, StreamEmpty) {
  using T = TypeParam;
  VerifyDecode<T>(
      /*first_value=*/ConvertTo<T>(100),
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/1, /*stream=*/{},
      /*expected_result=*/V5DecodeResult::kRanOutOfBits);
}

TYPED_TEST(V5RiceDecoderTypedTest, StreamEndsDuringQuotientDecoding) {
  using T = TypeParam;
  // The stream is all 1s (0xFF), so we run out of bits trying to read the unary
  // quotient 'q' before finding the 0 bit delimiter.
  VerifyDecode<T>(
      /*first_value=*/ConvertTo<T>(100),
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/1, /*stream=*/{0xFF},
      /*expected_result=*/V5DecodeResult::kRanOutOfBits);
}

TYPED_TEST(V5RiceDecoderTypedTest, StreamTooShortForR) {
  using T = TypeParam;
  // The stream has 1 byte made up of 2 bits ('01') and 6 padding 0s. After
  // decoding q=0 (1 bit consumed), 7 bits remain in the byte. We force the
  // rice_parameter to be at least 10 so that the decoder attempts to read more
  // bits for the remainder 'r' than the 7 remaining bits in the stream.
  int rice_parameter =
      std::max(v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter, 10);
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  WriteBit(/*bit=*/false, stream, bit_index);
  WriteBit(/*bit=*/true, stream, bit_index);
  VerifyDecode<T>(/*first_value=*/ConvertTo<T>(100), rice_parameter,
                  /*num_entries=*/1, stream,
                  /*expected_result=*/V5DecodeResult::kRanOutOfBits);
}

TYPED_TEST(V5RiceDecoderTypedTest, QZero) {
  using T = TypeParam;
  int rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter;
  T first_value = ConvertTo<T>(100);
  // Make the delta be shifted all the way over but not quite to the q zone, so
  // q remains 0.
  T delta = ConvertTo<T>(1) << (rice_parameter - 1);
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, rice_parameter, stream, bit_index);
  T expected_second_value = first_value + delta;
  VerifyDecode<T>(first_value, rice_parameter, /*num_entries=*/1, stream,
                  /*expected_result=*/V5DecodeResult::kSuccess,
                  /*expected_values=*/
                  {first_value, expected_second_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, QOne) {
  using T = TypeParam;
  int rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter;
  T first_value = ConvertTo<T>(100);
  // Make the delta be shifted all the way over to the q zone, and add 5 for the
  // r portion.
  T delta = (ConvertTo<T>(1) << rice_parameter) + ConvertTo<T>(5);
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, rice_parameter, stream, bit_index);
  T expected_second_value = first_value + delta;
  VerifyDecode<T>(first_value, rice_parameter, /*num_entries=*/1, stream,
                  /*expected_result=*/V5DecodeResult::kSuccess,
                  /*expected_values=*/
                  {first_value, expected_second_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, QMaxValid) {
  using T = TypeParam;
  int rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMaxRiceParameter;
  uint32_t max_valid_q =
      (1u << (v5_rice_utils::V5TypeTraits<T>::kBitWidth - rice_parameter)) - 1;
  T first_value = ConvertTo<T>(0);
  T delta = ConvertTo<T>(max_valid_q) << rice_parameter;
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, rice_parameter, stream, bit_index);
  T expected_second_value = first_value + delta;
  VerifyDecode<T>(first_value, rice_parameter, /*num_entries=*/1, stream,
                  /*expected_result=*/V5DecodeResult::kSuccess,
                  /*expected_values=*/
                  {first_value, expected_second_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, QTooHigh) {
  using T = TypeParam;
  int rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMaxRiceParameter;
  uint32_t max_valid_q =
      (1u << (v5_rice_utils::V5TypeTraits<T>::kBitWidth - rice_parameter)) - 1;
  uint32_t invalid_q = max_valid_q + 1;

  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  // unary q portion has one too many
  for (uint32_t i = 0; i < invalid_q; ++i) {
    WriteBit(/*bit=*/true, stream, bit_index);
  }
  WriteBit(/*bit=*/false, stream, bit_index);
  // r portion
  for (int i = 0; i < rice_parameter; ++i) {
    WriteBit(/*bit=*/false, stream, bit_index);
  }
  VerifyDecode<T>(/*first_value=*/ConvertTo<T>(0), rice_parameter,
                  /*num_entries=*/1, stream,
                  /*expected_result=*/V5DecodeResult::kQuotientTooLarge);
}

TYPED_TEST(V5RiceDecoderTypedTest, TooManyOnesForValidQuotient) {
  using T = TypeParam;
  VerifyDecode<T>(
      /*first_value=*/ConvertTo<T>(0),
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMaxRiceParameter,
      /*num_entries=*/1, /*stream=*/{0xFF},
      /*expected_result=*/V5DecodeResult::kQuotientTooLarge);
}

TYPED_TEST(V5RiceDecoderTypedTest, LastValueExactlyMax) {
  using T = TypeParam;
  T max_T = GetMaxVal<T>();
  T delta = ConvertTo<T>(10);
  T first_value = SubtractDelta(max_T, delta);
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
                  stream, bit_index);
  VerifyDecode<T>(
      first_value,
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/1, stream,
      /*expected_result=*/V5DecodeResult::kSuccess,
      /*expected_values=*/{first_value, max_T});
}

TYPED_TEST(V5RiceDecoderTypedTest, LastValueOverflowsByOne) {
  using T = TypeParam;
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(ConvertTo<T>(1),
                  v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter, stream,
                  bit_index);
  VerifyDecode<T>(
      /*first_value=*/GetMaxVal<T>(),
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter,
      /*num_entries=*/1, stream,
      /*expected_result=*/V5DecodeResult::kPrefixAccumulationOverflow);
}

TYPED_TEST(V5RiceDecoderTypedTest, DeltaMaxValid) {
  using T = TypeParam;
  T first_value = ConvertTo<T>(0);
  T delta = GetMaxVal<T>();
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(delta, v5_rice_utils::V5TypeTraits<T>::kMaxRiceParameter,
                  stream, bit_index);
  VerifyDecode<T>(
      first_value,
      /*rice_parameter=*/v5_rice_utils::V5TypeTraits<T>::kMaxRiceParameter,
      /*num_entries=*/1, stream,
      /*expected_result=*/V5DecodeResult::kSuccess,
      /*expected_values=*/{first_value, delta});
}

TYPED_TEST(V5RiceDecoderTypedTest, NumEntriesZeroWithData) {
  using T = TypeParam;
  int rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter;
  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  EncodeRiceDelta(ConvertTo<T>(50), rice_parameter, stream, bit_index);
  T first_value = ConvertTo<T>(100);
  // Follow num_entries, not the encoded data.
  VerifyDecode<T>(first_value, rice_parameter, /*num_entries=*/0, stream,
                  /*expected_result=*/V5DecodeResult::kSuccess,
                  /*expected_values=*/{first_value});
}

TYPED_TEST(V5RiceDecoderTypedTest, BasicMultipleEntries) {
  using T = TypeParam;
  T first_value = ConvertTo<T>(12345);
  int rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter + 5;

  // delta1: tests q=0 boundary with maximum possible remainder for q=0.
  T delta1 = (ConvertTo<T>(1) << (rice_parameter - 1));
  // delta2: tests q=1 with a small remainder.
  T delta2 = (ConvertTo<T>(1) << rice_parameter) + ConvertTo<T>(5);
  // delta3: tests q=2 (quotient > 1) with a remainder.
  T delta3 = (ConvertTo<T>(2) << rice_parameter) + ConvertTo<T>(10);
  std::vector<T> deltas = {delta1, delta2, delta3};

  std::vector<uint8_t> stream;
  size_t bit_index = 0;
  for (T delta : deltas) {
    EncodeRiceDelta(delta, rice_parameter, stream, bit_index);
  }

  std::vector<T> expected_ints = {
      first_value, first_value + deltas[0], first_value + deltas[0] + deltas[1],
      first_value + deltas[0] + deltas[1] + deltas[2]};
  VerifyDecode(first_value, rice_parameter, deltas.size(), stream,
               /*expected_result=*/V5DecodeResult::kSuccess,
               /*expected_values=*/expected_ints);
}

TYPED_TEST(V5RiceDecoderTypedTest, DecodePrefixesFailure) {
  // Most tests in this file check Decode directly for errors. This test
  // confirms that errors are threaded through to DecodePrefixes too.
  using T = TypeParam;
  T first_value = ConvertTo<T>(100);
  int min_rice_parameter = v5_rice_utils::V5TypeTraits<T>::kMinRiceParameter;

  VerifyDecodePrefixes<T>(first_value, min_rice_parameter, /*num_entries=*/1,
                          /*stream=*/{},
                          /*expected_result=*/V5DecodeResult::kRanOutOfBits);
}

template <typename T>
class V5RiceInputValidatorTypedTest : public ::testing::Test {};

TYPED_TEST_SUITE(V5RiceInputValidatorTypedTest, V5RiceTypes);

TYPED_TEST(V5RiceInputValidatorTypedTest, Validate) {
  using T = TypeParam;
  using Traits = v5_rice_utils::V5TypeTraits<T>;

  EXPECT_EQ(
      V5InputValidationResult::kSuccess,
      V5RiceInputValidator::Validate<T>(
          /*rice_parameter=*/Traits::kMinRiceParameter, /*num_entries=*/2));
  EXPECT_EQ(
      V5InputValidationResult::kSuccess,
      V5RiceInputValidator::Validate<T>(
          /*rice_parameter=*/Traits::kMaxRiceParameter, /*num_entries=*/2));
  EXPECT_EQ(
      V5InputValidationResult::kNegativeNumEntries,
      V5RiceInputValidator::Validate<T>(
          /*rice_parameter=*/Traits::kMinRiceParameter, /*num_entries=*/-1));
  EXPECT_EQ(
      V5InputValidationResult::kRiceParameterTooSmall,
      V5RiceInputValidator::Validate<T>(
          /*rice_parameter=*/Traits::kMinRiceParameter - 1, /*num_entries=*/2));
  EXPECT_EQ(
      V5InputValidationResult::kRiceParameterTooLarge,
      V5RiceInputValidator::Validate<T>(
          /*rice_parameter=*/Traits::kMaxRiceParameter + 1, /*num_entries=*/2));
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            V5RiceInputValidator::Validate<T>(
                /*rice_parameter=*/0, /*num_entries=*/0));
  EXPECT_EQ(V5InputValidationResult::kSuccess,
            V5RiceInputValidator::Validate<T>(
                /*rice_parameter=*/-1, /*num_entries=*/0));
}

}  // namespace safe_browsing
