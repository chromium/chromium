// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/credit_card_field.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

class CreditCardFieldTestBase {
 public:
  CreditCardFieldTestBase() {}
  ~CreditCardFieldTestBase() {}

 protected:
  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<const CreditCardField> field_;
  FieldCandidatesMap field_candidates_map_;

  // Parses the contents of |list_| as a form, and stores the result into
  // |field_|.
  void Parse() {
    AutofillScanner scanner(list_);
    std::unique_ptr<FormField> field =
        CreditCardField::Parse(&scanner, nullptr);
    field_ = std::unique_ptr<CreditCardField>(
        static_cast<CreditCardField*>(field.release()));
  }

  void MultipleParses() {
    std::unique_ptr<FormField> field;

    AutofillScanner scanner(list_);
    while (!scanner.IsEnd()) {
      field = CreditCardField::Parse(&scanner, nullptr);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardFieldTestBase);
};

class CreditCardFieldTest : public CreditCardFieldTestBase,
                            public testing::Test {
 public:
  CreditCardFieldTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardFieldTest);
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
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month1")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year2")));

  Parse();
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoDate) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number1")));

  Parse();
  ASSERT_EQ(nullptr, field_.get());
}

TEST_F(CreditCardFieldTest, ParseMiniumCreditCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number1")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month2")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year3")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year3")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseFullCreditCard) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("name")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("year")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("cvc")));

  field.form_control_type = "select-one";
  field.label = ASCIIToUTF16("Card Type");
  field.name = ASCIIToUTF16("card_type");
  field.option_contents.push_back(ASCIIToUTF16("visa"));
  field.option_values.push_back(ASCIIToUTF16("visa"));
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("type")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("type")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_TYPE,
            field_candidates_map_[ASCIIToUTF16("type")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("cvc")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[ASCIIToUTF16("cvc")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year4")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month3")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year4")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year4")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear2) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year4")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month3")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year4")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year4")].BestHeuristicType());
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
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.form_control_type = test_case.cc_fields_form_control_type;
  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("num2")));

  field.label = ASCIIToUTF16(test_case.label);
  if (test_case.max_length != 0) {
    field.max_length = test_case.max_length;
  }
  field.name = ASCIIToUTF16("cc_exp");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("exp3")));

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
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("num2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("num2")].BestHeuristicType());

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("exp3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(test_case.expected_prediction,
            field_candidates_map_[ASCIIToUTF16("exp3")].BestHeuristicType());
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
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
}

// Verifies that <input type="month"> controls are able to be parsed correctly.
TEST_F(CreditCardFieldTest, ParseMonthControl) {
  FormFieldData field;

  field.form_control_type = "text";
  field.label = ASCIIToUTF16("Card number:");
  field.name = ASCIIToUTF16("ccnumber");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number1")));

  field.form_control_type = "month";
  field.label = ASCIIToUTF16("Expiration date:");
  field.name = ASCIIToUTF16("ccexp");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("date2")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("date2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("date2")].BestHeuristicType());
}

// Verify that heuristics <input name="ccyear" maxlength="2"/> considers
// *maxlength* attribute while parsing 2 Digit expiration year.
TEST_F(CreditCardFieldTest, ParseCreditCardExpYear_2DigitMaxLength) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number")));

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month")));

  field.name = ASCIIToUTF16("ccyear");
  field.max_length = 2;
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("year")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_2_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNumberWithSplit) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q1");
  field.max_length = 4;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q2");
  field.max_length = 4;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q3");
  field.max_length = 4;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number3")));

  // For last credit card number input field it simply ignores the |max_length|
  // attribute. So even having a very big number, does not conside it an invalid
  // split for autofilling.
  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number_q4");
  field.max_length = 20;
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number4")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month5")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year6")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number1")].BestHeuristicType());
  EXPECT_EQ(0U, list_[0]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number2")].BestHeuristicType());
  EXPECT_EQ(4U, list_[1]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number3")].BestHeuristicType());
  EXPECT_EQ(8U, list_[2]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number4")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number4")].BestHeuristicType());
  EXPECT_EQ(12U, list_[3]->credit_card_number_offset());

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month5")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month5")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year6")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year6")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseMultipleCreditCardNumbers) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Confirm Card Number");
  field.name = ASCIIToUTF16("confirm_card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number3")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month4")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year5")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number3")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month4")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month4")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year5")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year5")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseFirstAndLastNames) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name on Card");
  field.name = ASCIIToUTF16("cc-fname");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("cc-lname");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("name2")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number3")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month4")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("year5")));

  Parse();
  ASSERT_NE(nullptr, field_.get());
  AddClassifications();

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name1")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST,
            field_candidates_map_[ASCIIToUTF16("name1")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_LAST,
            field_candidates_map_[ASCIIToUTF16("name2")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number3")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number3")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month4")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month4")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year5")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year5")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseConsecutiveCvc) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("name")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("year")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("cvc")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("cvc2")));

  MultipleParses();

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("cvc")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[ASCIIToUTF16("cvc")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("cvc2")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[ASCIIToUTF16("cvc2")].BestHeuristicType());
}

TEST_F(CreditCardFieldTest, ParseNonConsecutiveCvc) {
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("name")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("number")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("month")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("year")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("cvc")));

  field.label = ASCIIToUTF16("Unknown");
  field.name = ASCIIToUTF16("unknown");
  list_.push_back(
      std::make_unique<AutofillField>(field, ASCIIToUTF16("unknown")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(std::make_unique<AutofillField>(field, ASCIIToUTF16("cvc2")));

  MultipleParses();

  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("name")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            field_candidates_map_[ASCIIToUTF16("name")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("number")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            field_candidates_map_[ASCIIToUTF16("number")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("month")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            field_candidates_map_[ASCIIToUTF16("month")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("year")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            field_candidates_map_[ASCIIToUTF16("year")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("cvc")) !=
              field_candidates_map_.end());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            field_candidates_map_[ASCIIToUTF16("cvc")].BestHeuristicType());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("unknown")) ==
              field_candidates_map_.end());
  ASSERT_TRUE(field_candidates_map_.find(ASCIIToUTF16("cvc2")) ==
              field_candidates_map_.end());
}

}  // namespace autofill
