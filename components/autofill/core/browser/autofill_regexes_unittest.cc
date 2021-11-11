// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_regexes.h"

// Keep these tests in sync with
// components/autofill/core/browser/pattern_provider/default_regex_patterns_unittest.cc
// These tests wil be superceded once the pattern provider launches.

#include <stddef.h>

#include <string>

#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

struct InputPatternTestCase {
  const char16_t* const input;
  const char16_t* const pattern;
};

class PositiveSampleTest : public testing::TestWithParam<InputPatternTestCase> {
};

TEST_P(PositiveSampleTest, SampleRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  SCOPED_TRACE(test_case.pattern);
  EXPECT_TRUE(MatchesPattern(test_case.input, test_case.pattern));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         PositiveSampleTest,
                         testing::Values(
                             // Empty pattern
                             InputPatternTestCase{u"", u""},
                             InputPatternTestCase{
                                 u"Look, ma' -- a non-empty string!", u""},
                             // Substring
                             InputPatternTestCase{u"string", u"tri"},
                             // Substring at beginning
                             InputPatternTestCase{u"string", u"str"},
                             InputPatternTestCase{u"string", u"^str"},
                             // Substring at end
                             InputPatternTestCase{u"string", u"ring"},
                             InputPatternTestCase{u"string", u"ring$"},
                             // Case-insensitive
                             InputPatternTestCase{u"StRiNg", u"string"}));

class NegativeSampleTest : public testing::TestWithParam<InputPatternTestCase> {
};

TEST_P(NegativeSampleTest, SampleRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  SCOPED_TRACE(test_case.pattern);
  EXPECT_FALSE(MatchesPattern(test_case.input, test_case.pattern));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         NegativeSampleTest,
                         testing::Values(
                             // Empty string
                             InputPatternTestCase{
                                 u"", u"Look, ma' -- a non-empty pattern!"},
                             // Substring
                             InputPatternTestCase{u"string", u"trn"},
                             // Substring at beginning
                             InputPatternTestCase{u"string", u" str"},
                             InputPatternTestCase{u"string", u"^tri"},
                             // Substring at end
                             InputPatternTestCase{u"string", u"ring "},
                             InputPatternTestCase{u"string", u"rin$"}));

struct InputTestCase {
  const char16_t* const input;
};

class ExpirationDate2DigitYearPositive
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate2DigitYearPositive, ExpirationDate2DigitYearRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const std::u16string pattern = kExpirationDate2DigitYearRe;
  EXPECT_TRUE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ExpirationDate2DigitYearPositive,
    testing::Values(InputTestCase{u"mm / yy"},
                    InputTestCase{u"mm/ yy"},
                    InputTestCase{u"mm /yy"},
                    InputTestCase{u"mm/yy"},
                    InputTestCase{u"mm - yy"},
                    InputTestCase{u"mm- yy"},
                    InputTestCase{u"mm -yy"},
                    InputTestCase{u"mm-yy"},
                    InputTestCase{u"mmyy"},
                    // Complex two year cases
                    InputTestCase{u"Expiration Date (MM / YY)"},
                    InputTestCase{u"Expiration Date (MM/YY)"},
                    InputTestCase{u"Expiration Date (MM - YY)"},
                    InputTestCase{u"Expiration Date (MM-YY)"},
                    InputTestCase{u"Expiration Date MM / YY"},
                    InputTestCase{u"Expiration Date MM/YY"},
                    InputTestCase{u"Expiration Date MM - YY"},
                    InputTestCase{u"Expiration Date MM-YY"},
                    InputTestCase{u"expiration date yy"},
                    InputTestCase{u"Exp Date     (MM / YY)"}));

class ExpirationDate2DigitYearNegative
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate2DigitYearNegative, ExpirationDate2DigitYearRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const std::u16string pattern = kExpirationDate2DigitYearRe;
  EXPECT_FALSE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ExpirationDate2DigitYearNegative,
    testing::Values(InputTestCase{u""},
                    InputTestCase{u"Look, ma' -- an invalid string!"},
                    InputTestCase{u"mmfavouritewordyy"},
                    InputTestCase{u"mm a yy"},
                    InputTestCase{u"mm a yyyy"},
                    // Simple four year cases
                    InputTestCase{u"mm / yyyy"},
                    InputTestCase{u"mm/ yyyy"},
                    InputTestCase{u"mm /yyyy"},
                    InputTestCase{u"mm/yyyy"},
                    InputTestCase{u"mm - yyyy"},
                    InputTestCase{u"mm- yyyy"},
                    InputTestCase{u"mm -yyyy"},
                    InputTestCase{u"mm-yyyy"},
                    InputTestCase{u"mmyyyy"},
                    // Complex four year cases
                    InputTestCase{u"Expiration Date (MM / YYYY)"},
                    InputTestCase{u"Expiration Date (MM/YYYY)"},
                    InputTestCase{u"Expiration Date (MM - YYYY)"},
                    InputTestCase{u"Expiration Date (MM-YYYY)"},
                    InputTestCase{u"Expiration Date MM / YYYY"},
                    InputTestCase{u"Expiration Date MM/YYYY"},
                    InputTestCase{u"Expiration Date MM - YYYY"},
                    InputTestCase{u"Expiration Date MM-YYYY"},
                    InputTestCase{u"expiration date yyyy"},
                    InputTestCase{u"Exp Date     (MM / YYYY)"}));

