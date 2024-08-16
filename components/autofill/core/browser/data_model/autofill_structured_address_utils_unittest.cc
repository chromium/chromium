// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

#include <stddef.h>
#include <map>
#include <string>
#include <vector>

#include "base/i18n/char_iterator.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Element-wise comparison operator.
bool operator==(const AddressToken& lhs, const AddressToken& rhs) {
  return lhs.value == rhs.value &&
         lhs.normalized_value == rhs.normalized_value &&
         lhs.position == rhs.position;
}

// Regular expression with named capture groups for parsing US-style names.
char kFirstMiddleLastRe[] =
    "^(?P<NAME_FULL>((?P<NAME_FIRST>\\w+)\\s)?"
    "((?P<NAME_MIDDLE>(\\w+(?:\\s+\\w+)*))\\s)??"
    "(?P<NAME_LAST>\\w+))$";

// Test the successful parsing of a value by a regular expression.
TEST(AutofillStructuredAddressUtils, TestParseValueByRegularExpression) {
  std::string regex = kFirstMiddleLastRe;
  std::string value = "first middle1 middle2 middle3 last";

  std::optional<base::flat_map<std::string, std::string>> result_map;

  result_map = ParseValueByRegularExpression(value, regex);

  EXPECT_TRUE(result_map);
  EXPECT_EQ((*result_map)["NAME_FULL"], value);
  EXPECT_EQ((*result_map)["NAME_FIRST"], "first");
  EXPECT_EQ((*result_map)["NAME_MIDDLE"], "middle1 middle2 middle3");
  EXPECT_EQ((*result_map)["NAME_LAST"], "last");

  // Parse a name with only one middle name.
  value = "first middle1 last";
  result_map = ParseValueByRegularExpression(value, regex);

  EXPECT_TRUE(result_map);
  EXPECT_EQ((*result_map)["NAME_FULL"], value);
  EXPECT_EQ((*result_map)["NAME_FIRST"], "first");
  EXPECT_EQ((*result_map)["NAME_MIDDLE"], "middle1");
  EXPECT_EQ((*result_map)["NAME_LAST"], "last");

  // Parse a name without a middle name.
  value = "first last";
  result_map = ParseValueByRegularExpression(value, regex);

  // Verify the expectation.
  EXPECT_TRUE(result_map);
  EXPECT_EQ((*result_map)["NAME_FULL"], value);
  EXPECT_EQ((*result_map)["NAME_FIRST"], "first");
  EXPECT_EQ((*result_map)["NAME_MIDDLE"], "");
  EXPECT_EQ((*result_map)["NAME_LAST"], "last");

  // Parse a name without only a last name.
  value = "last";
  result_map = ParseValueByRegularExpression(value, regex);

  // Verify the expectations.
  EXPECT_TRUE(result_map);
  EXPECT_EQ((*result_map)["NAME_FULL"], value);
  EXPECT_EQ((*result_map)["NAME_FIRST"], "");
  EXPECT_EQ((*result_map)["NAME_MIDDLE"], "");
  EXPECT_EQ((*result_map)["NAME_LAST"], "last");

  // Parse an empty name that should not be successful.
  value = "";
  result_map = ParseValueByRegularExpression(value, regex);

  // Verify the expectations.
  EXPECT_FALSE(result_map);
}

TEST(AutofillStructuredAddressUtils,
     TestParseValueByRegularExpression_OnlyPartialMatch) {
  std::string regex = "(!<GROUP>this)";
  std::string value = "this is missing";

  EXPECT_FALSE(ParseValueByRegularExpression(value, regex));
}

TEST(AutofillStructuredAddressUtils,
     TestParseValueByRegularExpression_InvalidRegEx) {
  std::string regex = "(!<INVALID";
  std::string value = "first middle1 middle2 middle3 last";

  EXPECT_FALSE(ParseValueByRegularExpression(value, regex));
  auto expression = BuildRegExFromPattern(regex);
  EXPECT_FALSE(ParseValueByRegularExpression(value, expression.get()));
}

