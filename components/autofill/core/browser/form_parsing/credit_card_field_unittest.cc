// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/credit_card_field.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class CreditCardFieldTestBase {
 public:
  CreditCardFieldTestBase() = default;
  CreditCardFieldTestBase(const CreditCardFieldTestBase&) = delete;
  CreditCardFieldTestBase& operator=(const CreditCardFieldTestBase&) = delete;

 protected:
  // Parses the contents of |list_| as a form, and stores the result into
  // |field_|.
  void Parse() {
    AutofillScanner scanner(list_);
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    std::unique_ptr<FormField> field =
        CreditCardField::Parse(&scanner, LanguageCode(""), nullptr);
    field_ = std::unique_ptr<CreditCardField>(
        static_cast<CreditCardField*>(field.release()));
  }

  void MultipleParses() {
    std::unique_ptr<FormField> field;

    AutofillScanner scanner(list_);
    while (!scanner.IsEnd()) {
      // An empty page_language means the language is unknown and patterns of
      // all languages are used.
      field = CreditCardField::Parse(&scanner, LanguageCode(""), nullptr);
      field_ = std::unique_ptr<CreditCardField>(
          static_cast<CreditCardField*>(field.release()));
      if (field_ == nullptr) {
        scanner.Advance();
      } else {
        AddClassifications();
      }
    }
  }

  // Associates fields with their corresponding types, based on the previous
  // call to Parse().
  void AddClassifications() {
    return field_->AddClassifications(&field_candidates_map_);
  }

  FieldRendererId MakeFieldRendererId() {
    return FieldRendererId(++id_counter_);
  }

  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<const CreditCardField> field_;
  FieldCandidatesMap field_candidates_map_;

 private:
  uint64_t id_counter_ = 0;
};

class CreditCardFieldTest : public CreditCardFieldTestBase,
                            public testing::Test {
 public:
  CreditCardFieldTest() = default;
  CreditCardFieldTest(const CreditCardFieldTest&) = delete;
  CreditCardFieldTest& operator=(const CreditCardFieldTest&) = delete;
};

TEST_F(CreditCardFieldTest, Empty) {
  Parse();
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(CreditCardFieldTest, NonParse) {
  list_.push_back(std::make_unique<AutofillField>());
  Parse();
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoNumber) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));

  Parse();
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoDate) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));

  Parse();
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(CreditCardFieldTest, ParseMiniumCreditCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year3 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(number1) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month2) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month2].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year3) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year3].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseMinimumCreditCardWithExpiryDateOptions) {
  FormFieldData cc_number_field;
  FormFieldData month_field;
  FormFieldData year_field;

  cc_number_field.form_control_type = "text";
  cc_number_field.label = ASCIIToUTF16("Card Number");
  cc_number_field.name = ASCIIToUTF16("card_number");
  cc_number_field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(cc_number_field));
  FieldRendererId number = list_.back()->unique_renderer_id;

  // For month field, set the label and name to something which won't match
  // any regex, so we can test matching of the options themselves.
  month_field.form_control_type = "select-one";
  month_field.label = ASCIIToUTF16("Random label");
  month_field.name = ASCIIToUTF16("Random name");
  const std::vector<std::string> kMonths{"MM", "01", "02", "03", "04",
                                         "05", "06", "07", "08", "09",
                                         "10", "11", "12"};
  for (auto month : kMonths) {
    month_field.option_contents.push_back(base::UTF8ToUTF16(month));
    month_field.option_values.push_back(base::UTF8ToUTF16(month));
  }
  month_field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(month_field));
  FieldRendererId month = list_.back()->unique_renderer_id;

  // For year, keep the label and name to something which doesn't match regex
  // so we can test matching of the options themselves.
  year_field.form_control_type = "select-one";
  year_field.label = ASCIIToUTF16("Random label");
  year_field.name = ASCIIToUTF16("Random name");
  year_field.max_length = 2;
  year_field.option_contents.push_back(base::ASCIIToUTF16("YY"));
  year_field.option_values.push_back(base::ASCIIToUTF16("YY"));

  const base::Time time_now = AutofillClock::Now();
  base::Time::Exploded time_exploded;
  time_now.UTCExplode(&time_exploded);
  const int kYearsToAdd = 10;

  for (auto year = time_exploded.year; year < time_exploded.year + kYearsToAdd;
       year++) {
    year_field.option_contents.push_back(
        base::NumberToString16(year).substr(2));
    year_field.option_values.push_back(base::NumberToString16(year).substr(2));
  }
  year_field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(year_field));
  FieldRendererId year = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(number) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_2_DIGIT_YEAR,
            field_candidates_map_[year].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseFullCreditCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId cvc = list_.back()->unique_renderer_id;

  field.form_control_type = "select-one";
  field.label = ASCIIToUTF16("Card Type");
  field.name = ASCIIToUTF16("card_type");
  field.option_contents.push_back(ASCIIToUTF16("visa"));
  field.option_values.push_back(ASCIIToUTF16("visa"));
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId type = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(type) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_TYPE, field_candidates_map_[type].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(name) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(cvc) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[cvc].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month3 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year4 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(name1) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number2) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number2].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month3) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month3].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year4) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year4].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month3 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year4 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(name1) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number2) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number2].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month3) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month3].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year4) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year4].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseGiftCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Gift certificate");
  field.name = ASCIIToUTF16("gift.certificate");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId giftcert = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Gift card");
  field.name = ASCIIToUTF16("gift-card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId giftcard = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(name) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(giftcert) ==
              field_candidates_map_.end());
  ASSERT_TRUE(field_candidates_map_.find(giftcard) ==
              field_candidates_map_.end());
}

