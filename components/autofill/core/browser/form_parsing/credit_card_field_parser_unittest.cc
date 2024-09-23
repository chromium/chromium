// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/credit_card_field_parser.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {
// Returns a vector of numeric months with a leading 0 and an additional "MM"
// entry.
std::vector<SelectOption> GetMonths() {
  std::vector<std::string> months{"MM", "01", "02", "03", "04", "05", "06",
                                  "07", "08", "09", "10", "11", "12"};
  std::vector<SelectOption> options;
  for (const std::string& month : months)
    options.push_back({base::ASCIIToUTF16(month), base::ASCIIToUTF16(month)});
  return options;
}

// Returns a vector of 10 consecutive years starting today in $ digit format
// and an additional "YYYY" entry.
std::vector<SelectOption> Get4DigitYears() {
  std::vector<SelectOption> years = {{u"YYYY", u"YYYY"}};

  const base::Time time_now = AutofillClock::Now();
  base::Time::Exploded time_exploded;
  time_now.UTCExplode(&time_exploded);
  const int kYearsToAdd = 10;

  for (auto year = time_exploded.year; year < time_exploded.year + kYearsToAdd;
       year++) {
    std::u16string yyyy = base::ASCIIToUTF16(base::NumberToString(year));
    years.push_back({yyyy, yyyy});
  }

  return years;
}

// Returns a vector of 10 consecutive years starting today in 2 digit format
// and an additional "YY" entry.
std::vector<SelectOption> Get2DigitYears() {
  std::vector<SelectOption> years = Get4DigitYears();
  for (SelectOption& option : years) {
    DCHECK_EQ(option.text.size(), 4u);
    DCHECK_EQ(option.value.size(), 4u);
    option.text = option.text.substr(2);
    option.value = option.value.substr(2);
  }
  return years;
}

// Adds prefixes and postfixes to options and labels.
std::vector<SelectOption> WithNoise(std::vector<SelectOption> options) {
  for (SelectOption& option : options) {
    option.text = base::StrCat({u"bla", option.text, u"123"});
    option.value = base::StrCat({u"bla", option.text, u"123"});
  }
  return options;
}

