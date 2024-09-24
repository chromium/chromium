// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filling_payments_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field_parser.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const std::u16string kMidlineEllipsis2Dots =
    CreditCard::GetMidlineEllipsisDots(2);
const std::u16string kMidlineEllipsis3Dots =
    CreditCard::GetMidlineEllipsisDots(3);
const std::u16string kMidlineEllipsis4Dots =
    CreditCard::GetMidlineEllipsisDots(4);

// The following strings do not contain space padding and are more appropriate
// for input fields which have a strict `max_length` set.
const std::u16string kMidlineEllipsis2DotsWithoutPadding =
    CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/2);
const std::u16string kMidlineEllipsis3DotsWithoutPadding =
    CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/3);
const std::u16string kMidlineEllipsis4DotsWithoutPadding =
    CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/4);

constexpr char kAppLocale[] = "en-US";

const std::vector<const char*> NotNumericMonthsContentsNoPlaceholder() {
  const std::vector<const char*> result = {"Jan", "Feb", "Mar", "Apr",
                                           "May", "Jun", "Jul", "Aug",
                                           "Sep", "Oct", "Nov", "Dec"};
  return result;
}

const std::vector<const char*> NotNumericMonthsContentsWithPlaceholder() {
  const std::vector<const char*> result = {"Select a Month",
                                           "Jan",
                                           "Feb",
                                           "Mar",
                                           "Apr",
                                           "May",
                                           "Jun",
                                           "Jul",
                                           "Aug",
                                           "Sep",
                                           "Oct",
                                           "Nov",
                                           "Dec"};
  return result;
}

AutofillField CreateTestSelectAutofillField(
    const std::vector<const char*>& values,
    FieldType heuristic_type) {
  AutofillField field{test::CreateTestSelectField(values)};
  field.set_heuristic_type(GetActiveHeuristicSource(), heuristic_type);
  return field;
}

size_t GetIndexOfValue(const std::vector<SelectOption>& values,
                       const std::u16string& value) {
  size_t i =
      base::ranges::find(values, value, &SelectOption::value) - values.begin();
  CHECK_LT(i, values.size()) << "Passing invalid arguments to GetIndexOfValue";
  return i;
}

// Creates the select field from the specified |values| and |contents| and tests
// filling with 3 different values.
void TestFillingExpirationMonth(const std::vector<const char*>& values,
                                const std::vector<const char*>& contents) {
  AutofillField field{
      test::CreateTestSelectField("", "", "", values, contents)};
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_EXP_MONTH);

  size_t content_index = 0;
  // Try a single-digit month.
  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(3);
  std::u16string value_to_fill =
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field);

  ASSERT_FALSE(value_to_fill.empty());
  content_index = GetIndexOfValue(field.options(), value_to_fill);
  EXPECT_EQ(u"Mar", field.options()[content_index].text);

  // Try a two-digit month.
  credit_card.SetExpirationMonth(11);
  value_to_fill =
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field);

  ASSERT_FALSE(value_to_fill.empty());
  content_index = GetIndexOfValue(field.options(), value_to_fill);
  EXPECT_EQ(u"Nov", field.options()[content_index].text);
}

struct CreditCardTestCase {
  std::u16string card_number_;
  size_t total_splits_;
  std::vector<int> splits_;
  std::vector<std::u16string> expected_results_;
};

// Returns the offset to be set within the credit card number field.
size_t GetNumberOffset(size_t index, const CreditCardTestCase& test) {
  size_t result = 0;
  for (size_t i = 0; i < index; ++i) {
    result += test.splits_[i];
  }
  return result;
}

class FieldFillingPaymentsUtilTest : public testing::Test {
 public:
  FieldFillingPaymentsUtilTest() = default;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableCvcStorageAndFilling};
};

// Verify that credit card related fields with the autocomplete attribute
// set to off get filled.
TEST_F(FieldFillingPaymentsUtilTest,
       FillFormField_AutocompleteOff_CreditCardField) {
  AutofillField field;
  field.set_should_autocomplete(false);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"4111111111111111");
  EXPECT_EQ(u"4111111111111111", GetFillingValueForCreditCard(
                                     credit_card, /*cvc=*/u"", kAppLocale,
                                     mojom::ActionPersistence::kFill, field));
}

// Verify that the correct value is returned if the maximum length of the credit
// card value exceeds the actual length.
TEST_F(FieldFillingPaymentsUtilTest,
       FillFormField_MaxLength_CreditCardField_MaxLengthExceedsLength) {
  AutofillField field;
  field.set_max_length(30);
  field.set_credit_card_number_offset(2);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"0123456789999999");
  EXPECT_EQ(u"23456789999999", GetFillingValueForCreditCard(
                                   credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field));
}

// Verify that the full credit card number is returned if the offset exceeds the
// length.
TEST_F(FieldFillingPaymentsUtilTest,
       FillFormField_MaxLength_CreditCardField_OffsetExceedsLength) {
  AutofillField field;
  field.set_max_length(18);
  field.set_credit_card_number_offset(19);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"0123456789999999");
  EXPECT_EQ(u"0123456789999999", GetFillingValueForCreditCard(
                                     credit_card, /*cvc=*/u"", kAppLocale,
                                     mojom::ActionPersistence::kFill, field));
}

// Verify that only the truncated and offsetted value of the credit card number
// is set.
TEST_F(FieldFillingPaymentsUtilTest,
       FillFormField_MaxLength_CreditCardField_WithOffset) {
  AutofillField field;
  field.set_max_length(1);
  field.set_credit_card_number_offset(3);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"0123456789999999");
  // Verify that the field is filled with the third digit of the credit card
  // number.
  EXPECT_EQ(u"3", GetFillingValueForCreditCard(
                      credit_card, /*cvc=*/u"", kAppLocale,
                      mojom::ActionPersistence::kFill, field));
}

// Verify that only the truncated value of the credit card number is set.
TEST_F(FieldFillingPaymentsUtilTest, FillFormField_MaxLength_CreditCardField) {
  AutofillField field;
  field.set_max_length(1);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"4111111111111111");
  // Verify that the field is filled with only the first digit of the credit
  // card number.
  EXPECT_EQ(u"4", GetFillingValueForCreditCard(
                      credit_card, /*cvc=*/u"", kAppLocale,
                      mojom::ActionPersistence::kFill, field));
}

// Test that in the preview credit card numbers are obfuscated.
TEST_F(FieldFillingPaymentsUtilTest, FillFormField_Preview_CreditCardField) {
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"4111111111111111");
  // Verify that the field contains 4 but no more than 4 digits.
  size_t num_digits = base::ranges::count_if(
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field),
      &base::IsAsciiDigit<char16_t>);
  EXPECT_EQ(4u, num_digits);
}