// Test the matching of a value against a regular expression.
TEST(AutofillStructuredAddressUtils, TestIsPartialMatch) {
  EXPECT_TRUE(IsPartialMatch("123 sdf 123", "sdf"));
  EXPECT_FALSE(IsPartialMatch("123 sdf 123", "^sdf$"));
}

// Test the matching of a value against an invalid regular expression.
TEST(AutofillStructuredAddressUtils, TestIsPartialMatch_InvalidRegEx) {
  EXPECT_FALSE(IsPartialMatch("123 sdf 123", "(!<sdf"));
}

// Test the caching of regular expressions.
TEST(AutofillStructuredAddressUtils, TestRegExCaching) {
  std::string pattern = "(?P<SOME_EXPRESSION>.)";
  // Verify that the pattern is not cached yet.
  EXPECT_FALSE(Re2RegExCache::Instance()->IsRegExCachedForTesting(pattern));

  // Request the regular expression and verify that it is cached afterwards.
  Re2RegExCache::Instance()->GetRegEx(pattern);
  EXPECT_TRUE(Re2RegExCache::Instance()->IsRegExCachedForTesting(pattern));
}

TEST(AutofillStructuredAddressUtils, TestGetAllPartialMatches) {
  std::string input = "abaacaada";
  std::string pattern = "(a.a)";

  std::vector<std::string> expectation = {"aba", "aca", "ada"};
  EXPECT_TRUE(IsPartialMatch(input, pattern));
  EXPECT_EQ(GetAllPartialMatches(input, pattern), expectation);
}

TEST(AutofillStructuredAddressUtils, TestGetAllPartialMatches_InvalidPattern) {
  std::string input = "abaacaada";
  std::string pattern = "(a.a";

  std::vector<std::string> expectation = {};
  EXPECT_FALSE(IsPartialMatch(input, pattern));
  EXPECT_EQ(GetAllPartialMatches(input, pattern), expectation);
}

TEST(AutofillStructuredAddressUtils,
     TestExtractAllPlaceholders_Isolated_Placeholder) {
  std::string input = "${HOLDER1}";
  std::vector<std::string> expectation = {"HOLDER1"};
  EXPECT_EQ(ExtractAllPlaceholders(input), expectation);
}

TEST(AutofillStructuredAddressUtils,
     TestExtractAllPlaceholders_Placeholder_In_Text) {
  std::string input = "Some ${HOLDER1} Text";
  std::vector<std::string> expectation = {"HOLDER1"};
  EXPECT_EQ(ExtractAllPlaceholders(input), expectation);
}

TEST(AutofillStructuredAddressUtils,
     TestExtractAllPlaceholders_Multiple_Placeholders_In_Text) {
  std::string input = "Some ${HOLDER1} Text ${HOLDER2}";
  std::vector<std::string> expectation = {"HOLDER1", "HOLDER2"};
  EXPECT_EQ(ExtractAllPlaceholders(input), expectation);
}

TEST(AutofillStructuredAddressUtils, TestExtractAllPlaceholders_Broken_Syntax) {
  std::string input = "Some ${HOLDER1} }} ";
  std::vector<std::string> expectation = {"HOLDER1"};
  EXPECT_EQ(ExtractAllPlaceholders(input), expectation);
}

TEST(AutofillStructuredAddressUtils,
     TestExtractAllPlaceholders_Nested_Placeholders) {
  std::string input = "Some ${HOLDER${INANHOLDER}} }} ";
  std::vector<std::string> expectation = {"INANHOLDER"};
  EXPECT_EQ(ExtractAllPlaceholders(input), expectation);
}

TEST(AutofillStructuredAddressUtils, TestGetPlaceholderToken) {
  EXPECT_EQ("${VAR}", GetPlaceholderToken("VAR"));
}