class CreditCardFieldParserTestBase : public FormFieldParserTestBase {
 public:
  CreditCardFieldParserTestBase() = default;
  CreditCardFieldParserTestBase(const CreditCardFieldParserTestBase&) = delete;
  CreditCardFieldParserTestBase& operator=(
      const CreditCardFieldParserTestBase&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return CreditCardFieldParser::Parse(context, scanner);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class CreditCardFieldParserTest : public CreditCardFieldParserTestBase,
                                  public ::testing::Test {
 public:
  CreditCardFieldParserTest() = default;
  CreditCardFieldParserTest(const CreditCardFieldParserTest&) = delete;
  CreditCardFieldParserTest& operator=(const CreditCardFieldParserTest&) =
      delete;
};

TEST_F(CreditCardFieldParserTest, Empty) {
  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_F(CreditCardFieldParserTest, NonParse) {
  AddTextFormFieldData("", "", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_F(CreditCardFieldParserTest, ParseCreditCardNoNumber) {
  AddTextFormFieldData("ccmonth", "Exp Month", UNKNOWN_TYPE);
  AddTextFormFieldData("ccyear", "Exp Year", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_F(CreditCardFieldParserTest, ParseCreditCardNoDate) {
  AddTextFormFieldData("card_number", "Card Number", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_F(CreditCardFieldParserTest, ParseMiniumCreditCard) {
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Ensure that a placeholder hint for a 2-digit year is respected
TEST_F(CreditCardFieldParserTest, ParseMiniumCreditCardWith2DigitYearHint) {
  base::test::ScopedFeatureList scoped_features{
      features::kAutofillEnableExpirationDateImprovements};
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_2_DIGIT_YEAR);
  fields_.back()->set_placeholder(u"YY");
  ClassifyAndVerify(ParseResult::kParsed);
}

// Ensure that a max-length can trump an incorrect 4-digit placeholder hint.
TEST_F(CreditCardFieldParserTest, ParseMiniumCreditCardWithMaxLength) {
  base::test::ScopedFeatureList scoped_features{
      features::kAutofillEnableExpirationDateImprovements};
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_2_DIGIT_YEAR);
  // Even though the placehodler indicates YYYY, the max-length only enables
  // a YY expiration format.
  fields_.back()->set_max_length(2u);
  fields_.back()->set_placeholder(u"YYYY");
  ClassifyAndVerify(ParseResult::kParsed);
}

struct CreditCardFieldYearTestCase {
  bool with_noise;
  FieldType expected_type;
};

class CreditCardFieldYearTest
    : public CreditCardFieldParserTestBase,
      public testing::TestWithParam<
          std::tuple<CreditCardFieldYearTestCase, bool>> {
 public:
  CreditCardFieldYearTest() = default;

  bool with_noise() const { return std::get<0>(GetParam()).with_noise; }

  bool ShouldSwapMonthAndYear() const { return std::get<1>(GetParam()); }

  FieldType expected_type() const {
    return std::get<0>(GetParam()).expected_type;
  }

  std::vector<SelectOption> MakeOptionVector() const {
    std::vector<SelectOption> options;
    if (expected_type() == CREDIT_CARD_EXP_2_DIGIT_YEAR) {
      options = Get2DigitYears();
    } else {
      options = Get4DigitYears();
    }
    if (with_noise()) {
      options = WithNoise(options);
    }
    return options;
  }
};

TEST_P(CreditCardFieldYearTest, ParseMinimumCreditCardWithExpiryDateOptions) {
  base::test::ScopedFeatureList scoped_features{
      features::kAutofillEnableExpirationDateImprovements};
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddSelectOneFormFieldData("Random Label", "Random Label", GetMonths(),
                            CREDIT_CARD_EXP_MONTH);
  AddSelectOneFormFieldData("Random Label", "Random Label", MakeOptionVector(),
                            expected_type());

  if (ShouldSwapMonthAndYear())
    std::swap(fields_[1], fields_[2]);

  ClassifyAndVerify(ParseResult::kParsed);
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardFieldParserTest,
    CreditCardFieldYearTest,
    testing::Combine(
        testing::Values(
            CreditCardFieldYearTestCase{false, CREDIT_CARD_EXP_2_DIGIT_YEAR},
            CreditCardFieldYearTestCase{false, CREDIT_CARD_EXP_4_DIGIT_YEAR},
            CreditCardFieldYearTestCase{true, CREDIT_CARD_EXP_2_DIGIT_YEAR},
            CreditCardFieldYearTestCase{true, CREDIT_CARD_EXP_4_DIGIT_YEAR}),
        testing::Bool()));

TEST_F(CreditCardFieldParserTest, ParseFullCreditCard) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  AddTextFormFieldData("verification", "Verification",
                       CREDIT_CARD_VERIFICATION_CODE);
  AddSelectOneFormFieldData("Card Type", "card_type", {{u"visa", u"visa"}},
                            CREDIT_CARD_TYPE);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseExpMonthYear) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ExpDate", "ExpDate Month / Year",
                       CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ExpDate", "ExpDate Month / Year",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseExpMonthYear2) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ExpDate", "Expiration date Month / Year",
                       CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ExpDate", "Expiration date Month / Year",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseGiftCard) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("gift.certificate", "Gift certificate", UNKNOWN_TYPE);
  AddTextFormFieldData("gift-card", "Gift card", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::kParsed);
}

struct ParseExpFieldTestCase {
  const FormControlType cc_fields_form_control_type;
  const std::string label;
  const int max_length;
  const FieldType expected_prediction;
};

class ParseExpFieldTest : public CreditCardFieldParserTestBase,
                          public testing::TestWithParam<ParseExpFieldTestCase> {
 public:
  ParseExpFieldTest() = default;

  const ParseExpFieldTestCase& test_case() const { return GetParam(); }
};

TEST_P(ParseExpFieldTest, ParseExpField) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddFormFieldData(test_case().cc_fields_form_control_type, "card_number",
                   "Card Number", CREDIT_CARD_NUMBER);
  AddFormFieldData(test_case().cc_fields_form_control_type, "cc_exp",
                   test_case().label, /*placeholder=*/"",
                   test_case().max_length, test_case().expected_prediction);

  // Assists in identifying which case has failed.
  SCOPED_TRACE(test_case().expected_prediction);
  SCOPED_TRACE(test_case().max_length);
  SCOPED_TRACE(test_case().label);

  if (test_case().expected_prediction == UNKNOWN_TYPE) {
    // Expect failure and continue to next test case.
    // The expiry date is a required field for credit card forms, and thus the
    // parse sets |field_| to nullptr.
    ClassifyAndVerify(ParseResult::kNotParsed);
    return;
  }

  ClassifyAndVerify(ParseResult::kParsed);
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardFieldParserTest,
    ParseExpFieldTest,
    testing::Values(
        // CC fields input_type="text"
        // General label, no maxlength.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date", 0,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // General label, maxlength 4.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date", 4,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // General label, maxlength 5.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date", 5,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // General label, maxlength 6.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date", 6,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // General label, maxlength 7.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date", 7,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // General label, large maxlength.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date",
                              12, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

        // Unsupported maxlength, general label.
        ParseExpFieldTestCase{FormControlType::kInputText, "Expiration Date", 3,
                              UNKNOWN_TYPE},
        // Unsupported maxlength, two digit year label.
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YY)", 3, UNKNOWN_TYPE},
        // Unsupported maxlength, four digit year label.
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YYYY)", 3, UNKNOWN_TYPE},

        // Two digit year, simple label.
        ParseExpFieldTestCase{FormControlType::kInputText, "MM / YY", 0,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, with slash (MM/YY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YY)", 0,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, no slash (MMYY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MMYY)", 4,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, with slash and maxlength (MM/YY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YY)", 5,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, with slash and large maxlength (MM/YY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YY)", 12,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},

        // Four digit year, simple label.
        ParseExpFieldTestCase{FormControlType::kInputText, "MM / YYYY", 0,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, with slash (MM/YYYY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YYYY)", 0,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, no slash (MMYYYY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MMYYYY)", 6,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, with slash and maxlength (MM/YYYY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YYYY)", 7,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, with slash and large maxlength (MM/YYYY).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YYYY)", 12,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

        // Four digit year label with restrictive maxlength (4).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YYYY)", 4,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Four digit year label with restrictive maxlength (5).
        ParseExpFieldTestCase{FormControlType::kInputText,
                              "Expiration Date (MM/YYYY)", 5,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},

        // CC fields input_type="number"
        // General label, no maxlength.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              0, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // General label, maxlength 4.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              4, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // General label, maxlength 5.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              5, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // General label, maxlength 6.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              6, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // General label, maxlength 7.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              7, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // General label, large maxlength.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              12, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

        // Unsupported maxlength, general label.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "Expiration Date",
                              3, UNKNOWN_TYPE},
        // Unsupported maxlength, two digit year label.
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YY)", 3, UNKNOWN_TYPE},
        // Unsupported maxlength, four digit year label.
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YYYY)", 3, UNKNOWN_TYPE},

        // Two digit year, simple label.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "MM / YY", 0,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, with slash (MM/YY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YY)", 0,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, no slash (MMYY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MMYY)", 4,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, with slash and maxlength (MM/YY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YY)", 5,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Two digit year, with slash and large maxlength (MM/YY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YY)", 12,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},

        // Four digit year, simple label.
        ParseExpFieldTestCase{FormControlType::kInputNumber, "MM / YYYY", 0,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, with slash (MM/YYYY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YYYY)", 0,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, no slash (MMYYYY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MMYYYY)", 6,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, with slash and maxlength (MM/YYYY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YYYY)", 7,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // Four digit year, with slash and large maxlength (MM/YYYY).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YYYY)", 12,
                              CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

        // Four digit year label with restrictive maxlength (4).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YYYY)", 4,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // Four digit year label with restrictive maxlength (5).
        ParseExpFieldTestCase{FormControlType::kInputNumber,
                              "Expiration Date (MM/YYYY)", 5,
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}));

TEST_F(CreditCardFieldParserTest, ParseCreditCardHolderNameWithCCFullName) {
  AddTextFormFieldData("ccfullname", "Name", CREDIT_CARD_NAME_FULL);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Verifies that <input type="month"> controls are able to be parsed correctly.
TEST_F(CreditCardFieldParserTest, ParseMonthControl) {
  AddTextFormFieldData("ccnumber", "Card number:", CREDIT_CARD_NUMBER);
  AddFormFieldData(FormControlType::kInputMonth, "ccexp",
                   "Expiration date:", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

// Verify that heuristics <input name="ccyear" maxlength="2"/> considers
// *maxlength* attribute while parsing 2 Digit expiration year.
TEST_F(CreditCardFieldParserTest, ParseCreditCardExpYear_2DigitMaxLength) {
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Expiration Date", CREDIT_CARD_EXP_MONTH);
  AddFormFieldData(FormControlType::kInputText, "ccyear", "Expiration Date",
                   /*placeholder=*/"", 2, CREDIT_CARD_EXP_2_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseMultipleCreditCardNumbers) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("confirm_card_number", "Confirm Card Number",
                       CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseFirstAndLastNames) {
  AddTextFormFieldData("cc-fname", "First Name on Card",
                       CREDIT_CARD_NAME_FIRST);
  AddTextFormFieldData("cc-lname", "Last Name", CREDIT_CARD_NAME_LAST);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseConsecutiveCvc) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  AddTextFormFieldData("verification", "Verification",
                       CREDIT_CARD_VERIFICATION_CODE);
  AddTextFormFieldData("verification", "Verification",
                       CREDIT_CARD_VERIFICATION_CODE);

  ClassifyAndVerifyWithMultipleParses();
}

TEST_F(CreditCardFieldParserTest, ParseNonConsecutiveCvc) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  AddTextFormFieldData("verification", "Verification",
                       CREDIT_CARD_VERIFICATION_CODE);
  AddTextFormFieldData("unknown", "Unknown", UNKNOWN_TYPE);

  ClassifyAndVerifyWithMultipleParses();
}

TEST_F(CreditCardFieldParserTest, ParseCreditCardContextualNameNotCard) {
  AddTextFormFieldData("accNum", "Account ID", UNKNOWN_TYPE);
  AddTextFormFieldData("name", "Account Name", UNKNOWN_TYPE);
  AddTextFormFieldData("toAcctNum", "Move to Account ID", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::kNotParsed);
}

TEST_F(CreditCardFieldParserTest,
       ParseCreditCardContextualNameNotCardAcctMatch) {
  // TODO(crbug.com/40743092): This should be not parseable, but waiting before
  // changing kNameOnCardRe to use word boundaries.
  AddTextFormFieldData("acctNum", "Account ID", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("acctName", "Account Name", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("toAcctNum", "Move to Account ID", CREDIT_CARD_NUMBER);
  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest, ParseCreditCardContextualNameWithExpiration) {
  AddTextFormFieldData("acctNum", "Account ID", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("name", "Account Name", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::kParsed);
}

TEST_F(CreditCardFieldParserTest,
       ParseCreditCardContextualNameWithVerification) {
  AddTextFormFieldData("acctNum", "Account ID", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("name", "Account Name", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("cvv", "Verification", CREDIT_CARD_VERIFICATION_CODE);
  ClassifyAndVerify(ParseResult::kParsed);
}

struct DetermineExpirationDateFormatTestCase {
  const std::string expected_separator;
  const uint8_t expected_year_length;
  const std::string label;
  const int max_length;
  FieldType server_type_hint = NO_SERVER_DATA;
  bool is_server_override = false;
};

class DetermineExpirationDateFormat
    : public testing::TestWithParam<DetermineExpirationDateFormatTestCase> {
 public:
  DetermineExpirationDateFormat() {
    scoped_features_.InitAndEnableFeature(
        features::kAutofillEnableExpirationDateImprovements);
  }
  const DetermineExpirationDateFormatTestCase& test_case() const {
    return GetParam();
  }

 protected:
  base::test::ScopedFeatureList scoped_features_;
};

INSTANTIATE_TEST_SUITE_P(
    CreditCardFieldParserTest,
    DetermineExpirationDateFormat,
    testing::Values(
        // The order of parameters is:
        // label, max length, expected separator, expected digits in year:
        //
        // No label, no maxlength. -> "MM/YYYY"
        DetermineExpirationDateFormatTestCase{"/", 4, "", 0},
        // No label, maxlength 4. -> "MMYY"
        DetermineExpirationDateFormatTestCase{"", 2, "", 4},
        // No label, maxlength 5. -> "MM/YY"
        DetermineExpirationDateFormatTestCase{"/", 2, "", 5},
        // No label, maxlength 6. -> "MMYYYY"
        DetermineExpirationDateFormatTestCase{"", 4, "", 6},
        // No label, maxlength 7. -> "MM/YYYY"
        DetermineExpirationDateFormatTestCase{"/", 4, "", 7},
        // No label, large maxlength. -> "MM/YYYY"
        DetermineExpirationDateFormatTestCase{"/", 4, "", 12},

        // Unsupported maxlength, general label.
        DetermineExpirationDateFormatTestCase{"", 2, "", 3},
        // Unsupported maxlength, two digit year label.
        DetermineExpirationDateFormatTestCase{"", 2, "MM/YY", 3},
        // Unsupported maxlength, four digit year label.
        DetermineExpirationDateFormatTestCase{"", 2, "MM/YYYY", 3},

        // Two digit year, simple label.
        DetermineExpirationDateFormatTestCase{" / ", 2, "MM / YY", 0},
        // Two digit year, with slash (MM/YY).
        DetermineExpirationDateFormatTestCase{"/", 2, "(MM/YY)", 0},
        // Two digit year, no slash (MMYY).
        DetermineExpirationDateFormatTestCase{"", 2, "(MMYY)", 4},
        // Two digit year, with slash and maxlength (MM/YY).
        DetermineExpirationDateFormatTestCase{"/", 2, "(MM/YY)", 5},
        // Two digit year, with slash and large maxlength (MM/YY).
        DetermineExpirationDateFormatTestCase{"/", 2, "(MM/YY)", 12},

        // Four digit year, simple label.
        DetermineExpirationDateFormatTestCase{" / ", 4, "MM / YYYY", 0},
        // Four digit year, with slash (MM/YYYY).
        DetermineExpirationDateFormatTestCase{"/", 4, "(MM/YYYY)", 0},
        // Four digit year, no slash (MMYYYY).
        DetermineExpirationDateFormatTestCase{"", 4, "(MMYYYY)", 6},
        // Four digit year, with slash and maxlength (MM/YYYY).
        DetermineExpirationDateFormatTestCase{"/", 4, "(MM/YYYY)", 7},
        // Four digit year, with slash and large maxlength (MM/YYYY).
        DetermineExpirationDateFormatTestCase{"/", 4, "(MM/YYYY)", 12},

        // Four digit year label with restrictive maxlength (4).
        DetermineExpirationDateFormatTestCase{"", 2, "(MM/YYYY)", 4},
        // Four digit year label with restrictive maxlength (5).
        DetermineExpirationDateFormatTestCase{"/", 2, "(MM/YYYY)", 5},

        // Spanish format.
        DetermineExpirationDateFormatTestCase{" / ", 2, "MM / AA", 0},
        DetermineExpirationDateFormatTestCase{" / ", 4, "MM / AAAA", 0},

        // Different separator.
        DetermineExpirationDateFormatTestCase{" - ", 2, "MM - YY", 0},

        // Date fits after stripping whitespaces from separator.
        DetermineExpirationDateFormatTestCase{"-", 2, "MM - YY", 5},

        // Verify that server hints are getting priority over max_length
        // but not over the pattern.
        //
        // Due to the MM / YY pattern, the 2 digit expiration date is chosen.
        DetermineExpirationDateFormatTestCase{
            " / ", 2, "MM / YY", 0, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        DetermineExpirationDateFormatTestCase{
            " / ", 2, "MM / YY", 7, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // If no pattern and max length are given, the server hint wins.
        DetermineExpirationDateFormatTestCase{
            "/", 4, "", 0, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        DetermineExpirationDateFormatTestCase{
            "/", 2, "", 0, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
        // The max-length may require a pruning of the separator.
        DetermineExpirationDateFormatTestCase{
            "/", 4, "", 7, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        DetermineExpirationDateFormatTestCase{
            "", 4, "", 6, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
        // But at some point we ignore the server if the type does not fit:
        DetermineExpirationDateFormatTestCase{
            "/", 2, "", 5, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

        // Verify that server overrides are prioritized over everything else.
        DetermineExpirationDateFormatTestCase{
            " / ", 4, "MM / YY", 0, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true},
        // The max-length may require a pruning of the separator.
        DetermineExpirationDateFormatTestCase{
            "/", 4, "MM / YY", 7, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true},
        DetermineExpirationDateFormatTestCase{
            "", 4, "MM / YY", 6, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true},
        // But at some point we ignore the server if the type does not fit:
        DetermineExpirationDateFormatTestCase{
            "/", 2, "MM / YY", 5, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true}));

TEST_P(DetermineExpirationDateFormat, TestDetermineFormat) {
  // Assists in identifying which case has failed.
  SCOPED_TRACE(test_case().expected_separator);
  SCOPED_TRACE(test_case().expected_year_length);
  SCOPED_TRACE(test_case().label);
  SCOPED_TRACE(test_case().max_length);
  SCOPED_TRACE(test_case().server_type_hint);
  SCOPED_TRACE(test_case().is_server_override);

  AutofillField field;
  field.set_max_length(test_case().max_length);
  field.set_label(base::UTF8ToUTF16(test_case().label));

  FieldType fallback_type = CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;

  CreditCardFieldParser::ExpirationDateFormat result =
      CreditCardFieldParser::DetermineExpirationDateFormat(
          field, fallback_type, test_case().server_type_hint,
          test_case().is_server_override ? test_case().server_type_hint
                                         : NO_SERVER_DATA);
  EXPECT_EQ(base::UTF8ToUTF16(test_case().expected_separator),
            result.separator);
  EXPECT_EQ(test_case().expected_year_length, result.digits_in_expiration_year);
}

}  // namespace
}  // namespace autofill