class ExpirationDate4DigitYearPositive
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate4DigitYearPositive, ExpirationDate4DigitYearRegexes) {
  auto test_case = GetParam();
  const std::u16string pattern = kExpirationDate4DigitYearRe;
  SCOPED_TRACE(test_case.input);
  EXPECT_TRUE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         ExpirationDate4DigitYearPositive,
                         testing::Values(
                             // Simple four year cases
                             InputTestCase{u"mm / yyyy"},
                             InputTestCase{u"mm/ yyyy"},
                             InputTestCase{u"mm /yyyy"},
                             InputTestCase{u"mm/yyyy"},
                             InputTestCase{u"mm - yyyy"},
                             InputTestCase{u"mm- yyyy"},
                             InputTestCase{u"mm -yyyy"},
                             InputTestCase{u"mm-yyyy"},
                             InputTestCase{u"mmyyyy"},
                             // Complex four year cases
                             InputTestCase{u"Expiration Date (MM / YYYY)"},
                             InputTestCase{u"Expiration Date (MM/YYYY)"},
                             InputTestCase{u"Expiration Date (MM - YYYY)"},
                             InputTestCase{u"Expiration Date (MM-YYYY)"},
                             InputTestCase{u"Expiration Date MM / YYYY"},
                             InputTestCase{u"Expiration Date MM/YYYY"},
                             InputTestCase{u"Expiration Date MM - YYYY"},
                             InputTestCase{u"Expiration Date MM-YYYY"},
                             InputTestCase{u"expiration date yyyy"},
                             InputTestCase{u"Exp Date     (MM / YYYY)"}));

class ExpirationDate4DigitYearNegative
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate4DigitYearNegative, ExpirationDate4DigitYearRegexes) {
  auto test_case = GetParam();
  const std::u16string pattern = kExpirationDate4DigitYearRe;
  SCOPED_TRACE(test_case.input);
  EXPECT_FALSE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ExpirationDate4DigitYearNegative,
    testing::Values(InputTestCase{u""},
                    InputTestCase{u"Look, ma' -- an invalid string!"},
                    InputTestCase{u"mmfavouritewordyy"},
                    InputTestCase{u"mm a yy"},
                    InputTestCase{u"mm a yyyy"},
                    // Simple two year cases
                    InputTestCase{u"mm / yy"},
                    InputTestCase{u"mm/ yy"},
                    InputTestCase{u"mm /yy"},
                    InputTestCase{u"mm/yy"},
                    InputTestCase{u"mm - yy"},
                    InputTestCase{u"mm- yy"},
                    InputTestCase{u"mm -yy"},
                    InputTestCase{u"mm-yy"},
                    InputTestCase{u"mmyy"},
                    // Complex two year cases
                    InputTestCase{u"Expiration Date (MM / YY)"},
                    InputTestCase{u"Expiration Date (MM/YY)"},
                    InputTestCase{u"Expiration Date (MM - YY)"},
                    InputTestCase{u"Expiration Date (MM-YY)"},
                    InputTestCase{u"Expiration Date MM / YY"},
                    InputTestCase{u"Expiration Date MM/YY"},
                    InputTestCase{u"Expiration Date MM - YY"},
                    InputTestCase{u"Expiration Date MM-YY"},
                    InputTestCase{u"expiration date yy"},
                    InputTestCase{u"Exp Date     (MM / YY)"}));

class ZipCodePositive : public testing::TestWithParam<InputTestCase> {};

TEST_P(ZipCodePositive, ZipCodeRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const std::u16string pattern = kZipCodeRe;
  EXPECT_TRUE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         ZipCodePositive,
                         testing::Values(InputTestCase{u"Zip code"},
                                         InputTestCase{u"postal code"},
                                         InputTestCase{u"postleitzahl"}));

class ZipCodeNegative : public testing::TestWithParam<InputTestCase> {};

TEST_P(ZipCodeNegative, ZipCodeRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const std::u16string pattern = kZipCodeRe;
  EXPECT_FALSE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ZipCodeNegative,
    testing::Values(InputTestCase{u""},
                    InputTestCase{
                        u"Supported file formats: .docx, .rar, .zip."}));

class Zip4Positive : public testing::TestWithParam<InputTestCase> {};

TEST_P(Zip4Positive, Zip4Regexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const std::u16string pattern = kZip4Re;
  EXPECT_TRUE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         Zip4Positive,
                         testing::Values(InputTestCase{u"Zip code"}));

class Zip4Negative : public testing::TestWithParam<InputTestCase> {};

TEST_P(Zip4Negative, Zip4Regexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const std::u16string pattern = kZip4Re;
  EXPECT_FALSE(MatchesPattern(test_case.input, pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    Zip4Negative,
    testing::Values(InputTestCase{u""},
                    InputTestCase{
                        u"Supported file formats: .docx, .rar, .zip."}));

}  // namespace autofill