typedef struct {
  const std::string cc_fields_form_control_type;
  const std::string label;
  const int max_length;
  const ServerFieldType expected_prediction;
} ParseExpFieldTestCase;

class ParseExpFieldTest : public CreditCardFieldTestBase,
                          public testing::TestWithParam<ParseExpFieldTestCase> {
};

TEST_P(ParseExpFieldTest, ParseExpField) {
  auto test_case = GetParam();
  // Clean up after previous test cases.
  list_.clear();
  field_.reset();
  field_candidates_map_.clear();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name1 = list_.back()->unique_renderer_id;

  field.form_control_type = test_case.cc_fields_form_control_type;
  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId num2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16(test_case.label);
  if (test_case.max_length != 0) {
    field.max_length = test_case.max_length;
  }
  field.name = ASCIIToUTF16("cc_exp");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId exp3 = list_.back()->unique_renderer_id;

  Parse();

  // Assists in identifing which case has failed.
  SCOPED_TRACE(test_case.expected_prediction);
  SCOPED_TRACE(test_case.max_length);
  SCOPED_TRACE(test_case.label);

  if (test_case.expected_prediction == UNKNOWN_TYPE) {
    // Expect failure and continue to next test case.
    // The expiry date is a required field for credit card forms, and thus the
    // parse sets |field_| to nullptr.
    EXPECT_EQ(nullptr, field_.get());
    return;
  }

  // Ensure that the form was determined as valid.
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(name1) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name1].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(num2) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[num2].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(exp3) != field_candidates_map_.end());
  EXPECT_EQ(test_case.expected_prediction,
            field_candidates_map_[exp3].BestHeuristicType());
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardFieldTest,
    ParseExpFieldTest,
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
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}));

TEST_F(CreditCardFieldTest, ParseCreditCardHolderNameWithCCFullName) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("ccfullname");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name1 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(name1) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name1].BestHeuristicType());
}

// Verifies that <input type="month"> controls are able to be parsed correctly.
TEST_F(CreditCardFieldTest, ParseMonthControl) {
  FormFieldData field;

  field.form_control_type = "text";
  field.label = ASCIIToUTF16("Card number:");
  field.name = ASCIIToUTF16("ccnumber");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number1 = list_.back()->unique_renderer_id;

  field.form_control_type = "month";
  field.label = ASCIIToUTF16("Expiration date:");
  field.name = ASCIIToUTF16("ccexp");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId date2 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(number1) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(date2) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            field_candidates_map_[date2].BestHeuristicType());
}

// Verify that heuristics <input name="ccyear" maxlength="2"/> considers
// *maxlength* attribute while parsing 2 Digit expiration year.
TEST_F(CreditCardFieldTest, ParseCreditCardExpYear_2DigitMaxLength) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month = list_.back()->unique_renderer_id;

  field.name = ASCIIToUTF16("ccyear");
  field.max_length = 2;
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(number) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_2_DIGIT_YEAR,
            field_candidates_map_[year].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNumberWithSplit) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q1");
  field.max_length = 4;
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q2");
  field.max_length = 4;
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q3");
  field.max_length = 4;
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number3 = list_.back()->unique_renderer_id;

  // For last credit card number input field it simply ignores the |max_length|
  // attribute. So even having a very big number, does not conside it an invalid
  // split for autofilling.
  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q4");
  field.max_length = 20;
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number4 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month5 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year6 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();

  ASSERT_TRUE(field_candidates_map_.find(number1) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number1].BestHeuristicType());
  EXPECT_EQ(0U, list_[0]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(number2) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number2].BestHeuristicType());
  EXPECT_EQ(4U, list_[1]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(number3) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number3].BestHeuristicType());
  EXPECT_EQ(8U, list_[2]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(number4) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number4].BestHeuristicType());
  EXPECT_EQ(12U, list_[3]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(month5) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month5].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year6) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year6].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseMultipleCreditCardNumbers) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Confirm Card Number");
  field.name = ASCIIToUTF16("confirm_card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number3 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month4 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year5 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();

  ASSERT_TRUE(field_candidates_map_.find(name1) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number2) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number2].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number3) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number3].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month4) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month4].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year5) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year5].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseFirstAndLastNames) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name on Card");
  field.name = ASCIIToUTF16("cc-fname");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name1 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("cc-lname");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name2 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number3 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month4 = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year5 = list_.back()->unique_renderer_id;

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();

  ASSERT_TRUE(field_candidates_map_.find(name1) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST,
            field_candidates_map_[name1].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(name2) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_LAST,
            field_candidates_map_[name2].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number3) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number3].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month4) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month4].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year5) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year5].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseConsecutiveCvc) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId cvc = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId cvc2 = list_.back()->unique_renderer_id;

  MultipleParses();

  ASSERT_TRUE(field_candidates_map_.find(name) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(cvc) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[cvc].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(cvc2) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[cvc2].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseNonConsecutiveCvc) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId name = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId number = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId month = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId year = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId cvc = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Unknown");
  field.name = ASCIIToUTF16("unknown");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId unknown = list_.back()->unique_renderer_id;

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  field.unique_renderer_id = MakeFieldRendererId();
  list_.push_back(std::make_unique<AutofillField>(field));
  FieldRendererId cvc2 = list_.back()->unique_renderer_id;

  MultipleParses();

  ASSERT_TRUE(field_candidates_map_.find(name) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[name].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(number) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[number].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(month) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[month].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(year) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[year].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(cvc) != field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[cvc].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(unknown) ==
              field_candidates_map_.end());
  ASSERT_TRUE(field_candidates_map_.find(cvc2) == field_candidates_map_.end());
}

}  // namespace autofill
