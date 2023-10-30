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

// Tests for GetTextSelectionStart().
struct GetTextSelectionStartCase {
  const char* const field_suggestion;
  const char* const field_contents;
  const bool case_sensitive;
  const size_t expected_start;
};

class GetTextSelectionStartTest
    : public testing::TestWithParam<GetTextSelectionStartCase> {};

TEST_P(GetTextSelectionStartTest, GetTextSelectionStart) {
  auto test_case = GetParam();
  SCOPED_TRACE(testing::Message()
               << "suggestion = " << test_case.field_suggestion
               << ", contents = " << test_case.field_contents
               << ", case_sensitive = " << test_case.case_sensitive);
  EXPECT_EQ(
      test_case.expected_start,
      GetTextSelectionStart(base::ASCIIToUTF16(test_case.field_suggestion),
                            base::ASCIIToUTF16(test_case.field_contents),
                            test_case.case_sensitive));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillUtilTest,
    GetTextSelectionStartTest,
    testing::Values(
        GetTextSelectionStartCase{"ab@cd.b", "a", false, 1},
        GetTextSelectionStartCase{"ab@cd.b", "A", true, std::u16string::npos},
        GetTextSelectionStartCase{"ab@cd.b", "Ab", false, 2},
        GetTextSelectionStartCase{"ab@cd.b", "Ab", true, std::u16string::npos},
        GetTextSelectionStartCase{"ab@cd.b", "cd", false, 5},
        GetTextSelectionStartCase{"ab@cd.b", "ab@c", false, 4},
        GetTextSelectionStartCase{"ab@cd.b", "cd.b", false, 7},
        GetTextSelectionStartCase{"ab@cd.b", "b@cd", false,
                                  std::u16string::npos},
        GetTextSelectionStartCase{"ab@cd.b", "b", false, 7},
        GetTextSelectionStartCase{"ba.a.ab", "a.a", false, 6},
        GetTextSelectionStartCase{"texample@example.com", "example", false,
                                  16}));

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

TEST(LevenshteinDistanceTest, WithoutMaxDistance) {
  EXPECT_EQ(LevenshteinDistance(u"aa", u"aa"), 0u);
  EXPECT_EQ(LevenshteinDistance(u"a", u"aa"), 1u);
  EXPECT_EQ(LevenshteinDistance(u"aba", u"aa"), 1u);
  EXPECT_EQ(LevenshteinDistance(u"", u"12"), 2u);
  EXPECT_EQ(LevenshteinDistance(u"street", u"str."), 3u);
  EXPECT_EQ(LevenshteinDistance(u"asdf", u"fdsa"), 4u);
  EXPECT_EQ(
      LevenshteinDistance(std::u16string(100, 'a'), std::u16string(200, 'a')),
      100u);
}

TEST(LevenshteinDistanceTest, WithMaxDistance) {
  EXPECT_EQ(LevenshteinDistance(u"aa", u"aa", 0), 0u);
  EXPECT_EQ(LevenshteinDistance(u"a", u"aa", 1), 1u);
  EXPECT_EQ(LevenshteinDistance(u"ab", u"aa", 2), 1u);
  EXPECT_EQ(LevenshteinDistance(u"aba", u"aa", 3), 1u);

  // In the case where k is less than the LevenshteinDistance() this should
  // return k+1.
  EXPECT_EQ(LevenshteinDistance(u"", u"12", 1), 2u);
  EXPECT_EQ(LevenshteinDistance(u"street", u"str.", 1), 2u);
  EXPECT_EQ(LevenshteinDistance(u"asdf", u"fdsa", 2), 3u);
  EXPECT_EQ(LevenshteinDistance(std::u16string(100, 'a'),
                                std::u16string(200, 'a'), 50),
            51u);
}

TEST(StripAuthAndParamsTest, StripsAll) {
  GURL url = GURL("https://login:password@example.com/login/?param=value#ref");
  EXPECT_EQ(GURL("https://example.com/login/"), StripAuthAndParams(url));
}

}  // namespace autofill
