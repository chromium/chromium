// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/dense_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using ::testing::ContainerEq;

std::unique_ptr<FormStructure> CreateFormStructure(
    const DenseSet<FieldType>& field_types) {
  test::FormDescription form_description;
  for (FieldType field_type : field_types) {
    form_description.fields.emplace_back(
        test::FieldDescription({.role = field_type}));
  }
  auto form_structure =
      std::make_unique<FormStructure>(test::GetFormData(form_description));
  for (size_t i = 0; i < form_structure->field_count(); i++) {
    form_structure->field(i)->SetTypeTo(
        AutofillType(form_description.fields[i].role));
  }
  return form_structure;
}

class AutofillMetricsUtilsTest : public testing::Test {
 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

using IsCertainFormTypeTestData = std::tuple<DenseSet<FieldType>, bool>;

// Test fixture for testing `internal::IsEmailOnlyForm()`.
class IsEmailOnlyFormTest
    : public AutofillMetricsUtilsTest,
      public testing::WithParamInterface<IsCertainFormTypeTestData> {};

// Tests if a form consisting of fields with types specified in the first
// element of `GetParam()` is an email only form (expectation in second
// element).
TEST_P(IsEmailOnlyFormTest, ReturnsTrueIfIsEmailOnlyForm) {
  EXPECT_EQ(
      internal::IsEmailOnlyForm(*CreateFormStructure(std::get<0>(GetParam()))),
      std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsUtilsTest,
    IsEmailOnlyFormTest,
    testing::Values(
        // A form containing known, non-PWM field types is not considered an
        // email only form.
        IsCertainFormTypeTestData({NAME_FIRST, NAME_LAST, EMAIL_ADDRESS},
                                  false),
        // A form containing an email field and other field types that are
        // unknown or for PWM, is considered an email only form.
        IsCertainFormTypeTestData({UNKNOWN_TYPE, EMAIL_ADDRESS, PASSWORD},
                                  true),
        // A form containing multiple email fields is considered an email only
        // form.
        IsCertainFormTypeTestData({EMAIL_ADDRESS, EMAIL_ADDRESS}, true)));

// Test fixture for testing `internal::IsPostalAddressForm()`.
class IsPostalAddressFormTest
    : public AutofillMetricsUtilsTest,
      public testing::WithParamInterface<IsCertainFormTypeTestData> {};

// Tests if a form consisting of fields with types specified in the first
// element of `GetParam()` is a postal address form (expectation in second
// element).
TEST_P(IsPostalAddressFormTest, ReturnsTrueIfIsPostalAddressForm) {
  EXPECT_EQ(internal::IsPostalAddressForm(
                *CreateFormStructure(std::get<0>(GetParam()))),
            std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsUtilsTest,
    IsPostalAddressFormTest,
    testing::Values(
        // A form containing two fields or less is not considered a postal
        // address form.
        IsCertainFormTypeTestData({ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2},
                                  false),
        // A store locator form (consisting of exactly three fields with types
        // city, state and zip) is not considered a postal address form.
        IsCertainFormTypeTestData({ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                   ADDRESS_HOME_ZIP},
                                  false),
        // Even in combination with another non-`FieldTypeGroup::kAddress` field
        // (e.g. first name), the store locator form field types are not
        // considered a postal address form.
        IsCertainFormTypeTestData({NAME_FIRST, ADDRESS_HOME_CITY,
                                   ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP},
                                  false),
        // A country field does not count towards the form being a postal
        // address form.
        IsCertainFormTypeTestData({ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                   ADDRESS_HOME_COUNTRY},
                                  false),
        // Otherwise, a form containing three address fields is considered a
        // postal address form.
        IsCertainFormTypeTestData({ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                                   ADDRESS_HOME_CITY},
                                  true)));

// Given a form consisting of fields of types `DenseSet<FieldType>`, expect
// `DenseSet<FormTypeNameForLogging>` to be returned from `Get{Address,
// CreditCard}FormTypesForLogging()`.
using FormTypesForLoggingTestData =
    std::tuple<DenseSet<FieldType>, DenseSet<FormTypeNameForLogging>>;

// Test fixture for testing `GetAddressFormTypesForLogging()`.
class AddressFormTypesForLoggingTest
    : public AutofillMetricsUtilsTest,
      public testing::WithParamInterface<FormTypesForLoggingTestData> {};

// Tests that `GetAddressFormTypesForLogging()` returns appropriate form types
// (second element of the parameter) for a form with given field types (first
// element of the parameter).
TEST_P(AddressFormTypesForLoggingTest,
       GetAddressFormTypesForLoggingReturnsAddressFormTypes) {
  EXPECT_EQ(GetAddressFormTypesForLogging(
                *CreateFormStructure(std::get<0>(GetParam()))),
            std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsUtilsTest,
    AddressFormTypesForLoggingTest,
    testing::Values(
        // An address form.
        FormTypesForLoggingTestData({NAME_FIRST, NAME_LAST, EMAIL_ADDRESS},
                                    {FormTypeNameForLogging::kAddressForm}),
        // An email-only form is also an address form.
        FormTypesForLoggingTestData({EMAIL_ADDRESS},
                                    {FormTypeNameForLogging::kAddressForm,
                                     FormTypeNameForLogging::kEmailOnlyForm}),
        // A postal address form is also an address form.
        FormTypesForLoggingTestData(
            {NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY,
             ADDRESS_HOME_ZIP},
            {FormTypeNameForLogging::kAddressForm,
             FormTypeNameForLogging::kPostalAddressForm}),
        // A credit card form has no address form types.
        FormTypesForLoggingTestData({CREDIT_CARD_NUMBER}, {}),
        // A form containing an address field and a credit card field, has one
        // address form type.
        FormTypesForLoggingTestData({CREDIT_CARD_NUMBER, ADDRESS_HOME_LINE1},
                                    {FormTypeNameForLogging::kAddressForm})));

// Test fixture for testing `GetCreditCardFormTypesForLogging()`.
class CreditCardFormTypesForLoggingTest
    : public AutofillMetricsUtilsTest,
      public testing::WithParamInterface<FormTypesForLoggingTestData> {};

// Tests that `GetCreditCardFormTypesForLogging()` returns appropriate form
// types (second element of the parameter) for a form with given field types
// (first element of the parameter).
TEST_P(CreditCardFormTypesForLoggingTest,
       GetCreditCardFormTypesForLoggingReturnsCreditCardFormTypes) {
  EXPECT_EQ(GetCreditCardFormTypesForLogging(
                *CreateFormStructure(std::get<0>(GetParam()))),
            std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsUtilsTest,
    CreditCardFormTypesForLoggingTest,
    testing::Values(
        // A credit card form.
        FormTypesForLoggingTestData({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                     CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     CREDIT_CARD_VERIFICATION_CODE},
                                    {FormTypeNameForLogging::kCreditCardForm}),
        // A standalone CVC form is not considered a credit card form.
        FormTypesForLoggingTestData(
            {CREDIT_CARD_STANDALONE_VERIFICATION_CODE},
            {FormTypeNameForLogging::kStandaloneCvcForm}),
        // A standalone CVC form (with legacy CREDIT_CARD_VERIFICATION_CODE
        // field) is not considered a credit card form.
        FormTypesForLoggingTestData(
            {CREDIT_CARD_VERIFICATION_CODE},
            {FormTypeNameForLogging::kStandaloneCvcForm}),
        // A single field address form has no credit card form types.
        FormTypesForLoggingTestData({NAME_FIRST}, {}),
        // A form containing an address field and a credit card field, has one
        // credit card form type.
        FormTypesForLoggingTestData({CREDIT_CARD_NUMBER, ADDRESS_HOME_LINE1},
                                    {FormTypeNameForLogging::kCreditCardForm}),
        // A form containing an address field and a cvc field, has one credit
        // card form type.
        FormTypesForLoggingTestData(
            {CREDIT_CARD_VERIFICATION_CODE, ADDRESS_HOME_LINE1},
            {FormTypeNameForLogging::kCreditCardForm})));

// Test fixture for testing `GetFormTypesForLogging()`.
class FormTypesForLoggingTest
    : public AutofillMetricsUtilsTest,
      public testing::WithParamInterface<FormTypesForLoggingTestData> {};

// Tests that `GetFormTypesForLogging()` returns appropriate form
// types (second element of the parameter) for a form with given field types
// (first element of the parameter).
TEST_P(FormTypesForLoggingTest,
       GetFormTypesForLoggingReturnsAppropriateFormTypes) {
  EXPECT_EQ(
      GetFormTypesForLogging(*CreateFormStructure(std::get<0>(GetParam()))),
      std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillMetricsUtilsTest,
    FormTypesForLoggingTest,
    testing::Values(
        // An address form.
        FormTypesForLoggingTestData({NAME_FIRST, NAME_LAST, EMAIL_ADDRESS},
                                    {FormTypeNameForLogging::kAddressForm}),
        // An email-only form is also an address form.
        FormTypesForLoggingTestData({EMAIL_ADDRESS},
                                    {FormTypeNameForLogging::kAddressForm,
                                     FormTypeNameForLogging::kEmailOnlyForm}),
        // A postal address form is also an address form.
        FormTypesForLoggingTestData(
            {NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY,
             ADDRESS_HOME_ZIP},
            {FormTypeNameForLogging::kAddressForm,
             FormTypeNameForLogging::kPostalAddressForm}),
        // A credit card form.
        FormTypesForLoggingTestData({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                     CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     CREDIT_CARD_VERIFICATION_CODE},
                                    {FormTypeNameForLogging::kCreditCardForm}),
        // A standalone CVC form is not considered a credit card form.
        FormTypesForLoggingTestData(
            {CREDIT_CARD_STANDALONE_VERIFICATION_CODE},
            {FormTypeNameForLogging::kStandaloneCvcForm}),
        // A form containing an address field and a credit card field, is both
        // an address and a credit card form.
        FormTypesForLoggingTestData({CREDIT_CARD_NUMBER, ADDRESS_HOME_LINE1},
                                    {FormTypeNameForLogging::kCreditCardForm,
                                     FormTypeNameForLogging::kAddressForm})));

}  // namespace

}  // namespace autofill::autofill_metrics