class CreditCardVerificationCodeTest
    : public FieldFillingPaymentsUtilTest,
      public testing::WithParamInterface<mojom::ActionPersistence> {
 public:
  mojom::ActionPersistence persistence() { return GetParam(); }
};

// Test that verify CVC should be expected value for Preview and Fill.
TEST_P(CreditCardVerificationCodeTest,
       FillFormField_CreditCardVerificationCode) {
  AutofillField field;
  field.SetTypeTo(AutofillType(CREDIT_CARD_VERIFICATION_CODE));

  CreditCard credit_card;
  const std::u16string kCvc = u"1111";
  credit_card.set_cvc(kCvc);
  std::u16string value_to_fill = GetFillingValueForCreditCard(
      credit_card, /*cvc=*/u"", kAppLocale, persistence(), field);
  if (persistence() == mojom::ActionPersistence::kPreview) {
    EXPECT_EQ(kMidlineEllipsis4DotsWithoutPadding, value_to_fill);
  } else {
    EXPECT_EQ(kCvc, value_to_fill);
  }
}

// Test that verify CVC should be empty for Preview and Fill if CVC is empty.
TEST_P(CreditCardVerificationCodeTest,
       FillFormField_CreditCardVerificationCode_Empty) {
  AutofillField field;
  field.SetTypeTo(AutofillType(CREDIT_CARD_VERIFICATION_CODE));

  CreditCard credit_card;
  const std::u16string kEmptyCvc = u"";
  credit_card.set_cvc(kEmptyCvc);
  EXPECT_EQ(kEmptyCvc,
            GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                         persistence(), field));
}

// Tests that CVC is correctly previewed and filled for a standalone CVC field.
TEST_P(CreditCardVerificationCodeTest, FillFormField_StandaloneCVCField) {
  AutofillField field;
  field.SetTypeTo(AutofillType(CREDIT_CARD_STANDALONE_VERIFICATION_CODE));

  CreditCard credit_card = test::WithCvc(test::GetVirtualCard());
  std::u16string value_to_fill = GetFillingValueForCreditCard(
      credit_card, /*cvc=*/u"", kAppLocale, persistence(), field);
  switch (persistence()) {
    case mojom::ActionPersistence::kPreview:
      EXPECT_EQ(kMidlineEllipsis3DotsWithoutPadding, value_to_fill);
      return;
    case mojom::ActionPersistence::kFill:
      EXPECT_EQ(credit_card.cvc(), value_to_fill);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

// Tests that CVC is correctly previewed and filled for a standalone CVC field
// for American Express credit cards, which often use four digit verification
// codes.
TEST_P(CreditCardVerificationCodeTest,
       FillFormField_StandaloneCVCField_AmericanExpress) {
  AutofillField field;
  field.SetTypeTo(AutofillType(CREDIT_CARD_STANDALONE_VERIFICATION_CODE));

  CreditCard credit_card = test::GetVirtualCard();
  test_api(credit_card).set_network_for_card(kAmericanExpressCard);
  const std::u16string kCvc = u"1111";
  credit_card.set_cvc(kCvc);
  std::u16string value_to_fill = GetFillingValueForCreditCard(
      credit_card, kCvc, kAppLocale, persistence(), field);
  switch (persistence()) {
    case mojom::ActionPersistence::kPreview:
      EXPECT_EQ(kMidlineEllipsis4DotsWithoutPadding, value_to_fill);
      return;
    case mojom::ActionPersistence::kFill:
      EXPECT_EQ(kCvc, value_to_fill);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

INSTANTIATE_TEST_SUITE_P(FieldFillingPaymentsUtilTest,
                         CreditCardVerificationCodeTest,
                         testing::Values(mojom::ActionPersistence::kPreview,
                                         mojom::ActionPersistence::kFill));

struct FieldFillingPaymentsUtilTestCase {
  HtmlFieldType field_type;
  size_t field_max_length;
  std::u16string expected_value;

  FieldFillingPaymentsUtilTestCase(HtmlFieldType field_type,
                                   size_t field_max_length,
                                   std::u16string expected_value)
      : field_type(field_type),
        field_max_length(field_max_length),
        expected_value(expected_value) {}
};

class ExpirationYearTest
    : public FieldFillingPaymentsUtilTest,
      public testing::WithParamInterface<FieldFillingPaymentsUtilTestCase> {};

TEST_P(ExpirationYearTest, FillExpirationYearInput) {
  auto test_case = GetParam();
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.SetHtmlType(test_case.field_type, HtmlFieldMode());
  field.set_max_length(test_case.field_max_length);

  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationDateFromString(u"12/2023");
  EXPECT_EQ(
      test_case.expected_value,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field));
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingPaymentsUtilTest,
    ExpirationYearTest,
    testing::Values(
        // A field predicted as a 2 digits expiration year should fill the last
        // 2 digits of the expiration year if the field has an unspecified max
        // length (0) or if it's greater than 1.
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp2DigitYear,
            /* default value */ 0, u"23"},
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp2DigitYear, 2, u"23"},
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp2DigitYear, 12, u"23"},
        // A field predicted as a 2 digit expiration year should fill the last
        // digit of the expiration year if the field has a max length of 1.
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp2DigitYear, 1, u"3"},
        // A field predicted as a 4 digit expiration year should fill the 4
        // digits of the expiration year if the field has an unspecified max
        // length (0) or if it's greater than 3 .
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp4DigitYear,
            /* default value */ 0, u"2023"},
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp4DigitYear, 4, u"2023"},
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp4DigitYear, 12, u"2023"},
        // A field predicted as a 4 digits expiration year should fill the last
        // 2 digits of the expiration year if the field has a max length of 2.
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp4DigitYear, 2, u"23"},
        // A field predicted as a 4 digits expiration year should fill the last
        // digit of the expiration year if the field has a max length of 1.
        FieldFillingPaymentsUtilTestCase{
            HtmlFieldType::kCreditCardExp4DigitYear, 1, u"3"}));

struct FillUtilExpirationDateTestCase {
  const char* name;
  HtmlFieldType field_type;
  size_t field_max_length;
  std::u16string expected_value;
  bool expected_response;
  const char* opt_label = nullptr;
  FieldType server_override = UNKNOWN_TYPE;
  // If this is std::nullopt, a test is valid regardless whether the
  // features::kAutofillEnableExpirationDateImprovements is enabled or not.
  // If it is true, it should only execute if
  // features::kAutofillEnableExpirationDateImprovements is enabled. The inverse
  // applies for false.
  // TODO(crbug.com/40266396): Remove once launched. Delete all tests with a
  // value of false, and remove the attribute from tests with a value of true.
  std::optional<bool> for_expiration_date_improvements_experiment =
      std::nullopt;
};

