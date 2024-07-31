// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/language_detection/core/ngram_hash_ops_utils.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

using ::testing::Values;

std::string ReconstructStringFromTokens(TokenizedOutput output) {
  std::string reconstructed_str;
  for (size_t i = 0; i < output.tokens.size(); i++) {
    reconstructed_str.append(
        output.str.c_str() + output.tokens[i].first,
        output.str.c_str() + output.tokens[i].first + output.tokens[i].second);
  }
  return reconstructed_str;
}

struct TokenizeTestParams {
  std::string input_str;
  size_t max_tokens;
  bool exclude_nonalphaspace_tokens;
  std::string expected_output_str;
};

class TokenizeParameterizedTest
    : public ::testing::Test,
      public testing::WithParamInterface<TokenizeTestParams> {};

TEST_P(TokenizeParameterizedTest, Tokenize) {
  // Checks that the Tokenize method returns the expected value.
  const TokenizeTestParams params = TokenizeParameterizedTest::GetParam();
  const TokenizedOutput output = Tokenize(
      /*input_str=*/params.input_str.c_str(),
      /*len=*/params.input_str.size(),
      /*max_tokens=*/params.max_tokens,
      /*exclude_nonalphaspace_tokens=*/params.exclude_nonalphaspace_tokens);

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
                            /*expected_output_str=*/"^hi!$"}),
        // Test not including non-alphanumeric characters.
        TokenizeTestParams({/*input_str=*/"hi!", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/true,
                            /*expected_output_str=*/"^hi $"}),
        // Test with a maximum of 3 tokens.
        TokenizeTestParams({/*input_str=*/"hi!", /*max_tokens=*/3,
                            /*exclude_alphanonspace=*/true,
                            /*expected_output_str=*/"^h$"}),
        // Test with non-latin characters.
        TokenizeTestParams({/*input_str=*/"ありがと", /*max_tokens=*/100,
                            /*exclude_alphanonspace=*/true,
                            /*expected_output_str=*/"^ありがと$"})));

TEST(TokenizeTest, NullInputTest) {
  const TokenizedOutput output = Tokenize(nullptr, 10, 10, true);

  // Tokenize should early return and contain only the prefix and suffix tokens.
  EXPECT_EQ(output.str, "^$");
}
TEST(LowercaseUnicodeTest, TestLowercaseUnicode) {
  {
    // Check that the method is a no-op when the string is lowercase.
    std::string input_str = "hello";
    std::string output_str;
    LowercaseUnicodeStr(
        /*input_str=*/input_str.c_str(),
        /*len=*/input_str.size(),
        /*output_str=*/&output_str);

    EXPECT_EQ(output_str, "hello");
  }
  {
    // Check that the method has uppercase characters.
    std::string input_str = "hElLo";
    std::string output_str;
    LowercaseUnicodeStr(
        /*input_str=*/input_str.c_str(),
        /*len=*/input_str.size(),
        /*output_str=*/&output_str);

    EXPECT_EQ(output_str, "hello");
  }
  {
    // Check that the method works with non-latin scripts.
    // Cyrillic has the concept of cases, so it should change the input.
    std::string input_str = "БЙп";
    std::string output_str;
    LowercaseUnicodeStr(
        /*input_str=*/input_str.c_str(),
        /*len=*/input_str.size(),
        /*output_str=*/&output_str);

    EXPECT_EQ(output_str, "бйп");
  }
  {
    // Check that the method works with non-latin scripts.
    // Japanese doesn't have the concept of cases, so it should not change.
    std::string input_str = "ありがと";
    std::string output_str;
    LowercaseUnicodeStr(
        /*input_str=*/input_str.c_str(),
        /*len=*/input_str.size(),
        /*output_str=*/&output_str);

    EXPECT_EQ(output_str, "ありがと");
  }
}

}  // namespace language_detection
