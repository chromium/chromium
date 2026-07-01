// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_rice.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using v5_rice_utils::SerializeToBigEndianBytes;
using v5_rice_utils::TryAdd;
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

// Helper to cast or truncate a value to uint32_t.
template <typename T>
uint32_t ToUint32(const T& val) {
  return static_cast<uint32_t>(val);
}

// Returns the maximum representable value for type T.
template <typename T>
T GetMaxVal() {
  return std::numeric_limits<T>::max();
}

// Subtraction (only used for tests).
template <typename T>
T SubtractDelta(T val, T delta) {
  return val - delta;
}

// Conversion helpers.
template <typename T>
T ConvertTo(uint64_t v) {
  return static_cast<T>(v);
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

TEST(V5RiceTryAddTest, TryAdd) {
  // uint32_t
  uint32_t result_u32;
  uint32_t a_u32 = 10;
  uint32_t b_u32 = 20;
  EXPECT_TRUE(TryAdd(a_u32, b_u32, &result_u32));
  EXPECT_EQ(result_u32, 30u);
  EXPECT_FALSE(TryAdd(0xFFFFFFFFu, 1u, &result_u32));
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

TEST(V5RiceSerializeTest, SerializeToBigEndianBytes) {
  // Test converting decoded values to big-endian raw bytes string.
  // For uint32_t:
  std::vector<uint32_t> vec32 = {0x12345678, 0x9ABCDEF0};
  std::string res32 = SerializeToBigEndianBytes(vec32);
  std::string expected32 =
      std::string("\x12\x34\x56\x78\x9A\xBC\xDE\xF0", /*n=*/8);
  EXPECT_EQ(res32, expected32);
}

// =============================================================================
// V5RiceDecoderTest
// =============================================================================

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
}

// =============================================================================
// V5RiceDecoderTypedTest (Typed Tests)
// =============================================================================

template <typename T>
class V5RiceDecoderTypedTest : public ::testing::Test {};

using V5RiceTypes = ::testing::Types<uint32_t>;
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

}  // namespace safe_browsing
