// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Tests for FieldIsSuggestionSubstringStartingOnTokenBoundary().
struct FieldIsTokenBoundarySubstringCase {
  const char* const field_suggestion;
  const char* const field_contents;
  const bool case_sensitive;
  const bool expected_result;
};

class FieldIsTokenBoundarySubstringCaseTest
    : public testing::TestWithParam<FieldIsTokenBoundarySubstringCase> {};

TEST_P(FieldIsTokenBoundarySubstringCaseTest,
       FieldIsSuggestionSubstringStartingOnTokenBoundary) {
  {
    base::test::ScopedFeatureList features_disabled;
    features_disabled.InitAndDisableFeature(
        features::kAutofillTokenPrefixMatching);

    // FieldIsSuggestionSubstringStartingOnTokenBoundary should not work yet
    // without a flag.
    EXPECT_FALSE(FieldIsSuggestionSubstringStartingOnTokenBoundary(
        base::ASCIIToUTF16("ab@cd.b"), base::ASCIIToUTF16("b"), false));
  }

  base::test::ScopedFeatureList features_enabled;
  features_enabled.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  auto test_case = GetParam();
  SCOPED_TRACE(testing::Message()
               << "suggestion = " << test_case.field_suggestion
               << ", contents = " << test_case.field_contents
               << ", case_sensitive = " << test_case.case_sensitive);

  EXPECT_EQ(test_case.expected_result,
            FieldIsSuggestionSubstringStartingOnTokenBoundary(
                base::ASCIIToUTF16(test_case.field_suggestion),
                base::ASCIIToUTF16(test_case.field_contents),
                test_case.case_sensitive));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillUtilTest,
    FieldIsTokenBoundarySubstringCaseTest,
    testing::Values(
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "a", false, true},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "b", false, true},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "Ab", false, true},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "Ab", true, false},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "cd", true, true},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "d", false, false},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "b@", true, false},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "ab", false, true},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "cd.b", true, true},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "b@cd", false, false},
        FieldIsTokenBoundarySubstringCase{"ab@cd.b", "ab@c", false, true},
        FieldIsTokenBoundarySubstringCase{"ba.a.ab", "a.a", false, true},
        FieldIsTokenBoundarySubstringCase{"", "ab", false, false},
        FieldIsTokenBoundarySubstringCase{"", "ab", true, false},
        FieldIsTokenBoundarySubstringCase{"ab", "", false, true},
        FieldIsTokenBoundarySubstringCase{"ab", "", true, true}));

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
        GetTextSelectionStartCase{"ab@cd.b", "A", true, base::string16::npos},
        GetTextSelectionStartCase{"ab@cd.b", "Ab", false, 2},
        GetTextSelectionStartCase{"ab@cd.b", "Ab", true, base::string16::npos},
        GetTextSelectionStartCase{"ab@cd.b", "cd", false, 5},
        GetTextSelectionStartCase{"ab@cd.b", "ab@c", false, 4},
        GetTextSelectionStartCase{"ab@cd.b", "cd.b", false, 7},
        GetTextSelectionStartCase{"ab@cd.b", "b@cd", false,
                                  base::string16::npos},
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

}  // namespace autofill
