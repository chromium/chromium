// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_regexes.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

struct InputPatternTestCase {
  const char* const input;
  const char* const pattern;
};

class PositiveSampleTest : public testing::TestWithParam<InputPatternTestCase> {
};

TEST_P(PositiveSampleTest, SampleRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  SCOPED_TRACE(test_case.pattern);
  EXPECT_TRUE(MatchesPattern(ASCIIToUTF16(test_case.input),
                             ASCIIToUTF16(test_case.pattern)));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         PositiveSampleTest,
                         testing::Values(
                             // Empty pattern
                             InputPatternTestCase{"", ""},
                             InputPatternTestCase{
                                 "Look, ma' -- a non-empty string!", ""},
                             // Substring
                             InputPatternTestCase{"string", "tri"},
                             // Substring at beginning
                             InputPatternTestCase{"string", "str"},
                             InputPatternTestCase{"string", "^str"},
                             // Substring at end
                             InputPatternTestCase{"string", "ring"},
                             InputPatternTestCase{"string", "ring$"},
                             // Case-insensitive
                             InputPatternTestCase{"StRiNg", "string"}));

class NegativeSampleTest : public testing::TestWithParam<InputPatternTestCase> {
};

TEST_P(NegativeSampleTest, SampleRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  SCOPED_TRACE(test_case.pattern);
  EXPECT_FALSE(MatchesPattern(ASCIIToUTF16(test_case.input),
                              ASCIIToUTF16(test_case.pattern)));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         NegativeSampleTest,
                         testing::Values(
                             // Empty string
                             InputPatternTestCase{
                                 "", "Look, ma' -- a non-empty pattern!"},
                             // Substring
                             InputPatternTestCase{"string", "trn"},
                             // Substring at beginning
                             InputPatternTestCase{"string", " str"},
                             InputPatternTestCase{"string", "^tri"},
                             // Substring at end
                             InputPatternTestCase{"string", "ring "},
                             InputPatternTestCase{"string", "rin$"}));

struct InputTestCase {
  const char* const input;
};

