// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "components/autofill/core/browser/randomized_encoder.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/common/signatures.h"
#include "components/unified_consent/pref_names.h"
#include "net/base/hex_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

constexpr size_t kBitsPerByte = 8;
constexpr size_t kEncodedChunkLengthInBytes = 64;
constexpr size_t kEncodedChunkLengthInBits =
    kEncodedChunkLengthInBytes * kBitsPerByte;

// Get the |i|-th bit of |s| where |i| counts up from the 0-bit of the first
// character in |s|. It is expected that the caller guarantees that |i| is a
// valid bit-offset into |s|
bool GetBit(std::string_view s, size_t i) {
  DCHECK_LT(i / kBitsPerByte, s.length());
  return static_cast<bool>((s[i / kBitsPerByte]) & (1 << (i % kBitsPerByte)));
}

// This is a reference encoder implementation. This implementation performs the
// all bits encoding one full byte at a time and then packs the selected bits
// into a final output buffer.
std::string ReferenceEncodeImpl(std::string_view coins,
                                std::string_view noise,
                                std::string_view value,
                                size_t bit_offset,
                                size_t bit_stride) {
  // Encode all of the bits.
  std::string all_bits(noise);
  size_t value_length = std::min(value.length(), noise.length());
  for (size_t i = 0; i < value_length; ++i) {
    all_bits[i] = (value[i] & coins[i]) | (all_bits[i] & ~coins[i]);
  }

  // Select the only the ones matching bit_offset and bit_stride.
  std::string output(all_bits.length() / bit_stride, 0);
  size_t src_offset = bit_offset;
  size_t dst_offset = 0;
  while (src_offset < all_bits.length() * kBitsPerByte) {
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
  using RandomizedEncoder::GetChunkCount;
  using RandomizedEncoder::GetCoins;
  using RandomizedEncoder::GetNoise;
  using RandomizedEncoder::RandomizedEncoder;
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

// As described in randomized_encoder.cc
// TODO(crbug.com/40570965): resolve circular dependency and remove
// hardcoded constant
TEST(RandomizedEncoderTest, CorrectUrlConsentFlag) {
  EXPECT_STREQ(
      TestRandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled,
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

TEST_P(RandomizedEncoderTest, Encode) {
  const autofill::FormSignature form_signature(0x1234567812345678);
  const autofill::FieldSignature field_signature(0xCAFEBABE);
  const std::string data_type = TestRandomizedEncoder::FORM_CSS_CLASS;
  const EncodeParams& params = GetParam();
  const std::string value("This is some text for testing purposes.");

  TestRandomizedEncoder encoder("this is the seed", params.encoding_type, true);

  size_t chunk_count = encoder.GetChunkCount(value, data_type);
  size_t padded_input_length = chunk_count * kEncodedChunkLengthInBytes;
  EXPECT_LE(value.length(), padded_input_length);

  // Encode the output string.
  std::string actual_result =
      encoder.Encode(form_signature, field_signature, data_type, value);

  // Capture the coin and noise bits used for the form, field and metadata type.
  std::string coins = encoder.GetCoins(form_signature, field_signature,
                                       data_type, padded_input_length);
  std::string noise = encoder.GetNoise(form_signature, field_signature,
                                       data_type, padded_input_length);

  // Use the reference encoder implementation to get the expected output.
  std::string expected_result = ReferenceEncodeImpl(
      coins, noise, value, params.bit_offset, params.bit_stride);

  // The results should be the same.
  EXPECT_EQ(padded_input_length / params.bit_stride, actual_result.length());
  EXPECT_EQ(expected_result, actual_result);
}

TEST_P(RandomizedEncoderTest, EncodeLarge) {
  const std::string data_types[] = {TestRandomizedEncoder::FORM_NAME,
                                    TestRandomizedEncoder::FORM_URL};
  for (std::string data_type : data_types) {
    const autofill::FormSignature form_signature(0x8765432187654321);
    const autofill::FieldSignature field_signature(0xDEADBEEF);
    const EncodeParams& params = GetParam();
    const std::string value(
        "This is some text for testing purposes. It exceeds the maximum "
        "encoding "
        "size. This serves to validate that truncation is performed. Lots and "
        "lots of text. Yay!");

    TestRandomizedEncoder encoder("this is the seed", params.encoding_type,
                                  true);

    size_t chunk_count = encoder.GetChunkCount(value, data_type);
    size_t padded_input_length = chunk_count * kEncodedChunkLengthInBytes;

    EXPECT_EQ(data_type == TestRandomizedEncoder::FORM_URL, chunk_count > 1);

    // Encode the output string.
    std::string actual_result =
        encoder.Encode(form_signature, field_signature, data_type, value);

    if (data_type == TestRandomizedEncoder::FORM_URL) {
      EXPECT_LE(value.length(), actual_result.length() * params.bit_stride);
    } else {
      EXPECT_GT(value.length(), actual_result.length() * params.bit_stride);
    }

    // Capture the coin and noise bits used for the form, field and metadata
    // type.
    std::string coins = encoder.GetCoins(form_signature, field_signature,
                                         data_type, padded_input_length);
    std::string noise = encoder.GetNoise(form_signature, field_signature,
                                         data_type, padded_input_length);

    // Use the reference encoder implementation to get the expected output.
    std::string expected_result = ReferenceEncodeImpl(
        coins, noise, value, params.bit_offset, params.bit_stride);

    // The results should be the same.
    EXPECT_EQ(padded_input_length / params.bit_stride, actual_result.length());
    EXPECT_EQ(expected_result, actual_result);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         RandomizedEncoderTest,
                         ::testing::ValuesIn(kEncodeParams));

namespace {

// Data structure used to drive the decoding test cases.
struct DecodeParams {
  // The number of samples for this test.
  size_t num_votes;

  // The lower and upper bound of the confidence interval given |num_votes|
  // samples. If fewer than (lower_bound * num_votes) samples are one then the
  // bit is a zero. If more than (upper_bound * num_votes) samples are one then
  // the bit is a one.
  double lower_bound;
  double upper_bound;
};

// The fundamental idea of this algorithm is to count the number of reported 1s
// and 0s from a crowdsourced set of votes and look for a threshold that gives
// us confidence that we have seen enough 1s or 0s to assume that the actual
// value is a 1 or 0.
//
// Due to the symmetriy of the problem, we can conveniently choose the following
// null-hypothesis:
//
// N0: The true value of a bit is random on each pageload.
//
// In this case, we expect a binomial distribution B(n, 0.5), for which we can
// find a confidence interval that a sample of size n ends up in this interval.
// If a sample is outside of this interval, we can assume with the given
// confidence that we can reject the null-hypothesis and assume that the actual
// value is 0 or 1.
//
// For the confidence interval, we use the Wilson Score interval.
// https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval#Wilson_score_interval
//
// The confidence intervals are calculate as:
//
//   import scipy.stats as st
//   import math
//
//   def ConfidenceInterval(ph, n, confidence):
//     """
//     Args:
//       confidence: e.g. 0.95 for a 95% confidence
//       ph = p hat (see Wilson Score inverval link above)
//       n = number of samples
//     """
//     alpha = 1 - confidence
//     # for the 95% confidence interval this gives 1.96.
//     z = st.norm.ppf(1 - alpha/2)
//
//     base = (ph + z*z/(2*n)) / (1 + z*z/n)
//     offset = z / (1 + z*z/n) * math.sqrt(ph*(1-ph)/n + z*z/(4*n*n))
//
//     return [base - offset, base + offset]
//
// Note: The string being encoded/decoded for each "sample" consists of a common
// prefix followed by the decimal value of the sample number (i.e., "foo 25").
// Avoid test cases that skew the distribution of the appended noise to far
// from random (for example, using 0-199, more than 55% of the appended noise
// starts with the ASCII digit '1').
const DecodeParams kDecodeParams[] = {
    // 99.0 % confidence interval
    {150, 0.39709349666174232, 0.60290650333825768},
    {256, 0.42052859913480434, 0.57947140086519566},

    // 99.5% confidence interval
    {150, 0.38829956746162475, 0.61170043253837525},
    {256, 0.41359977651950797, 0.58640022348049203},
};

using RandomizedDecoderTest = ::testing::TestWithParam<DecodeParams>;

// Generate a hex string representing the 128 bit value where each byte has
// value |i|. This lets us spread the seeds across the 128 bit space.
std::string Make128BitSeed(size_t i) {
  EXPECT_LT(i, 256u);
  return net::HexDump(
      std::string(128 / kBitsPerByte, static_cast<char>(i & 0xFF)));
}

}  // namespace

TEST(RandomizedEncoderTest, GetChunkCount) {
  TestRandomizedEncoder encoder(
      "secret", autofill::AutofillRandomizedValue_EncodingType_ALL_BITS, true);

  std::string_view url_type = TestRandomizedEncoder::FORM_URL;
  EXPECT_EQ(encoder.GetChunkCount("", url_type), 0);
  EXPECT_EQ(encoder.GetChunkCount("1", url_type), 1);
  EXPECT_EQ(encoder.GetChunkCount(std::string(33, '-'), url_type), 1);
  EXPECT_EQ(encoder.GetChunkCount(std::string(64, '-'), url_type), 1);
  EXPECT_EQ(encoder.GetChunkCount(std::string(65, '-'), url_type), 2);
  EXPECT_EQ(encoder.GetChunkCount(std::string(512, '-'), url_type), 8);
  EXPECT_EQ(encoder.GetChunkCount(std::string(513, '-'), url_type), 8);
  EXPECT_EQ(encoder.GetChunkCount(std::string(1000, '-'), url_type), 8);

  std::string_view name_type = TestRandomizedEncoder::FORM_NAME;
  EXPECT_EQ(encoder.GetChunkCount("", name_type), 1);
  EXPECT_EQ(encoder.GetChunkCount("1", name_type), 1);
  EXPECT_EQ(encoder.GetChunkCount(std::string(33, '-'), name_type), 1);
  EXPECT_EQ(encoder.GetChunkCount(std::string(70, '-'), name_type), 1);
  EXPECT_EQ(encoder.GetChunkCount(std::string(1000, '-'), name_type), 1);
}

TEST_P(RandomizedDecoderTest, Decode) {
  static const std::string_view prefixes[] = {
      "This is the common prefix to encode and recover",

      "This is the longer common prefix to encode and recover to test input "
      "|data_type==FORM_URL| values can be up to 8 * 64 bytes.",
  };
  for (std::string_view common_prefix : prefixes) {
    static const autofill::FormSignature form_signature(0x8765432187654321);
    static const autofill::FieldSignature field_signature(0xDEADBEEF);
    static const std::string data_type = TestRandomizedEncoder::FORM_URL;

    const size_t num_votes = GetParam().num_votes;
    const double lower_bound = GetParam().lower_bound;
    const double upper_bound = GetParam().upper_bound;

    size_t chunk_count =
        TestRandomizedEncoder(
            "secret", autofill::AutofillRandomizedValue_EncodingType_ALL_BITS,
            true)
            .GetChunkCount(base::StringPrintf("%.*s%zu",
                                              base::saturated_cast<int>(
                                                  common_prefix.length()),
                                              common_prefix.data(), num_votes),
                           data_type);
    SCOPED_TRACE(testing::Message() << "chunk_count=" << chunk_count);

    // This vector represents the aggregate counts of the number of times a
    // separate encoding of out sample string had a given bit encoded as a one.
    std::vector<size_t> num_times_bit_is_1(
        /*count=*/kEncodedChunkLengthInBits * chunk_count,
        /*value=*/0);

    // Perform |num_votes| independent encoding operations, with seeds
    // (somewhat) evenly spread out across a 128-bit space.
    for (size_t i = 0; i < num_votes; ++i) {
      // Create a new encoder with a different secret each time.
      TestRandomizedEncoder encoder(
          Make128BitSeed(i),
          autofill::AutofillRandomizedValue_EncodingType_ALL_BITS, true);

      // Encode the common prefix plus some non-constant data.
      std::string encoded = encoder.Encode(
          form_signature, field_signature, data_type,
          base::StringPrintf("%.*s%zu",
                             base::saturated_cast<int>(common_prefix.length()),
                             common_prefix.data(), i));

      // Update |num_times_bit_is_1| for each bit in the encoded string.
      for (size_t b = 0; b < kEncodedChunkLengthInBits * chunk_count; ++b) {
        num_times_bit_is_1[b] += GetBit(encoded, b);
      }
    }

    // Use |num_times_bit_is_1| to reconstruct the encoded string, bit by bit,
    // as well as a record of whether or not each bit in the reconstruction
    // buffer was validated with sufficient confidence.
    std::string output(kEncodedChunkLengthInBytes * chunk_count,
                       static_cast<char>(0));
    std::vector<bool> bit_is_valid(kEncodedChunkLengthInBits * chunk_count);
    const double threshold_for_zero = lower_bound * num_votes;
    const double threshold_for_one = upper_bound * num_votes;
    for (size_t b = 0; b < kEncodedChunkLengthInBits * chunk_count; ++b) {
      if (num_times_bit_is_1[b] < threshold_for_zero) {
        // bit it already zero, just mark it as valid
        bit_is_valid[b] = true;
      } else if (num_times_bit_is_1[b] > threshold_for_one) {
        output[b / kBitsPerByte] |= (1 << (b % kBitsPerByte));
        bit_is_valid[b] = true;
      }
    }

    // Validation: All bits overlapping the constant prefix should be valid.
    for (size_t b = 0; b < common_prefix.length() * kBitsPerByte; ++b) {
      EXPECT_TRUE(bit_is_valid[b]) << "True bit found to be noise at " << b;
    }

    // Validation: All of the recovered prefix bits should match the prefix.
    for (size_t i = 0; i < common_prefix.length(); ++i) {
      EXPECT_EQ(common_prefix[i], output[i])
          << "Incorrect char at offset " << i;
    }

    // Validation: Most noise bits should be invalid, but we may get some false
    // positives. Instead, we expect that no noise byte will have all of its
    // bits turn up as valid.
    for (size_t i = common_prefix.length();
         i < kEncodedChunkLengthInBytes * chunk_count; ++i) {
      size_t num_valid_bits = 0;
      for (size_t b = 0; b < kBitsPerByte; ++b) {
        num_valid_bits += bit_is_valid[i * kBitsPerByte + b];
      }
      EXPECT_LT(num_valid_bits, kBitsPerByte)
          << "Noise byte at offset " << i << " decoded as " << output[i];
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         RandomizedDecoderTest,
                         ::testing::ValuesIn(kDecodeParams));

}  // namespace
}  // namespace autofill
