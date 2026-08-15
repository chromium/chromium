// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/base32/base32.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <ostream>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/base32/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace base32 {
namespace {

// The test is parameterized on whether to use the Rust implementation of the
// component. See crbug.com/536936880.
class Base32Test : public ::testing::TestWithParam<bool> {
 public:
  Base32Test() {
    features_.InitWithFeatureState(features::kComponentsBase32InRust,
                                   GetParam());
  }

  void SetUp() override { ASSERT_TRUE(base::FeatureList::GetInstance()); }

 protected:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(,
                         Base32Test,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Rust" : "Cpp";
                         });

TEST_P(Base32Test, EncodesRfcTestVectorsCorrectlyWithoutPadding) {
  // Tests from http://tools.ietf.org/html/rfc4648#section-10.
  static constexpr uint8_t test_data[] = "foobar";
  constexpr base::span test_subspan(test_data);

  constexpr auto expected = std::to_array<const char*>(
      {"", "MY", "MZXQ", "MZXW6", "MZXW6YQ", "MZXW6YTB", "MZXW6YTBOI"});

  // Run the tests, with one more letter in the input every pass.
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    auto encoded_output =
        Base32Encode(test_subspan.first(i), Base32EncodePolicy::OMIT_PADDING);
    EXPECT_EQ(expected[i], encoded_output);
    auto decoded_output = Base32Decode(encoded_output);
    EXPECT_TRUE(std::ranges::equal(test_subspan.first(i), decoded_output));
  }
}

TEST_P(Base32Test, EncodesRfcTestVectorsCorrectlyWithPadding) {
  // Tests from http://tools.ietf.org/html/rfc4648#section-10.
  static constexpr uint8_t test_data[] = "foobar";
  constexpr base::span test_subspan(test_data);

  constexpr auto expected = std::to_array<const char*>(
      {"", "MY======", "MZXQ====", "MZXW6===", "MZXW6YQ=", "MZXW6YTB",
       "MZXW6YTBOI======"});

  // Run the tests, with one more letter in the input every pass.
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    std::string encoded_output = Base32Encode(test_subspan.first(i));
    EXPECT_EQ(expected[i], encoded_output);
    std::vector<uint8_t> decoded_output = Base32Decode(encoded_output);
    EXPECT_TRUE(std::ranges::equal(test_subspan.first(i), decoded_output));
  }
}

TEST_P(Base32Test, EncodesSha256HashCorrectly) {
  // Useful to test with longer input than the RFC test vectors, and encoding
  // SHA-256 hashes is one of the use cases for this component.
  static constexpr uint8_t hash[] =
      "\x1f\x25\xe1\xca\xba\x4f\xf9\xb8\x27\x24\x83\x0f\xca\x60\xe4\xc2\xbe\xa8"
      "\xc3\xa9\x44\x1c\x27\xb0\xb4\x3e\x6a\x96\x94\xc7\xb8\x04";
  constexpr auto test_span = base::span(hash).first(32u);
  std::string encoded_output =
      Base32Encode(test_span, Base32EncodePolicy::OMIT_PADDING);
  EXPECT_EQ("D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA",
            encoded_output);
  std::vector<uint8_t> decoded_output = Base32Decode(encoded_output);
  EXPECT_TRUE(std::ranges::equal(test_span, decoded_output));
}

void EncodeParity(const std::string_view input) {
  const base::span<const uint8_t> input_span = base::as_byte_span(input);

  std::string cpp_encode_result_include_padding = internal::Base32EncodeCpp(
      input_span, Base32EncodePolicy::INCLUDE_PADDING);
  std::string cpp_encode_result_omit_padding =
      internal::Base32EncodeCpp(input_span, Base32EncodePolicy::OMIT_PADDING);

  std::string rs_encode_result_include_padding = internal::Base32EncodeRust(
      input_span, Base32EncodePolicy::INCLUDE_PADDING);
  std::string rs_encode_result_omit_padding =
      internal::Base32EncodeRust(input_span, Base32EncodePolicy::OMIT_PADDING);

  EXPECT_EQ(cpp_encode_result_include_padding, rs_encode_result_include_padding)
      << "Base32Encode(_, INCLUDE_PADDING) mismatch for input: "
      << base::HexEncode(input)
      << "\nCPP: " << cpp_encode_result_include_padding
      << "\nRust: " << rs_encode_result_include_padding;
  EXPECT_EQ(cpp_encode_result_omit_padding, rs_encode_result_omit_padding)
      << "Base32Encode(_, OMIT_PADDING) mismatch for input: "
      << base::HexEncode(input) << "\nCPP: " << cpp_encode_result_omit_padding
      << "\nRust: " << rs_encode_result_omit_padding;
}

void DecodeParity(const std::string_view input) {
  std::vector<uint8_t> cpp_decode_result = internal::Base32DecodeCpp(input);
  std::vector<uint8_t> rs_decode_result = internal::Base32DecodeRust(input);

  EXPECT_EQ(cpp_decode_result, rs_decode_result)
      << "Base32Decode(_) mismatch for input: " << input
      << "\nCPP: " << base::HexEncode(cpp_decode_result)
      << "\nRust: " << base::HexEncode(rs_decode_result);
}

FUZZ_TEST(Base32ParityTest, EncodeParity)
    .WithDomains(fuzztest::Arbitrary<std::string_view>());

FUZZ_TEST(Base32ParityTest, DecodeParity)
    .WithDomains(fuzztest::Arbitrary<std::string_view>());

}  // namespace
}  // namespace base32