class ExpirationDate2DigitYearPositive
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate2DigitYearPositive, ExpirationDate2DigitYearRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const base::string16 pattern = ASCIIToUTF16(kExpirationDate2DigitYearRe);
  EXPECT_TRUE(MatchesPattern(ASCIIToUTF16(test_case.input), pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ExpirationDate2DigitYearPositive,
    testing::Values(InputTestCase{"mm / yy"},
                    InputTestCase{"mm/ yy"},
                    InputTestCase{"mm /yy"},
                    InputTestCase{"mm/yy"},
                    InputTestCase{"mm - yy"},
                    InputTestCase{"mm- yy"},
                    InputTestCase{"mm -yy"},
                    InputTestCase{"mm-yy"},
                    InputTestCase{"mmyy"},
                    // Complex two year cases
                    InputTestCase{"Expiration Date (MM / YY)"},
                    InputTestCase{"Expiration Date (MM/YY)"},
                    InputTestCase{"Expiration Date (MM - YY)"},
                    InputTestCase{"Expiration Date (MM-YY)"},
                    InputTestCase{"Expiration Date MM / YY"},
                    InputTestCase{"Expiration Date MM/YY"},
                    InputTestCase{"Expiration Date MM - YY"},
                    InputTestCase{"Expiration Date MM-YY"},
                    InputTestCase{"expiration date yy"},
                    InputTestCase{"Exp Date     (MM / YY)"}));

class ExpirationDate2DigitYearNegative
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate2DigitYearNegative, ExpirationDate2DigitYearRegexes) {
  auto test_case = GetParam();
  SCOPED_TRACE(test_case.input);
  const base::string16 pattern = ASCIIToUTF16(kExpirationDate2DigitYearRe);
  EXPECT_FALSE(MatchesPattern(ASCIIToUTF16(test_case.input), pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ExpirationDate2DigitYearNegative,
    testing::Values(InputTestCase{""},
                    InputTestCase{"Look, ma' -- an invalid string!"},
                    InputTestCase{"mmfavouritewordyy"},
                    InputTestCase{"mm a yy"},
                    InputTestCase{"mm a yyyy"},
                    // Simple four year cases
                    InputTestCase{"mm / yyyy"},
                    InputTestCase{"mm/ yyyy"},
                    InputTestCase{"mm /yyyy"},
                    InputTestCase{"mm/yyyy"},
                    InputTestCase{"mm - yyyy"},
                    InputTestCase{"mm- yyyy"},
                    InputTestCase{"mm -yyyy"},
                    InputTestCase{"mm-yyyy"},
                    InputTestCase{"mmyyyy"},
                    // Complex four year cases
                    InputTestCase{"Expiration Date (MM / YYYY)"},
                    InputTestCase{"Expiration Date (MM/YYYY)"},
                    InputTestCase{"Expiration Date (MM - YYYY)"},
                    InputTestCase{"Expiration Date (MM-YYYY)"},
                    InputTestCase{"Expiration Date MM / YYYY"},
                    InputTestCase{"Expiration Date MM/YYYY"},
                    InputTestCase{"Expiration Date MM - YYYY"},
                    InputTestCase{"Expiration Date MM-YYYY"},
                    InputTestCase{"expiration date yyyy"},
                    InputTestCase{"Exp Date     (MM / YYYY)"}));

class ExpirationDate4DigitYearPositive
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate4DigitYearPositive, ExpirationDate4DigitYearRegexes) {
  auto test_case = GetParam();
  const base::string16 pattern = ASCIIToUTF16(kExpirationDate4DigitYearRe);
  SCOPED_TRACE(test_case.input);
  EXPECT_TRUE(MatchesPattern(ASCIIToUTF16(test_case.input), pattern));
}

INSTANTIATE_TEST_SUITE_P(AutofillRegexes,
                         ExpirationDate4DigitYearPositive,
                         testing::Values(
                             // Simple four year cases
                             InputTestCase{"mm / yyyy"},
                             InputTestCase{"mm/ yyyy"},
                             InputTestCase{"mm /yyyy"},
                             InputTestCase{"mm/yyyy"},
                             InputTestCase{"mm - yyyy"},
                             InputTestCase{"mm- yyyy"},
                             InputTestCase{"mm -yyyy"},
                             InputTestCase{"mm-yyyy"},
                             InputTestCase{"mmyyyy"},
                             // Complex four year cases
                             InputTestCase{"Expiration Date (MM / YYYY)"},
                             InputTestCase{"Expiration Date (MM/YYYY)"},
                             InputTestCase{"Expiration Date (MM - YYYY)"},
                             InputTestCase{"Expiration Date (MM-YYYY)"},
                             InputTestCase{"Expiration Date MM / YYYY"},
                             InputTestCase{"Expiration Date MM/YYYY"},
                             InputTestCase{"Expiration Date MM - YYYY"},
                             InputTestCase{"Expiration Date MM-YYYY"},
                             InputTestCase{"expiration date yyyy"},
                             InputTestCase{"Exp Date     (MM / YYYY)"}));

class ExpirationDate4DigitYearNegative
    : public testing::TestWithParam<InputTestCase> {};

TEST_P(ExpirationDate4DigitYearNegative, ExpirationDate4DigitYearRegexes) {
  auto test_case = GetParam();
  const base::string16 pattern = ASCIIToUTF16(kExpirationDate4DigitYearRe);
  SCOPED_TRACE(test_case.input);
  EXPECT_FALSE(MatchesPattern(ASCIIToUTF16(test_case.input), pattern));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillRegexes,
    ExpirationDate4DigitYearNegative,
    testing::Values(InputTestCase{""},
                    InputTestCase{"Look, ma' -- an invalid string!"},
                    InputTestCase{"mmfavouritewordyy"},
                    InputTestCase{"mm a yy"},
                    InputTestCase{"mm a yyyy"},
                    // Simple two year cases
                    InputTestCase{"mm / yy"},
                    InputTestCase{"mm/ yy"},
                    InputTestCase{"mm /yy"},
                    InputTestCase{"mm/yy"},
                    InputTestCase{"mm - yy"},
                    InputTestCase{"mm- yy"},
                    InputTestCase{"mm -yy"},
                    InputTestCase{"mm-yy"},
                    InputTestCase{"mmyy"},
                    // Complex two year cases
                    InputTestCase{"Expiration Date (MM / YY)"},
                    InputTestCase{"Expiration Date (MM/YY)"},
                    InputTestCase{"Expiration Date (MM - YY)"},
                    InputTestCase{"Expiration Date (MM-YY)"},
                    InputTestCase{"Expiration Date MM / YY"},
                    InputTestCase{"Expiration Date MM/YY"},
                    InputTestCase{"Expiration Date MM - YY"},
                    InputTestCase{"Expiration Date MM-YY"},
                    InputTestCase{"expiration date yy"},
                    InputTestCase{"Exp Date     (MM / YY)"}));

}  // namespace autofill