class ExpirationDateTest
    : public FieldFillingPaymentsUtilTest,
      public testing::WithParamInterface<
          std::tuple<FillUtilExpirationDateTestCase,
                     // Whether kAutofillEnableExpirationDateImprovements should
                     // be enabled.
                     bool>> {};

TEST_P(ExpirationDateTest, FillExpirationDateInput) {
  auto test_case = std::get<0>(GetParam());
  auto enable_expiration_date_improvements = std::get<1>(GetParam());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      features::kAutofillEnableExpirationDateImprovements,
      enable_expiration_date_improvements);
  SCOPED_TRACE(
      ::testing::Message()
      << "name=" << test_case.name << ", field_type=" << test_case.field_type
      << ", field_max_length=" << test_case.field_max_length
      << ", expected_value=" << test_case.expected_value
      << ", expected_response=" << test_case.expected_response
      << ", opt_label=" << test_case.opt_label
      << ", server_override=" << test_case.server_override
      << ", for_expiration_date_improvements_experiment="
      << (test_case.for_expiration_date_improvements_experiment.has_value()
              ? (test_case.for_expiration_date_improvements_experiment.value()
                     ? "1"
                     : "0")
              : "not set"));

  if (test_case.for_expiration_date_improvements_experiment.has_value() &&
      test_case.for_expiration_date_improvements_experiment.value() !=
          base::FeatureList::IsEnabled(
              features::kAutofillEnableExpirationDateImprovements)) {
    // The test case does not apply to the current experiment configuration and
    // gets skipped.
    return;
  }

  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.SetHtmlType(test_case.field_type, HtmlFieldMode());
  field.set_max_length(test_case.field_max_length);

  CreditCardFieldParser::ExpirationDateFormat format =
      CreditCardFieldParser::DetermineExpirationDateFormat(
          field, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, NO_SERVER_DATA,
          NO_SERVER_DATA);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           format.digits_in_expiration_year == 2
                               ? CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
                               : CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  if (test_case.server_override != UNKNOWN_TYPE) {
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
        prediction;
    prediction.set_type(test_case.server_override);
    prediction.set_override(true);
    field.set_server_predictions({prediction});
  }
  if (test_case.opt_label) {
    field.set_label(base::UTF8ToUTF16(test_case.opt_label));
  }

  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationDateFromString(u"03/2022");
  std::u16string value_to_fill =
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field);
  EXPECT_EQ(!value_to_fill.empty(), test_case.expected_response);
  if (!value_to_fill.empty()) {
    EXPECT_EQ(test_case.expected_value, value_to_fill);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingPaymentsUtilTest,
    ExpirationDateTest,
    testing::Combine(
        testing::Values(
            // A field predicted as a expiration date w/ 2 digit year should
            // fill with a format of MM/YY
            FillUtilExpirationDateTestCase{
                "Test 1", HtmlFieldType::kCreditCardExpDate2DigitYear,
                /* default value */ 0, u"03/22", true},
            // Unsupported max lengths of 1-3, fail
            FillUtilExpirationDateTestCase{
                "Test 2", HtmlFieldType::kCreditCardExpDate2DigitYear, 1, u"",
                false},
            FillUtilExpirationDateTestCase{
                "Test 3", HtmlFieldType::kCreditCardExpDate2DigitYear, 2, u"",
                false},
            FillUtilExpirationDateTestCase{
                "Test 4", HtmlFieldType::kCreditCardExpDate2DigitYear, 3, u"",
                false},
            // A max length of 4 indicates a format of MMYY.
            FillUtilExpirationDateTestCase{
                "Test 5", HtmlFieldType::kCreditCardExpDate2DigitYear, 4,
                u"0322", true},
            // Desired case of proper max length >= 5
            FillUtilExpirationDateTestCase{
                "Test 6", HtmlFieldType::kCreditCardExpDate2DigitYear, 5,
                u"03/22", true},
            // If kAutofillEnableExpirationDateImprovements is enabled, the
            // overall type comes from
            // HtmlFieldType::kCreditCardExpDate2DigitYear and therefore, we
            // fill 03/22.
            FillUtilExpirationDateTestCase{
                .name = "Test 7",
                .field_type = HtmlFieldType::kCreditCardExpDate2DigitYear,
                .field_max_length = 6,
                .expected_value = u"03/22",
                .expected_response = true,
                .for_expiration_date_improvements_experiment = true},
            // If kAutofillEnableExpirationDateImprovements is disabled, the max
            // length drives the length of the filling.
            FillUtilExpirationDateTestCase{
                .name = "Test 8",
                .field_type = HtmlFieldType::kCreditCardExpDate2DigitYear,
                .field_max_length = 6,
                .expected_value = u"032022",
                .expected_response = true,
                .for_expiration_date_improvements_experiment = false},
            // If kAutofillEnableExpirationDateImprovements is enabled, the
            // overall type comes from
            // HtmlFieldType::kCreditCardExpDate2DigitYear and therefore, we
            // fill 03/22.
            FillUtilExpirationDateTestCase{
                .name = "Test 9",
                .field_type = HtmlFieldType::kCreditCardExpDate2DigitYear,
                .field_max_length = 7,
                .expected_value = u"03/22",
                .expected_response = true,
                .for_expiration_date_improvements_experiment = true},
            // If kAutofillEnableExpirationDateImprovements is disabled, the max
            // length drives the length of the filling.
            FillUtilExpirationDateTestCase{
                .name = "Test 10",
                .field_type = HtmlFieldType::kCreditCardExpDate2DigitYear,
                .field_max_length = 7,
                .expected_value = u"03/2022",
                .expected_response = true,
                .for_expiration_date_improvements_experiment = false},
            FillUtilExpirationDateTestCase{
                "Test 11", HtmlFieldType::kCreditCardExpDate2DigitYear, 12,
                u"03/22", true},

            // A field predicted as a expiration date w/ 4 digit year should
            // fill with a format of MM/YYYY unless it has max-length of: 4: Use
            // format MMYY 5: Use format MM/YY 6: Use format MMYYYY
            FillUtilExpirationDateTestCase{
                "Test 12", HtmlFieldType::kCreditCardExpDate4DigitYear,
                /* default value */ 0, u"03/2022", true},
            // Unsupported max lengths of 1-3, fail
            FillUtilExpirationDateTestCase{
                "Test 13", HtmlFieldType::kCreditCardExpDate4DigitYear, 1, u"",
                false},
            FillUtilExpirationDateTestCase{
                "Test 14", HtmlFieldType::kCreditCardExpDate4DigitYear, 2, u"",
                false},
            FillUtilExpirationDateTestCase{
                "Test 15", HtmlFieldType::kCreditCardExpDate4DigitYear, 3, u"",
                false},
            // A max length of 4 indicates a format of MMYY.
            FillUtilExpirationDateTestCase{
                "Test 16", HtmlFieldType::kCreditCardExpDate4DigitYear, 4,
                u"0322", true},
            // A max length of 5 indicates a format of MM/YY.
            FillUtilExpirationDateTestCase{
                "Test 17", HtmlFieldType::kCreditCardExpDate4DigitYear, 5,
                u"03/22", true},
            // A max length of 6 indicates a format of MMYYYY.
            FillUtilExpirationDateTestCase{
                "Test 18", HtmlFieldType::kCreditCardExpDate4DigitYear, 6,
                u"032022", true},
            // Desired case of proper max length >= 7
            FillUtilExpirationDateTestCase{
                "Test 19", HtmlFieldType::kCreditCardExpDate4DigitYear, 7,
                u"03/2022", true},
            FillUtilExpirationDateTestCase{
                "Test 20", HtmlFieldType::kCreditCardExpDate4DigitYear, 12,
                u"03/2022", true},

            // Tests for features::kAutofillFillCreditCardAsPerFormatString:

            // Base case works regardless of capitalization.
            FillUtilExpirationDateTestCase{
                "Test 21", HtmlFieldType::kCreditCardExpDate2DigitYear, 0,
                u"03/22", true, "mm/yy"},
            FillUtilExpirationDateTestCase{
                "Test 22", HtmlFieldType::kCreditCardExpDate2DigitYear, 0,
                u"03/22", true, "MM/YY"},
            // If we expect a 4 digit expiration date, we consider only the
            // separator of the the placeholder once
            // kAutofillEnableExpirationDateImprovements is launched.
            //
            // If kAutofillEnableExpirationDateImprovements is enabled, the
            // overall type comes from
            // HtmlFieldType::kCreditCardExpDate4DigitYear and therefore, we
            // fill 03/2022.
            FillUtilExpirationDateTestCase{
                .name = "Test 23",
                .field_type = HtmlFieldType::kCreditCardExpDate4DigitYear,
                .field_max_length = 0,
                .expected_value = u"03/2022",
                .expected_response = true,
                .opt_label = "MM/YY",
                .for_expiration_date_improvements_experiment = true},
            // If kAutofillEnableExpirationDateImprovements is disabled, the
            // pattern defines the length of the filling.
            FillUtilExpirationDateTestCase{
                .name = "Test 24",
                .field_type = HtmlFieldType::kCreditCardExpDate4DigitYear,
                .field_max_length = 0,
                .expected_value = u"03/22",
                .expected_response = true,
                .opt_label = "MM/YY",
                .for_expiration_date_improvements_experiment = false},
            // Whitespaces are respected.
            FillUtilExpirationDateTestCase{
                "Test 25", HtmlFieldType::kCreditCardExpDate2DigitYear, 0,
                u"03 / 22", true, "MM / YY"},
            // Whitespaces are stripped if that makes the string fit.
            FillUtilExpirationDateTestCase{
                "Test 26", HtmlFieldType::kCreditCardExpDate2DigitYear, 5,
                u"03/22", true, "MM / YY"},
            // Different separators work.
            FillUtilExpirationDateTestCase{
                "Test 27", HtmlFieldType::kCreditCardExpDate2DigitYear, 0,
                u"03-22", true, "MM-YY"},
            // Four year expiration years work.
            FillUtilExpirationDateTestCase{
                "Test 28", HtmlFieldType::kCreditCardExpDate4DigitYear, 0,
                u"03-2022", true, "MM-YYYY"},
            // Some extra text around the pattern does not matter.
            FillUtilExpirationDateTestCase{
                "Test 29", HtmlFieldType::kCreditCardExpDate2DigitYear, 0,
                u"03/22", true, "Credit card in format MM/YY."},
            // Fallback to the length based filling in case the maxlength is too
            // low.
            FillUtilExpirationDateTestCase{
                "Test 30", HtmlFieldType::kCreditCardExpDate4DigitYear, 5,
                u"03/22", true, "MM/YYYY"},
            // Empty strings are handled gracefully.
            FillUtilExpirationDateTestCase{
                "Test 31", HtmlFieldType::kCreditCardExpDate4DigitYear, 5,
                u"03/22", true, ""},

            // Test manual server overrides:
            // Even if the label indicates a mm/yy, a server override for a 4
            // digit year should be honored if it fits.
            FillUtilExpirationDateTestCase{
                "Test 32", HtmlFieldType::kCreditCardExpDate2DigitYear, 0,
                u"03/2022", true, "mm/yy", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
            // Follow the server if it overrides a shorter date format.
            FillUtilExpirationDateTestCase{
                "Test 33", HtmlFieldType::kCreditCardExpDate4DigitYear, 0,
                u"03/22", true, "mm/yyyy", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Follow the server but preserve the separator if possible.
            FillUtilExpirationDateTestCase{
                "Test 34", HtmlFieldType::kCreditCardExpDate4DigitYear, 0,
                u"03 - 22", true, "mm - yyyy",
                CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // Follow the server but preserve the separator if possible, even if
            // that means that whitespaces need to be pruned.
            FillUtilExpirationDateTestCase{
                "Test 35", HtmlFieldType::kCreditCardExpDate4DigitYear, 5,
                u"03-22", true, "mm - yy", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
            // If the server format just does not fit, fall back to heuristics.
            FillUtilExpirationDateTestCase{
                "Test 36", HtmlFieldType::kCreditCardExpDate4DigitYear, 4,
                u"0322", true, "mm/yy", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}),
        testing::Bool()));

struct FillWithExpirationMonthTestCase {
  std::vector<const char*> select_values;
  std::vector<const char*> select_contents;
};

class AutofillSelectWithExpirationMonthTest
    : public FieldFillingPaymentsUtilTest,
      public testing::WithParamInterface<FillWithExpirationMonthTestCase> {};

TEST_P(AutofillSelectWithExpirationMonthTest,
       FillSelectControlWithExpirationMonth) {
  auto test_case = GetParam();
  ASSERT_EQ(test_case.select_values.size(), test_case.select_contents.size());

  TestFillingExpirationMonth(test_case.select_values,
                             test_case.select_contents);
}

INSTANTIATE_TEST_SUITE_P(
    FieldFillingPaymentsUtilTest,
    AutofillSelectWithExpirationMonthTest,
    testing::Values(
        // Values start at 1.
        FillWithExpirationMonthTestCase{
            {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Values start at 1 but single digits are whitespace padded!
        FillWithExpirationMonthTestCase{
            {" 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11",
             "12"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Values start at 0.
        FillWithExpirationMonthTestCase{
            {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Values start at 00.
        FillWithExpirationMonthTestCase{
            {"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10",
             "11"},
            NotNumericMonthsContentsNoPlaceholder()},
        // The AngularJS framework can add a "number:" prefix to select values.
        FillWithExpirationMonthTestCase{
            {"number:1", "number:2", "number:3", "number:4", "number:5",
             "number:6", "number:7", "number:8", "number:9", "number:10",
             "number:11", "number:12"},
            NotNumericMonthsContentsNoPlaceholder()},
        // The AngularJS framework can add a "string:" prefix to select values.
        FillWithExpirationMonthTestCase{
            {"string:1", "string:2", "string:3", "string:4", "string:5",
             "string:6", "string:7", "string:8", "string:9", "string:10",
             "string:11", "string:12"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Unexpected values that can be matched with the content.
        FillWithExpirationMonthTestCase{
            {"object:1", "object:2", "object:3", "object:4", "object:5",
             "object:6", "object:7", "object:8", "object:9", "object:10",
             "object:11", "object:12"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Another example where unexpected values can be matched with the
        // content.
        FillWithExpirationMonthTestCase{
            {"object:a", "object:b", "object:c", "object:d", "object:e",
             "object:f", "object:g", "object:h", "object:i", "object:j",
             "object:k", "object:l"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Another example where unexpected values can be matched with the
        // content.
        FillWithExpirationMonthTestCase{
            {"Farvardin", "Ordibehesht", "Khordad", "Tir", "Mordad",
             "Shahrivar", "Mehr", "Aban", "Azar", "Dey", "Bahman", "Esfand"},
            NotNumericMonthsContentsNoPlaceholder()},
        // Values start at 0 and the first content is a placeholder.
        FillWithExpirationMonthTestCase{
            {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
             "12"},
            NotNumericMonthsContentsWithPlaceholder()},
        // Values start at 1 and the first content is a placeholder.
        FillWithExpirationMonthTestCase{
            {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
             "13"},
            NotNumericMonthsContentsWithPlaceholder()},
        // Values start at 01 and the first content is a placeholder.
        FillWithExpirationMonthTestCase{
            {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11",
             "12", "13"},
            NotNumericMonthsContentsWithPlaceholder()},
        // Values start at 0 after a placeholder.
        FillWithExpirationMonthTestCase{
            {"?", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"},
            NotNumericMonthsContentsWithPlaceholder()},
        // Values start at 1 after a placeholder.
        FillWithExpirationMonthTestCase{
            {"?", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
             "12"},
            NotNumericMonthsContentsWithPlaceholder()},
        // Values start at 0 after a negative number.
        FillWithExpirationMonthTestCase{
            {"-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
             "11"},
            NotNumericMonthsContentsWithPlaceholder()},
        // Values start at 1 after a negative number.
        FillWithExpirationMonthTestCase{
            {"-1", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
             "12"},
            NotNumericMonthsContentsWithPlaceholder()}));

TEST_F(FieldFillingPaymentsUtilTest,
       FillSelectControlWithAbbreviatedMonthName) {
  AutofillField field =
      CreateTestSelectAutofillField({"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
                                    CREDIT_CARD_EXP_MONTH);

  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(4);
  EXPECT_EQ(u"Apr", GetFillingValueForCreditCard(
                        credit_card, /*cvc=*/u"", kAppLocale,
                        mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest, FillSelectControlWithMonthName) {
  AutofillField field = CreateTestSelectAutofillField(
      {"January", "February", "March", "April", "May", "June", "July", "August",
       "September", "October", "November", "December"},
      CREDIT_CARD_EXP_MONTH);
  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(4);
  EXPECT_EQ(u"April", GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest, FillSelectControlWithMonthNameAndDigits) {
  AutofillField field = CreateTestSelectAutofillField(
      {"January (01)", "February (02)", "March (03)", "April (04)", "May (05)",
       "June (06)", "July (07)", "August (08)", "September (09)",
       "October (10)", "November (11)", "December (12)"},
      CREDIT_CARD_EXP_MONTH);
  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(4);
  EXPECT_EQ(u"April (04)", GetFillingValueForCreditCard(
                               credit_card, /*cvc=*/u"", kAppLocale,
                               mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest,
       FillSelectControlWithMonthNameAndDigits_French) {
  AutofillField field = CreateTestSelectAutofillField(
      {
          "01 - JANVIER", "02 - FÉVRIER", "03 - MARS", "04 - AVRIL", "05 - MAI",
          "06 - JUIN", "07 - JUILLET", "08 - AOÛT", "09 - SEPTEMBRE",
          "10 - OCTOBRE", "11 - NOVEMBRE",
          "12 - DECEMBRE" /* Intentionally not including accent in DÉCEMBRE */
      },
      CREDIT_CARD_EXP_MONTH);
  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(8);
  EXPECT_EQ(u"08 - AOÛT", GetFillingValueForCreditCard(
                              credit_card, /*cvc=*/u"", /*app_locale=*/"fr-FR",
                              mojom::ActionPersistence::kFill, field));
  credit_card.SetExpirationMonth(12);
  EXPECT_EQ(u"12 - DECEMBRE", GetFillingValueForCreditCard(
                                  credit_card, /*cvc=*/u"", kAppLocale,
                                  mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest, FillSelectControlWithMonthName_French) {
  AutofillField field = CreateTestSelectAutofillField(
      {"JANV", "FÉVR.", "MARS", "décembre"}, CREDIT_CARD_EXP_MONTH);
  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(2);
  EXPECT_EQ(u"FÉVR.", GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", /*app_locale=*/"fr-FR",
                          mojom::ActionPersistence::kFill, field));

  credit_card.SetExpirationMonth(1);
  EXPECT_EQ(u"JANV", GetFillingValueForCreditCard(
                         credit_card, /*cvc=*/u"", /*app_locale=*/"fr-FR",
                         mojom::ActionPersistence::kFill, field));

  credit_card.SetExpirationMonth(12);
  EXPECT_EQ(u"décembre", GetFillingValueForCreditCard(
                             credit_card, /*cvc=*/u"", /*app_locale=*/"fr-FR",
                             mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest,
       FillSelectControlWithNumericMonthSansLeadingZero) {
  std::vector<const char*> kMonthsNumeric = {
      "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
  };
  AutofillField field =
      CreateTestSelectAutofillField(kMonthsNumeric, CREDIT_CARD_EXP_MONTH);

  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationMonth(4);
  EXPECT_EQ(u"4", GetFillingValueForCreditCard(
                      credit_card, /*cvc=*/u"", kAppLocale,
                      mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest,
       FillSelectControlWithTwoDigitCreditCardYear) {
  AutofillField field = CreateTestSelectAutofillField(
      {"12", "13", "14", "15", "16", "17", "18", "19"},
      CREDIT_CARD_EXP_2_DIGIT_YEAR);

  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationYear(2017);
  EXPECT_EQ(u"17", GetFillingValueForCreditCard(
                       credit_card, /*cvc=*/u"", kAppLocale,
                       mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest, FillSelectControlWithCreditCardType) {
  AutofillField field = CreateTestSelectAutofillField(
      {"Visa", "Mastercard", "AmEx", "discover"}, CREDIT_CARD_TYPE);
  CreditCard credit_card = test::GetCreditCard();

  // Normal case:
  credit_card.SetNumber(u"4111111111111111");  // Visa number.
  EXPECT_EQ(u"Visa", GetFillingValueForCreditCard(
                         credit_card, /*cvc=*/u"", kAppLocale,
                         mojom::ActionPersistence::kFill, field));

  // Filling should be able to handle intervening whitespace:
  credit_card.SetNumber(u"5555555555554444");  // MC number.
  EXPECT_EQ(u"Mastercard", GetFillingValueForCreditCard(
                               credit_card, /*cvc=*/u"", kAppLocale,
                               mojom::ActionPersistence::kFill, field));

  // American Express is sometimes abbreviated as AmEx:
  credit_card.SetNumber(u"378282246310005");  // Amex number.
  EXPECT_EQ(u"AmEx", GetFillingValueForCreditCard(
                         credit_card, /*cvc=*/u"", kAppLocale,
                         mojom::ActionPersistence::kFill, field));

  // Case insensitivity:
  credit_card.SetNumber(u"6011111111111117");  // Discover number.
  EXPECT_EQ(u"discover", GetFillingValueForCreditCard(
                             credit_card, /*cvc=*/u"", kAppLocale,
                             mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest, FillMonthControl) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputMonth);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_4_DIGIT_YEAR);

  // Try a month with two digits.
  CreditCard credit_card = test::GetCreditCard();
  credit_card.SetExpirationDateFromString(u"12/2017");
  EXPECT_EQ(u"2017-12", GetFillingValueForCreditCard(
                            credit_card, /*cvc=*/u"", kAppLocale,
                            mojom::ActionPersistence::kFill, field));

  // Try a month with a leading zero.
  credit_card.SetExpirationDateFromString(u"03/2019");
  EXPECT_EQ(u"2019-03", GetFillingValueForCreditCard(
                            credit_card, /*cvc=*/u"", kAppLocale,
                            mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest, FillCreditCardNumberWithoutSplits) {
  // Case 1: card number without any split.
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(u"41111111111111111");
  // Verify that full card-number shall get filled properly.
  EXPECT_EQ(u"41111111111111111", GetFillingValueForCreditCard(
                                      credit_card, /*cvc=*/u"", kAppLocale,
                                      mojom::ActionPersistence::kFill, field));
  EXPECT_EQ(0U, field.credit_card_number_offset());
}

TEST_F(FieldFillingPaymentsUtilTest, FillCreditCardNumberWithEqualSizeSplits) {
  // Case 2: card number broken up into four equal groups, of length 4.
  CreditCardTestCase test;
  test.card_number_ = u"5187654321098765";
  test.total_splits_ = 4;
  test.splits_ = {4, 4, 4, 4};
  test.expected_results_ = {u"5187", u"6543", u"2109", u"8765"};

  for (size_t i = 0; i < test.total_splits_; ++i) {
    AutofillField field;
    field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
    field.set_max_length(test.splits_[i]);
    field.set_credit_card_number_offset(4 * i);

    // Fill with a card-number; should fill just the card_number_part.
    CreditCard credit_card;
    credit_card.SetNumber(test.card_number_);
    EXPECT_EQ(
        test.expected_results_[i],
        GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                     mojom::ActionPersistence::kFill, field));
    EXPECT_EQ(4 * i, field.credit_card_number_offset());
  }

  // Verify that full card-number shall get fill properly as well.
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(test.card_number_);
  EXPECT_EQ(test.card_number_, GetFillingValueForCreditCard(
                                   credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest,
       PreviewCreditCardNumberWithEqualSizeSplits) {
  // Case 2: card number broken up into four equal groups, of length 4.
  CreditCardTestCase test;
  test.card_number_ = u"5187654321098765";
  test.total_splits_ = 4;
  test.splits_ = {4, 4, 4, 4};
  test.expected_results_ = {u"\x2022\x2022\x2022\x2022",
                            u"\x2022\x2022\x2022\x2022",
                            u"\x2022\x2022\x2022\x2022", u"8765"};
  // 12 dots and last four of card number.
  std::u16string obfuscated_card_number =
      u"\x202A\x2022\x2060\x2006\x2060\x2022\x2060\x2006\x2060\x2022\x2060"
      u"\x2006\x2060\x2022\x2060\x2006\x2060"
      u"8765\x202C";
  for (size_t i = 0; i < test.total_splits_; ++i) {
    AutofillField field;
    field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
    field.set_max_length(test.splits_[i]);
    field.set_credit_card_number_offset(4 * i);

    // Fill with a card-number; should fill just the card_number_part.
    CreditCard credit_card;
    credit_card.SetNumber(test.card_number_);
    EXPECT_EQ(test.expected_results_[i],
              GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                           mojom::ActionPersistence::kPreview,
                                           field));
    EXPECT_EQ(4 * i, field.credit_card_number_offset());
  }

  // Verify that full card-number shall get fill properly as well.
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card;
  credit_card.SetNumber(test.card_number_);
  EXPECT_EQ(
      obfuscated_card_number,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest,
       FillCreditCardNumberWithUnequalSizeSplits) {
  // Case 3: card with 15 digits number, broken up into three unequal groups, of
  // lengths 4, 6, and 5.
  CreditCardTestCase test;
  test.card_number_ = u"423456789012345";
  test.total_splits_ = 3;
  test.splits_ = {4, 6, 5};
  test.expected_results_ = {u"4234", u"567890", u"12345"};

  // Start executing test cases to verify parts and full credit card number.
  for (size_t i = 0; i < test.total_splits_; ++i) {
    AutofillField field;
    field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
    field.set_max_length(test.splits_[i]);
    field.set_credit_card_number_offset(GetNumberOffset(i, test));

    // Fill with a card-number; should fill just the card_number_part.
    CreditCard credit_card;
    credit_card.SetNumber(test.card_number_);
    EXPECT_EQ(
        test.expected_results_[i],
        GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                     mojom::ActionPersistence::kFill, field));
    EXPECT_EQ(GetNumberOffset(i, test), field.credit_card_number_offset());
  }

  // Verify that full card-number shall get fill properly as well.
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
  CreditCard credit_card;
  credit_card.SetNumber(test.card_number_);
  EXPECT_EQ(test.card_number_, GetFillingValueForCreditCard(
                                   credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kFill, field));
}

TEST_F(FieldFillingPaymentsUtilTest,
       PreviewCreditCardNumberWithUnequalSizeSplits) {
  // Case 3: card with 15 digits number, broken up into three unequal groups, of
  // lengths 4, 6, and 5.
  CreditCardTestCase test;
  test.card_number_ = u"423456789012345";
  // 12 dots and last four of card number.
  std::u16string obfuscated_card_number =
      u"\x202A\x2022\x2060\x2006\x2060\x2022\x2060\x2006\x2060\x2022\x2060"
      u"\x2006\x2060\x2022\x2060\x2006\x2060"
      u"2345\x202C";
  test.total_splits_ = 3;
  test.splits_ = {4, 6, 6};
  test.expected_results_ = {u"\x2022\x2022\x2022\x2022",
                            u"\x2022\x2022\x2022\x2022\x2022\x2022",
                            u"\x2022\x2022"
                            u"2345"};

  // Start executing test cases to verify parts and full credit card number.
  for (size_t i = 0; i < test.total_splits_; ++i) {
    AutofillField field;
    field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
    field.set_max_length(test.splits_[i]);
    field.set_credit_card_number_offset(GetNumberOffset(i, test));

    // Fill with a card-number; should fill just the card_number_part.
    CreditCard credit_card;
    credit_card.SetNumber(test.card_number_);
    EXPECT_EQ(test.expected_results_[i],
              GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                           mojom::ActionPersistence::kPreview,
                                           field));
    EXPECT_EQ(GetNumberOffset(i, test), field.credit_card_number_offset());
  }

  // Verify that full card-number shall get fill properly as well.
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
  CreditCard credit_card;
  credit_card.SetNumber(test.card_number_);
  EXPECT_EQ(
      obfuscated_card_number,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualMonth) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_EXP_MONTH);

  // A month with two digits should return two dots.
  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetExpirationDateFromString(u"12/2017");
  EXPECT_EQ(
      kMidlineEllipsis2DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));

  // A month with one digit should still return two dots.
  credit_card.SetExpirationDateFromString(u"03/2019");
  EXPECT_EQ(
      kMidlineEllipsis2DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

// Test that month should be empty for Preview if the form control type of the
// field is `kSelectOne`, i.e., is a combobox or listbox.
TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualMonthOneSelectOne_Empty) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kSelectOne);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_EXP_MONTH);

  CreditCard card = test::GetVirtualCard();
  EXPECT_TRUE(GetFillingValueForCreditCard(card, /*cvc=*/u"", kAppLocale,
                                           mojom::ActionPersistence::kPreview,
                                           field)
                  .empty());
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualYear) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_4_DIGIT_YEAR);

  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetExpirationDateFromString(u"12/2017");
  EXPECT_EQ(
      kMidlineEllipsis4DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));

  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_2_DIGIT_YEAR);
  EXPECT_EQ(
      kMidlineEllipsis2DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

// Test that 4 digit year should be empty for Preview if the form control type
// of the field is `kSelectOne`, i.e., is a combobox or listbox.
TEST_F(FieldFillingPaymentsUtilTest,
       PreviewVirtualFourDigitYearOnSelectOne_Empty) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kSelectOne);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_4_DIGIT_YEAR);

  CreditCard card = test::GetVirtualCard();
  EXPECT_TRUE(GetFillingValueForCreditCard(card, /*cvc=*/u"", kAppLocale,
                                           mojom::ActionPersistence::kPreview,
                                           field)
                  .empty());
}

// Test that 2 digit year should be empty for Preview if the form control type
// of the field is `kSelectOne`, i.e., is a combobox or listbox.
TEST_F(FieldFillingPaymentsUtilTest,
       PreviewVirtualTwoDigitYearOnSelectOne_Empty) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kSelectOne);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_2_DIGIT_YEAR);

  CreditCard card = test::GetVirtualCard();
  EXPECT_TRUE(GetFillingValueForCreditCard(card, /*cvc=*/u"", kAppLocale,
                                           mojom::ActionPersistence::kPreview,
                                           field)
                  .empty());
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualShortenedYear) {
  // Test reducing 4 digit year to 2 digits.
  AutofillField field;
  field.set_max_length(2);
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_4_DIGIT_YEAR);

  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetExpirationDateFromString(u"12/2017");
  EXPECT_EQ(
      kMidlineEllipsis2DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualDate) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  field.set_max_length(7);

  // A date that has a year containing four digits should return two dots for
  // month and four dots for year.
  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetExpirationDateFromString(u"12/2017");
  std::u16string slash = u"/";
  std::u16string expected =
      base::StrCat({kMidlineEllipsis2DotsWithoutPadding, slash,
                    kMidlineEllipsis4DotsWithoutPadding});
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));

  // A date that has a year containing two digits should return two dots for
  // month and two for year.
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  field.set_max_length(5);
  expected = base::StrCat({kMidlineEllipsis2DotsWithoutPadding, slash,
                           kMidlineEllipsis2DotsWithoutPadding});
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualShortenedDate) {
  // Test reducing dates to various max length field values.
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_max_length(4);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetExpirationDateFromString(u"12/2017");
  // Expected: MMYY = ••••. Unlikely case
  std::u16string expected = kMidlineEllipsis4DotsWithoutPadding;
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));

  field.set_max_length(5);
  std::u16string slash = u"/";
  // Expected: MM/YY = ••/••.
  expected = base::StrCat({kMidlineEllipsis2DotsWithoutPadding, slash,
                           kMidlineEllipsis2DotsWithoutPadding});
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));

  field.set_max_length(6);
  // Expected: MMYYYY = ••••••.
  expected = base::StrCat({kMidlineEllipsis2DotsWithoutPadding,
                           kMidlineEllipsis4DotsWithoutPadding});
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));

  field.set_max_length(7);
  // Expected: MM/YYYY = ••/••••.
  expected = base::StrCat({kMidlineEllipsis2DotsWithoutPadding, slash,
                           kMidlineEllipsis4DotsWithoutPadding});
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualCVC) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_VERIFICATION_CODE);

  CreditCard credit_card = test::GetVirtualCard();
  test_api(credit_card).set_network_for_card(kMasterCard);
  EXPECT_EQ(
      kMidlineEllipsis3DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualCVCAmericanExpress) {
  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           CREDIT_CARD_VERIFICATION_CODE);

  CreditCard credit_card = test::GetVirtualCard();
  test_api(credit_card).set_network_for_card(kAmericanExpressCard);
  EXPECT_EQ(
      kMidlineEllipsis4DotsWithoutPadding,
      GetFillingValueForCreditCard(credit_card, /*cvc=*/u"", kAppLocale,
                                   mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualCardNumber) {
  AutofillField field;
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);
  field.set_form_control_type(FormControlType::kInputText);

  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetNumber(u"5454545454545454");
  test_api(credit_card).set_network_for_card(kMasterCard);
  // Virtual card Mastercard ••••5454‬
  std::u16string expected =
      u"Virtual card Mastercard  "
      u"\x202A\x2022\x2060\x2006\x2060\x2022\x2060\x2006\x2060\x2022\x2060"
      u"\x2006\x2060\x2022\x2060\x2006\x2060"
      u"5454\x202C";
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));
}

// Verify that the obfuscated virtual card number is returned if the offset is
// greater than 0 and the offset exceeds the length.
TEST_F(FieldFillingPaymentsUtilTest,
       PreviewVirtualCardNumber_OffsetExceedsLength) {
  AutofillField field;
  field.set_max_length(17);
  field.set_credit_card_number_offset(18);
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NUMBER);

  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetNumber(u"5454545454545454");
  test_api(credit_card).set_network_for_card(kMasterCard);
  // ••••••••••••5454‬
  std::u16string expected =
      u"\x2022\x2022\x2022\x2022\x2022\x2022\x2022\x2022\x2022\x2022\x2022"
      u"\x2022"
      u"5454";
  // Verify that the field is previewed with the full card number.
  EXPECT_EQ(expected, GetFillingValueForCreditCard(
                          credit_card, /*cvc=*/u"", kAppLocale,
                          mojom::ActionPersistence::kPreview, field));
}

TEST_F(FieldFillingPaymentsUtilTest, PreviewVirtualCardholderName) {
  std::u16string name = u"Jone Doe";

  AutofillField field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_heuristic_type(GetActiveHeuristicSource(), CREDIT_CARD_NAME_FULL);

  CreditCard credit_card = test::GetVirtualCard();
  credit_card.SetRawInfoWithVerificationStatus(CREDIT_CARD_NAME_FULL, name,
                                               VerificationStatus::kFormatted);
  EXPECT_EQ(name, GetFillingValueForCreditCard(
                      credit_card, /*cvc=*/u"", kAppLocale,
                      mojom::ActionPersistence::kPreview, field));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns false on a form with no
// credit card number or CVC fields.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_NoCCNumberField) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"}}});

  FormStructure form_structure(form_data);
  test_api(form_structure).SetFieldTypes({NAME_FIRST});

  EXPECT_FALSE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns false on a form where
// the credit card number field is present but it is not empty.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_CCNumberFieldNotEmpty) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"},
                                    {.role = CREDIT_CARD_NUMBER,
                                     .label = u"Card Number",
                                     .value = u"field is not empty",
                                     .properties_mask = kUserTyped}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NUMBER});

  EXPECT_FALSE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns false on a form where
// the credit card number field is present but it's autofilled.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_CCNumberFieldIsAutofilled) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"},
                                    {.role = CREDIT_CARD_NUMBER,
                                     .label = u"Card Number",
                                     .is_autofilled = true,
                                     .properties_mask = kUserTyped}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NUMBER});

  EXPECT_FALSE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` return true on a form where the
// credit card number field is present and is both empty and not autofilled.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_CCNumberFieldPresent) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"},
                                    {.role = CREDIT_CARD_NUMBER,
                                     .label = u"Card Number",
                                     .properties_mask = kUserTyped}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NUMBER});

  EXPECT_TRUE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` return true on a form where the
// credit card number field is present and not empty but was not typed by the
// user if `features::kAutofillSkipPreFilledFields` is disabled.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_CCNumberFieldNotEmpty_NotUserTyped) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCvcStorageAndFilling},
      /*disabled_features=*/{features::kAutofillSkipPreFilledFields});
  FormData form_data = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .label = u"First Name on Card"},
           {.role = CREDIT_CARD_NUMBER,
            .label = u"Card Number",
            .value = u"field is not empty",
            .properties_mask = kAutofilledOnPageLoad}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NUMBER});

  EXPECT_TRUE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns true on a form with only
