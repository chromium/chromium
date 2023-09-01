// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/credit_card_field.h"

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
    DCHECK_EQ(option.content.size(), 4u);
    DCHECK_EQ(option.value.size(), 4u);
    option.content = option.content.substr(2);
    option.value = option.value.substr(2);
  }
  return years;
}

// Adds prefixes and postfixes to options and labels.
std::vector<SelectOption> WithNoise(std::vector<SelectOption> options) {
  for (SelectOption& option : options) {
    option.content = base::StrCat({u"bla", option.content, u"123"});
    option.value = base::StrCat({u"bla", option.content, u"123"});
  }
  return options;
}

class CreditCardFieldTestBase : public FormFieldTestBase {
 public:
  explicit CreditCardFieldTestBase(
      PatternProviderFeatureState pattern_provider_feature_state)
      : FormFieldTestBase(pattern_provider_feature_state) {}
  CreditCardFieldTestBase(const CreditCardFieldTestBase&) = delete;
  CreditCardFieldTestBase& operator=(const CreditCardFieldTestBase&) = delete;

 protected:
  std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language = LanguageCode("us")) override {
    return CreditCardField::Parse(scanner, page_language,
                                  *GetActivePatternSource(), nullptr);
  }

  // Runs multiple parsing attempts until the end of the form is reached.
  void ClassifyAndVerifyWithMultipleParses(
      const LanguageCode& page_language = LanguageCode("")) {
    AutofillScanner scanner(list_);
    while (!scanner.IsEnd()) {
      // An empty page_language means the language is unknown and patterns of
      // all languages are used.
      field_ = Parse(&scanner, page_language);
      if (field_ == nullptr) {
        scanner.Advance();
      } else {
        field_->AddClassificationsForTesting(field_candidates_map_);
      }
    }
    TestClassificationExpectations();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class CreditCardFieldTest
    : public CreditCardFieldTestBase,
      public ::testing::TestWithParam<PatternProviderFeatureState> {
 public:
  CreditCardFieldTest() : CreditCardFieldTestBase(GetParam()) {}
  CreditCardFieldTest(const CreditCardFieldTest&) = delete;
  CreditCardFieldTest& operator=(const CreditCardFieldTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(CreditCardFieldTest,
                         CreditCardFieldTest,
                         testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(CreditCardFieldTest, Empty) {
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(CreditCardFieldTest, NonParse) {
  AddTextFormFieldData("", "", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(CreditCardFieldTest, ParseCreditCardNoNumber) {
  AddTextFormFieldData("ccmonth", "Exp Month", UNKNOWN_TYPE);
  AddTextFormFieldData("ccyear", "Exp Year", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(CreditCardFieldTest, ParseCreditCardNoDate) {
  AddTextFormFieldData("card_number", "Card Number", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(CreditCardFieldTest, ParseMiniumCreditCard) {
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

struct CreditCardFieldYearTestCase {
  bool with_noise;
  ServerFieldType expected_type;
};

class CreditCardFieldYearTest
    : public CreditCardFieldTestBase,
      public testing::TestWithParam<std::tuple<PatternProviderFeatureState,
                                               CreditCardFieldYearTestCase,
                                               bool>> {
 public:
  CreditCardFieldYearTest()
      : CreditCardFieldTestBase(std::get<0>(GetParam())) {}

  bool with_noise() const { return std::get<1>(GetParam()).with_noise; }

  bool ShouldSwapMonthAndYear() const { return std::get<2>(GetParam()); }

  ServerFieldType expected_type() const {
    return std::get<1>(GetParam()).expected_type;
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
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddSelectOneFormFieldData("Random Label", "Random Label", GetMonths(),
                            CREDIT_CARD_EXP_MONTH);
  AddSelectOneFormFieldDataWithLength(
      "Random Label", "Random Label",
      expected_type() == CREDIT_CARD_EXP_2_DIGIT_YEAR ? 2 : 4,
      MakeOptionVector(), expected_type());

  if (ShouldSwapMonthAndYear())
    std::swap(list_[1], list_[2]);

  ClassifyAndVerify(ParseResult::PARSED);
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardFieldTest,
    CreditCardFieldYearTest,
    testing::Combine(
        testing::ValuesIn(PatternProviderFeatureState::All()),
        testing::Values(
            CreditCardFieldYearTestCase{false, CREDIT_CARD_EXP_2_DIGIT_YEAR},
            CreditCardFieldYearTestCase{false, CREDIT_CARD_EXP_4_DIGIT_YEAR},
            CreditCardFieldYearTestCase{true, CREDIT_CARD_EXP_2_DIGIT_YEAR},
            CreditCardFieldYearTestCase{true, CREDIT_CARD_EXP_4_DIGIT_YEAR}),
        testing::Bool()));

TEST_P(CreditCardFieldTest, ParseFullCreditCard) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  AddTextFormFieldData("verification", "Verification",
                       CREDIT_CARD_VERIFICATION_CODE);
  AddSelectOneFormFieldData("Card Type", "card_type", {{u"visa", u"visa"}},
                            CREDIT_CARD_TYPE);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseExpMonthYear) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ExpDate", "ExpDate Month / Year",
                       CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ExpDate", "ExpDate Month / Year",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseExpMonthYear2) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ExpDate", "Expiration date Month / Year",
                       CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ExpDate", "Expiration date Month / Year",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseGiftCard) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("gift.certificate", "Gift certificate", UNKNOWN_TYPE);
  AddTextFormFieldData("gift-card", "Gift card", UNKNOWN_TYPE);

  ClassifyAndVerify(ParseResult::PARSED);
}

struct ParseExpFieldTestCase {
  const std::string cc_fields_form_control_type;
  const std::string label;
  const int max_length;
  const ServerFieldType expected_prediction;
};

class ParseExpFieldTest
    : public CreditCardFieldTestBase,
      public testing::TestWithParam<
          std::tuple<PatternProviderFeatureState, ParseExpFieldTestCase>> {
 public:
  ParseExpFieldTest() : CreditCardFieldTestBase(std::get<0>(GetParam())) {}

  const ParseExpFieldTestCase& test_case() const {
    return std::get<1>(GetParam());
  }
};

TEST_P(ParseExpFieldTest, ParseExpField) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddFormFieldData(test_case().cc_fields_form_control_type, "card_number",
                   "Card Number", CREDIT_CARD_NUMBER);
  AddFormFieldDataWithLength(test_case().cc_fields_form_control_type, "cc_exp",
                             test_case().label, test_case().max_length,
                             test_case().expected_prediction);

  // Assists in identifying which case has failed.
  SCOPED_TRACE(test_case().expected_prediction);
  SCOPED_TRACE(test_case().max_length);
  SCOPED_TRACE(test_case().label);

  if (test_case().expected_prediction == UNKNOWN_TYPE) {
    // Expect failure and continue to next test case.
    // The expiry date is a required field for credit card forms, and thus the
    // parse sets |field_| to nullptr.
    ClassifyAndVerify(ParseResult::NOT_PARSED);
    return;
  }

  ClassifyAndVerify(ParseResult::PARSED);
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardFieldTest,
    ParseExpFieldTest,
    testing::Combine(
        testing::ValuesIn(PatternProviderFeatureState::All()),
        testing::Values(
            // CC fields input_type="text"
            // General label, no maxlength.
            ParseExpFieldTestCase{"text", "Expiration Date", 0,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // General label, maxlength 4.
            ParseExpFieldTestCase{"text", "Expiration Date", 4,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // General label, maxlength 5.
            ParseExpFieldTestCase{"text", "Expiration Date", 5,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // General label, maxlength 6.
            ParseExpFieldTestCase{"text", "Expiration Date", 6,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // General label, maxlength 7.
            ParseExpFieldTestCase{"text", "Expiration Date", 7,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // General label, large maxlength.
            ParseExpFieldTestCase{"text", "Expiration Date", 12,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

            // Unsupported maxlength, general label.
            ParseExpFieldTestCase{"text", "Expiration Date", 3, UNKNOWN_TYPE},
            // Unsupported maxlength, two digit year label.
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YY)", 3,
                                  UNKNOWN_TYPE},
            // Unsupported maxlength, four digit year label.
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YYYY)", 3,
                                  UNKNOWN_TYPE},

            // Two digit year, simple label.
            ParseExpFieldTestCase{"text", "MM / YY", 0,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, with slash (MM/YY).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YY)", 0,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, no slash (MMYY).
            ParseExpFieldTestCase{"text", "Expiration Date (MMYY)", 4,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, with slash and maxlength (MM/YY).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YY)", 5,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, with slash and large maxlength (MM/YY).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YY)", 12,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},

            // Four digit year, simple label.
            ParseExpFieldTestCase{"text", "MM / YYYY", 0,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, with slash (MM/YYYY).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YYYY)", 0,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, no slash (MMYYYY).
            ParseExpFieldTestCase{"text", "Expiration Date (MMYYYY)", 6,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, with slash and maxlength (MM/YYYY).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YYYY)", 7,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, with slash and large maxlength (MM/YYYY).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YYYY)", 12,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

            // Four digit year label with restrictive maxlength (4).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YYYY)", 4,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Four digit year label with restrictive maxlength (5).
            ParseExpFieldTestCase{"text", "Expiration Date (MM/YYYY)", 5,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},

            // CC fields input_type="number"
            // General label, no maxlength.
            ParseExpFieldTestCase{"number", "Expiration Date", 0,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // General label, maxlength 4.
            ParseExpFieldTestCase{"number", "Expiration Date", 4,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // General label, maxlength 5.
            ParseExpFieldTestCase{"number", "Expiration Date", 5,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // General label, maxlength 6.
            ParseExpFieldTestCase{"number", "Expiration Date", 6,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // General label, maxlength 7.
            ParseExpFieldTestCase{"number", "Expiration Date", 7,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // General label, large maxlength.
            ParseExpFieldTestCase{"number", "Expiration Date", 12,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

            // Unsupported maxlength, general label.
            ParseExpFieldTestCase{"number", "Expiration Date", 3, UNKNOWN_TYPE},
            // Unsupported maxlength, two digit year label.
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YY)", 3,
                                  UNKNOWN_TYPE},
            // Unsupported maxlength, four digit year label.
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YYYY)", 3,
                                  UNKNOWN_TYPE},

            // Two digit year, simple label.
            ParseExpFieldTestCase{"number", "MM / YY", 0,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, with slash (MM/YY).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YY)", 0,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, no slash (MMYY).
            ParseExpFieldTestCase{"number", "Expiration Date (MMYY)", 4,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, with slash and maxlength (MM/YY).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YY)", 5,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Two digit year, with slash and large maxlength (MM/YY).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YY)", 12,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},

            // Four digit year, simple label.
            ParseExpFieldTestCase{"number", "MM / YYYY", 0,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, with slash (MM/YYYY).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YYYY)", 0,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, no slash (MMYYYY).
            ParseExpFieldTestCase{"number", "Expiration Date (MMYYYY)", 6,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, with slash and maxlength (MM/YYYY).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YYYY)", 7,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Four digit year, with slash and large maxlength (MM/YYYY).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YYYY)", 12,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

            // Four digit year label with restrictive maxlength (4).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YYYY)", 4,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Four digit year label with restrictive maxlength (5).
            ParseExpFieldTestCase{"number", "Expiration Date (MM/YYYY)", 5,
                                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR})));

TEST_P(CreditCardFieldTest, ParseCreditCardHolderNameWithCCFullName) {
  AddTextFormFieldData("ccfullname", "Name", CREDIT_CARD_NAME_FULL);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Verifies that <input type="month"> controls are able to be parsed correctly.
TEST_P(CreditCardFieldTest, ParseMonthControl) {
  AddTextFormFieldData("ccnumber", "Card number:", CREDIT_CARD_NUMBER);
  AddFormFieldData("month", "ccexp",
                   "Expiration date:", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

// Verify that heuristics <input name="ccyear" maxlength="2"/> considers
// *maxlength* attribute while parsing 2 Digit expiration year.
TEST_P(CreditCardFieldTest, ParseCreditCardExpYear_2DigitMaxLength) {
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Expiration Date", CREDIT_CARD_EXP_MONTH);
  AddFormFieldDataWithLength("text", "ccyear", "Expiration Date", 2,
                             CREDIT_CARD_EXP_2_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseMultipleCreditCardNumbers) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("confirm_card_number", "Confirm Card Number",
                       CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseFirstAndLastNames) {
  AddTextFormFieldData("cc-fname", "First Name on Card",
                       CREDIT_CARD_NAME_FIRST);
  AddTextFormFieldData("cc-lname", "Last Name", CREDIT_CARD_NAME_LAST);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseConsecutiveCvc) {
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

TEST_P(CreditCardFieldTest, ParseNonConsecutiveCvc) {
  AddTextFormFieldData("name_on_card", "Name on Card", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("card_number", "Card Number", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  AddTextFormFieldData("verification", "Verification",
                       CREDIT_CARD_VERIFICATION_CODE);
  AddTextFormFieldData("unknown", "Unknown", UNKNOWN_TYPE);

  ClassifyAndVerifyWithMultipleParses();
}

TEST_P(CreditCardFieldTest, ParseCreditCardContextualNameNotCard) {
  AddTextFormFieldData("accNum", "Account ID", UNKNOWN_TYPE);
  AddTextFormFieldData("name", "Account Name", UNKNOWN_TYPE);
  AddTextFormFieldData("toAcctNum", "Move to Account ID", UNKNOWN_TYPE);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(CreditCardFieldTest, ParseCreditCardContextualNameNotCardAcctMatch) {
  // TODO(crbug.com/1167977): This should be not parseable, but waiting before
  // changing kNameOnCardRe to use word boundaries.
  AddTextFormFieldData("acctNum", "Account ID", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("acctName", "Account Name", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("toAcctNum", "Move to Account ID", CREDIT_CARD_NUMBER);
  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseCreditCardContextualNameWithExpiration) {
  AddTextFormFieldData("acctNum", "Account ID", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("name", "Account Name", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("ccmonth", "Exp Month", CREDIT_CARD_EXP_MONTH);
  AddTextFormFieldData("ccyear", "Exp Year", CREDIT_CARD_EXP_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(CreditCardFieldTest, ParseCreditCardContextualNameWithVerification) {
  AddTextFormFieldData("acctNum", "Account ID", CREDIT_CARD_NUMBER);
  AddTextFormFieldData("name", "Account Name", CREDIT_CARD_NAME_FULL);
  AddTextFormFieldData("cvv", "Verification", CREDIT_CARD_VERIFICATION_CODE);
  ClassifyAndVerify(ParseResult::PARSED);
}

struct DetermineExpirationDateFormatTestCase {
  const std::string expected_separator;
  const uint8_t expected_year_length;
  const std::string label;
  const int max_length;
  ServerFieldType server_type_hint = NO_SERVER_DATA;
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
    CreditCardFieldTest,
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
  field.max_length = test_case().max_length;
  field.label = base::UTF8ToUTF16(test_case().label);

  ServerFieldType fallback_type = CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;

  CreditCardField::ExpirationDateFormat result =
      CreditCardField::DetermineExpirationDateFormat(
          field, fallback_type, test_case().server_type_hint,
          test_case().is_server_override ? test_case().server_type_hint
                                         : NO_SERVER_DATA);
  EXPECT_EQ(base::UTF8ToUTF16(test_case().expected_separator),
            result.separator);
  EXPECT_EQ(test_case().expected_year_length, result.digits_in_expiration_year);
}

}  // namespace
}  // namespace autofill
