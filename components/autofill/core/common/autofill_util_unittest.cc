// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

struct AtSignPrefixCase {
  const char* const field_suggestion;
  const char* const field_contents;
  const bool expected_result;
};

class PrefixEndingOnTokenBoundaryTest
    : public testing::TestWithParam<AtSignPrefixCase> {};

TEST_P(PrefixEndingOnTokenBoundaryTest, IsPrefixOfEmailEndingWithAtSign) {
  auto test_case = GetParam();
  SCOPED_TRACE(testing::Message()
               << "suggestion = " << test_case.field_suggestion
               << ", contents = " << test_case.field_contents);

  EXPECT_EQ(test_case.expected_result,
            IsPrefixOfEmailEndingWithAtSign(
                base::ASCIIToUTF16(test_case.field_suggestion),
                base::ASCIIToUTF16(test_case.field_contents)));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillUtilTest,
    PrefixEndingOnTokenBoundaryTest,
    testing::Values(AtSignPrefixCase{"ab@cd.b", "a", false},
                    AtSignPrefixCase{"ab@cd.b", "b", false},
                    AtSignPrefixCase{"ab@cd.b", "Ab", false},
                    AtSignPrefixCase{"ab@cd.b", "cd", false},
                    AtSignPrefixCase{"ab@cd.b", "d", false},
                    AtSignPrefixCase{"ab@cd.b", "b@", false},
                    AtSignPrefixCase{"ab@cd.b", "cd.b", false},
                    AtSignPrefixCase{"ab@cd.b", "b@cd", false},
                    AtSignPrefixCase{"ab@cd.b", "ab@c", false},
                    AtSignPrefixCase{"ba.a.ab", "a.a", false},
                    AtSignPrefixCase{"", "ab", false},
                    AtSignPrefixCase{"ab@c", "ab@", false},
                    AtSignPrefixCase{"ab@cd@g", "ab", true},
                    AtSignPrefixCase{"ab@cd@g", "ab@cd", true},
                    AtSignPrefixCase{"abc", "abc", false},
                    AtSignPrefixCase{"ab", "", false},
                    AtSignPrefixCase{"ab@cd.b", "ab", true}));

// Tests for LowercaseAndTokenizeAttributeString
struct LowercaseAndTokenizeAttributeStringCase {
  const char* const attribute;
  std::vector<std::string> tokens;
};

class LowercaseAndTokenizeAttributeStringTest
    : public testing::TestWithParam<LowercaseAndTokenizeAttributeStringCase> {};

TEST_P(LowercaseAndTokenizeAttributeStringTest,
       LowercaseAndTokenizeAttributeStringTest) {
  auto test_case = GetParam();
  SCOPED_TRACE(testing::Message() << "attribute = " << test_case.attribute);

  EXPECT_EQ(test_case.tokens,
            LowercaseAndTokenizeAttributeString(test_case.attribute));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillUtilTest,
    LowercaseAndTokenizeAttributeStringTest,
    testing::Values(
        // Test leading and trailing whitespace, test tabs and newlines
        LowercaseAndTokenizeAttributeStringCase{"foo bar baz",
                                                {"foo", "bar", "baz"}},
        LowercaseAndTokenizeAttributeStringCase{" foo bar baz ",
                                                {"foo", "bar", "baz"}},
        LowercaseAndTokenizeAttributeStringCase{"foo\tbar baz ",
                                                {"foo", "bar", "baz"}},
        LowercaseAndTokenizeAttributeStringCase{"foo\nbar baz ",
                                                {"foo", "bar", "baz"}},

        // Test different forms of capitalization
        LowercaseAndTokenizeAttributeStringCase{"FOO BAR BAZ",
                                                {"foo", "bar", "baz"}},
        LowercaseAndTokenizeAttributeStringCase{"foO baR bAz",
                                                {"foo", "bar", "baz"}},

        // Test collapsing of multiple whitespace characters in a row
        LowercaseAndTokenizeAttributeStringCase{"  \t\t\n\n   ",
                                                std::vector<std::string>()},
        LowercaseAndTokenizeAttributeStringCase{"foO    baR bAz",
                                                {"foo", "bar", "baz"}}));

TEST(StripAuthAndParamsTest, StripsAll) {
  GURL url = GURL("https://login:password@example.com/login/?param=value#ref");
  EXPECT_EQ(GURL("https://example.com/login/"), StripAuthAndParams(url));
}

TEST(SanitizeCreditCardFieldValueTest, SanitizeCreditCardFieldValue) {
  EXPECT_EQ(u"1231231111", SanitizeCreditCardFieldValue(u" 123-123-1111 "));
}

}  // namespace autofill