// a credit card credential standalone field if the card has CVC saved.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_StandaloneCvcField_CardHasCvc) {
  FormData form_data = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
                   .label = u"Card verification standalone code"}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_STANDALONE_VERIFICATION_CODE});

  EXPECT_TRUE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns true on a form with only
// a credit card credential field if the card has CVC saved.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_NormalCvcFormField_CardHasCvc) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE,
                                     .label = u"Card verification code"}}});

  FormStructure form_structure(form_data);
  test_api(form_structure).SetFieldTypes({CREDIT_CARD_VERIFICATION_CODE});

  EXPECT_TRUE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns false on a form where
// the credit card verification code field is present but it is not empty and
// the card has CVC saved.
// The CVC field isn't overridden in this case, and we don't need to fetch the
// card as there is no card number field.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_CvcFieldNotEmpty_CardHasCvc) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"},
                                    {.role = CREDIT_CARD_VERIFICATION_CODE,
                                     .label = u"Card verification code",
                                     .value = u"123",
                                     .properties_mask = kUserTyped}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_VERIFICATION_CODE});

  EXPECT_FALSE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns true on a form where
// the credit card verification code field is present but it is empty and the
// card has CVC saved. Also the trigger field is the non CVC field.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_FormHasCvcAndName_CardHasCvc) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"},
                                    {.role = CREDIT_CARD_VERIFICATION_CODE,
                                     .label = u"Card verification code"}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_VERIFICATION_CODE});

  EXPECT_TRUE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/true));
}

// Verify that `WillFillCreditCardNumberOrCvc` returns false on a form where
// the credit card verification code field is present but it is empty and the
// card has no CVC saved. Also the trigger field is the non CVC field.
TEST_F(FieldFillingPaymentsUtilTest,
       WillFillCreditCardNumberOrCvc_FormHasCvcAndName_CardHasNoCvc) {
  FormData form_data =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .label = u"First Name on Card"},
                                    {.role = CREDIT_CARD_VERIFICATION_CODE,
                                     .label = u"Card verification code"}}});

  FormStructure form_structure(form_data);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_VERIFICATION_CODE});

  EXPECT_FALSE(WillFillCreditCardNumberOrCvc(
      form_data.fields(), form_structure.fields(), *form_structure.fields()[0],
      /*card_has_cvc=*/false));
}

}  // namespace

}  // namespace autofill