TEST(AutofillStructuredAddressUtils, CaptureTypeWithPattern) {
  EXPECT_EQ("(?i:(?P<NAME_FULL>abs\\w)(?:,|\\s+|$)+)?",
            CaptureTypeWithPattern(NAME_FULL, {"abs", "\\w"},
                                   {.quantifier = MatchQuantifier::kOptional}));
  EXPECT_EQ("(?i:(?P<NAME_FULL>abs\\w)(?:,|\\s+|$)+)",
            CaptureTypeWithPattern(NAME_FULL, {"abs", "\\w"}));
  EXPECT_EQ(
      "(?i:(?P<NAME_FULL>abs\\w)(?:,|\\s+|$)+)??",
      CaptureTypeWithPattern(NAME_FULL, "abs\\w",
                             {.quantifier = MatchQuantifier::kLazyOptional}));
  EXPECT_EQ("(?i:(?P<NAME_FULL>abs\\w)(?:,|\\s+|$)+)",
            CaptureTypeWithPattern(NAME_FULL, "abs\\w"));
  EXPECT_EQ("(?i:(?P<NAME_FULL>abs\\w)(?:_)+)",
            CaptureTypeWithPattern(NAME_FULL, "abs\\w", {.separator = "_"}));
}

TEST(AutofillStructuredAddressUtils, NoCaptureTypeWithPattern) {
  EXPECT_EQ(
      "(?i:abs\\w(?:,|\\s+|$)+)?",
      NoCapturePattern("abs\\w", {.quantifier = MatchQuantifier::kOptional}));
  EXPECT_EQ("(?i:abs\\w(?:,|\\s+|$)+)", NoCapturePattern("abs\\w"));
  EXPECT_EQ("(?i:abs\\w(?:,|\\s+|$)+)??",
            NoCapturePattern("abs\\w",
                             {.quantifier = MatchQuantifier::kLazyOptional}));
  EXPECT_EQ("(?i:abs\\w(?:,|\\s+|$)+)", NoCapturePattern("abs\\w"));
  EXPECT_EQ("(?i:abs\\w(?:_)+)",
            NoCapturePattern("abs\\w", {.separator = "_"}));
}

TEST(AutofillStructuredAddressUtils, TokenizeValue) {
  std::vector<AddressToken> expected_tokens = {
      {u"AnD", u"and", 1},
      {u"anotherOne", u"anotherone", 2},
      {u"valUe", u"value", 0}};

  EXPECT_EQ(TokenizeValue(u"  valUe AnD    anotherOne"), expected_tokens);

  std::vector<AddressToken> expected_cjk_tokens = {
      {u"영", u"영", 1}, {u"이", u"이", 0}, {u"호", u"호", 2}};

  EXPECT_EQ(TokenizeValue(u"이영 호"), expected_cjk_tokens);
  EXPECT_EQ(TokenizeValue(u"이・영호"), expected_cjk_tokens);
  EXPECT_EQ(TokenizeValue(u"이영 호"), expected_cjk_tokens);
}

TEST(AutofillStructuredAddressUtils, NormalizeValue) {
  EXPECT_EQ(NormalizeValue(u" MÜLLeR   Örber"), u"muller orber");
}

TEST(AutofillStructuredAddressUtils, TestGetRewriter) {
  EXPECT_EQ(NormalizeAndRewrite(AddressCountryCode("us"), u"unit #3",
                                /*keep_white_space=*/true),
            u"u 3");
  EXPECT_EQ(NormalizeAndRewrite(AddressCountryCode("us"), u"california",
                                /*keep_white_space=*/true),
            u"ca");
}

TEST(AutofillStructuredAddressUtils, AreStringTokenCompatible) {
  EXPECT_TRUE(AreStringTokenCompatible(u"moto hello", u"hello, moto"));
  EXPECT_TRUE(AreStringTokenCompatible(u"moto hello", u"hello, moto cross"));
  EXPECT_FALSE(
      AreStringTokenCompatible(u"moto hello, extra", u"hello, moto cross"));
  EXPECT_TRUE(AreStringTokenCompatible(u"us foo", u"used, foo,us"));
  EXPECT_FALSE(AreStringTokenCompatible(u"us foo", u"used, foo"));
}

}  // namespace autofill
