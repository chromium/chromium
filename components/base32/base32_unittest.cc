// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/base32/base32.h"

#include <stdint.h>

#include <array>
#include <string>

#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base32 {
namespace {

TEST(Base32Test, EncodesRfcTestVectorsCorrectlyWithoutPadding) {
  // Tests from http://tools.ietf.org/html/rfc4648#section-10.
  static constexpr uint8_t test_data[] = "foobar";
  constexpr base::span test_subspan(test_data);

  constexpr auto expected = std::to_array<const char*>(
      {"", "MY", "MZXQ", "MZXW6", "MZXW6YQ", "MZXW6YTB", "MZXW6YTBOI"});

  // Run the tests, with one more letter in the input every pass.
  for (size_t i = 0; i < expected.size(); ++i) {
    auto encoded_output =
        Base32Encode(test_subspan.first(i), Base32EncodePolicy::OMIT_PADDING);
    EXPECT_EQ(expected[i], encoded_output);
    auto decoded_output = Base32Decode(encoded_output);
    EXPECT_TRUE(base::ranges::equal(test_subspan.first(i), decoded_output));
  }
}

TEST(Base32Test, EncodesRfcTestVectorsCorrectlyWithPadding) {
  // Tests from http://tools.ietf.org/html/rfc4648#section-10.
  static constexpr uint8_t test_data[] = "foobar";
  constexpr base::span test_subspan(test_data);

  constexpr auto expected = std::to_array<const char*>(
      {"", "MY======", "MZXQ====", "MZXW6===", "MZXW6YQ=", "MZXW6YTB",
       "MZXW6YTBOI======"});

  // Run the tests, with one more letter in the input every pass.
  for (size_t i = 0; i < expected.size(); ++i) {
    std::string encoded_output = Base32Encode(test_subspan.first(i));
    EXPECT_EQ(expected[i], encoded_output);
    std::vector<uint8_t> decoded_output = Base32Decode(encoded_output);
    EXPECT_TRUE(base::ranges::equal(test_subspan.first(i), decoded_output));
  }
}

TEST(Base32Test, EncodesSha256HashCorrectly) {
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
  EXPECT_TRUE(base::ranges::equal(test_span, decoded_output));
}

}  // namespace
}  // namespace base32
