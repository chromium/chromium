// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/randomized_encoder.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr size_t kBitsPerByte = 8;
constexpr size_t kMaxLengthInBytes = 64;
constexpr size_t kMaxLengthInBits = kMaxLengthInBytes * kBitsPerByte;

// Get the |i|-th bit of |s| where |i| counts up from the 0-bit of the first
// character in |s|. It is expected that the caller guarantees that |i| is a
// valid bit-offset into |s|
bool GetBit(base::StringPiece s, size_t i) {
  DCHECK_LT(i / kBitsPerByte, s.length());
  return static_cast<bool>((s[i / kBitsPerByte]) & (1 << (i % kBitsPerByte)));
}

// This is a reference encoder implementation. This implementation performs the
// all bits encoding one full byte at a time and then packs the selected bits
// into a final output buffer.
std::string ReferenceEncodeImpl(base::StringPiece coins,
                                base::StringPiece noise,
                                base::StringPiece value,
                                size_t bit_offset,
                                size_t bit_stride) {
  // Encode all of the bits.
  std::string all_bits = noise.as_string();
  size_t value_length = std::min(value.length(), kMaxLengthInBytes);
  for (size_t i = 0; i < value_length; ++i) {
    all_bits[i] = (value[i] & coins[i]) | (all_bits[i] & ~coins[i]);
  }

  // Select the only the ones matching bit_offset and bit_stride.
  std::string output(kMaxLengthInBytes / bit_stride, 0);
  size_t src_offset = bit_offset;
  size_t dst_offset = 0;
  while (src_offset < kMaxLengthInBits) {
    bool bit_value = GetBit(all_bits, src_offset);
    output[dst_offset / kBitsPerByte] |=
        (bit_value << (dst_offset % kBitsPerByte));
    src_offset += bit_stride;
    dst_offset += 1;
  }
  return output;
}

// A test version of the RandomizedEncoder class. Exposes "ForTest" methods.
class TestRandomizedEncoder : public autofill::RandomizedEncoder {
 public:
  using RandomizedEncoder::RandomizedEncoder;
  using RandomizedEncoder::GetCoins;
  using RandomizedEncoder::GetNoise;
};

// Data structure used to drive the encoding test cases.
struct EncodeParams {
  // The type of encoding to perform with the RandomizedEncoder.
  autofill::AutofillRandomizedValue_EncodingType encoding_type;

  // The bit offset to start from with the reference encoder.
  size_t bit_offset;

  // The bit stride to select the next bit to encode with the reference encoder.
  size_t bit_stride;
};

// A table to test cases, mapping encoding scheme to the reference encoder.
const EncodeParams kEncodeParams[] = {
    // One bit per byte. These all require 8 bytes to encode and have 8-bit
    // strides, starting from a different initial bit offset.
    {autofill::AutofillRandomizedValue_EncodingType_BIT_0, 0, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_1, 1, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_2, 2, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_3, 3, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_4, 4, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_5, 5, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_6, 6, 8},
    {autofill::AutofillRandomizedValue_EncodingType_BIT_7, 7, 8},

    // Four bits per byte. These require 32 bytes to encode and have 2-bit
    // strides/
    {autofill::AutofillRandomizedValue_EncodingType_EVEN_BITS, 0, 2},
    {autofill::AutofillRandomizedValue_EncodingType_ODD_BITS, 1, 2},

    // All bits per byte. This require 64 bytes to encode and has a 1-bit
    // stride.
    {autofill::AutofillRandomizedValue_EncodingType_ALL_BITS, 0u, 1},
};

using RandomizedEncoderTest = ::testing::TestWithParam<EncodeParams>;

}  // namespace

TEST_P(RandomizedEncoderTest, Encode) {
  const autofill::FormSignature form_signature = 0x1234567812345678;
  const autofill::FieldSignature field_signature = 0xCAFEBABE;
  const std::string data_type = "css_class";
  const EncodeParams& params = GetParam();
  const std::string value("This is some text for testing purposes.");

  EXPECT_LT(value.length(), kMaxLengthInBytes);

  TestRandomizedEncoder encoder("this is a secret", params.encoding_type);

  // Encode the output string.
  std::string actual_result =
      encoder.Encode(form_signature, field_signature, data_type, value);

  // Capture the coin and noise bits used for the form, field and metadata type.
  std::string coins =
      encoder.GetCoins(form_signature, field_signature, data_type);
  std::string noise =
      encoder.GetNoise(form_signature, field_signature, data_type);

  // Use the reference encoder implementation to get the expected output.
  std::string expected_result = ReferenceEncodeImpl(
      coins, noise, value, params.bit_offset, params.bit_stride);

  // The results should be the same.
  EXPECT_EQ(kMaxLengthInBytes / params.bit_stride, actual_result.length());
  EXPECT_EQ(expected_result, actual_result);
}

TEST_P(RandomizedEncoderTest, EncodeLarge) {
  const autofill::FormSignature form_signature = 0x8765432187654321;
  const autofill::FieldSignature field_signature = 0xDEADBEEF;
  const std::string data_type = "html_name";
  const EncodeParams& params = GetParam();
  const std::string value(
      "This is some text for testing purposes. It exceeds the maximum encoding "
      "size. This serves to validate that truncation is performed. Lots and "
      " or text. Yay!");
  EXPECT_GT(value.length(), kMaxLengthInBytes);

  TestRandomizedEncoder encoder("this is a secret", params.encoding_type);

  // Encode the output string.
  std::string actual_result =
      encoder.Encode(form_signature, field_signature, data_type, value);

  // Capture the coin and noise bits used for the form, field and metadata type.
  std::string coins =
      encoder.GetCoins(form_signature, field_signature, data_type);
  std::string noise =
      encoder.GetNoise(form_signature, field_signature, data_type);

  // Use the reference encoder implementation to get the expected output.
  std::string expected_result = ReferenceEncodeImpl(
      coins, noise, value, params.bit_offset, params.bit_stride);

  // The results should be the same.
  EXPECT_EQ(kMaxLengthInBytes / params.bit_stride, actual_result.length());
  EXPECT_EQ(expected_result, actual_result);
}

INSTANTIATE_TEST_CASE_P(All,
                        RandomizedEncoderTest,
                        ::testing::ValuesIn(kEncodeParams));
