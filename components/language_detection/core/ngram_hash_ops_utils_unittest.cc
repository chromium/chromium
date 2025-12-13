// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/ngram_hash_ops_utils.h"

#include <string>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

using ::testing::Values;

std::string ReconstructStringFromTokens(TokenizedOutput output) {
  std::string reconstructed_str;
  for (size_t i = 0; i < output.tokens.size(); i++) {
    reconstructed_str.append(
        UNSAFE_TODO(output.str.c_str() + output.tokens[i].first),
        UNSAFE_TODO(output.str.c_str() + output.tokens[i].first +
                    output.tokens[i].second));
  }
  return reconstructed_str;
}

struct TokenizeTestParams {
  std::string input_str;
  size_t max_tokens;
  bool exclude_nonalphaspace_tokens;
  bool lower_case_input;
  std::string expected_output_str;
};

class TokenizeParameterizedTest
    : public ::testing::Test,
      public testing::WithParamInterface<TokenizeTestParams> {};

TEST_P(TokenizeParameterizedTest, Tokenize) {
  // Checks that the Tokenize method returns the expected value.
  const TokenizeTestParams params = TokenizeParameterizedTest::GetParam();
  const TokenizedOutput output = Tokenize(
      /*input_str=*/params.input_str,
      /*max_tokens=*/params.max_tokens,
      /*exclude_nonalphaspace_tokens=*/params.exclude_nonalphaspace_tokens,
      /*lower_case_input=*/params.lower_case_input);

  // The output string should have the necessary prefixes, and the "!" token
  // should have been replaced with a " ".
  EXPECT_EQ(output.str, params.expected_output_str);
  EXPECT_EQ(ReconstructStringFromTokens(output), params.expected_output_str);
}

INSTANTIATE_TEST_SUITE_P(
    TokenizeParameterizedTests,
    TokenizeParameterizedTest,
    Values(
        // Test including non-alphanumeric characters.
        TokenizeTestParams({/*input_str=*/"hi!", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/false,
                            /*lower_case_input=*/false,
                            /*expected_output_str=*/"^hi!$"}),
        // Test not including non-alphanumeric characters.
        TokenizeTestParams({/*input_str=*/"hi!", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/true,
                            /*lower_case_input=*/false,
                            /*expected_output_str=*/"^hi $"}),
        // Test with a maximum of 3 tokens.
        TokenizeTestParams({/*input_str=*/"hi!", /*max_tokens=*/3,
                            /*exclude_alphanonspace=*/true,
                            /*lower_case_input=*/false,
                            /*expected_output_str=*/"^h$"}),
        // Test with non-latin characters.
        TokenizeTestParams({/*input_str=*/"ありがと", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/true,
                            /*lower_case_input=*/false,
                            /*expected_output_str=*/"^ありがと$"}),
        // Test with incomplete unicode character.
        TokenizeTestParams({/*input_str=*/"a\x80", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/true,
                            /*lower_case_input=*/false,
                            /*expected_output_str=*/"^a$"}),
        // Test lower case with latin characters.
        TokenizeTestParams({/*input_str=*/"HI!", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/false,
                            /*lower_case_input=*/true,
                            /*expected_output_str=*/"^hi!$"}),
        // Test lower case with non-latin characters.
        TokenizeTestParams({/*input_str=*/"ありがと", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/false,
                            /*lower_case_input=*/true,
                            /*expected_output_str=*/"^ありがと$"}),
        // Test lower case with characters that have less bytes in lower case
        // than upper case.
        TokenizeTestParams({/*input_str=*/"ẞẞẞẞẞẞ", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/false,
                            /*lower_case_input=*/true,
                            /*expected_output_str=*/"^ßßßßßß$"})));

TEST(TokenizeTest, EmptyInputTest) {
  const TokenizedOutput output = Tokenize({}, 10, true, false);

  // Tokenize should early return and contain only the prefix and suffix tokens.
  EXPECT_EQ(output.str, "^$");
}

TEST(LowercaseUnicodeTest, TestLowercaseUnicode) {
  // Check that the method is a no-op when the string is lowercase.
  EXPECT_EQ(LowercaseUnicodeStr("hello"), "hello");

  // Check that the method has uppercase characters.
  EXPECT_EQ(LowercaseUnicodeStr("hElLo"), "hello");

  // Check that the method works with non-latin scripts.
  // Cyrillic has the concept of cases, so it should change the input.
  EXPECT_EQ(LowercaseUnicodeStr("БЙп"), "бйп");

  // Check that the method works with non-latin scripts.
  // Japanese doesn't have the concept of cases, so it should not change.
  EXPECT_EQ(LowercaseUnicodeStr("ありがと"), "ありがと");
}

}  // namespace language_detection
