// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payments_validators.h"

#include <ostream>  // NOLINT

#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

struct CurrencyCodeTestCase {
  CurrencyCodeTestCase(const char* code, bool expected_valid)
      : code(code), expected_valid(expected_valid) {}
  ~CurrencyCodeTestCase() {}

  const char* code;
  bool expected_valid;
};

class PaymentsCurrencyValidatorTest
    : public testing::TestWithParam<CurrencyCodeTestCase> {};

const char* LongString2049() {
  static char long_string[2050];
  for (int i = 0; i < 2049; i++)
    long_string[i] = 'a';
  long_string[2049] = '\0';
  return long_string;
}

TEST_P(PaymentsCurrencyValidatorTest, IsValidCurrencyCodeFormat) {
  std::string error_message;
  EXPECT_EQ(GetParam().expected_valid,
            payments::PaymentsValidators::IsValidCurrencyCodeFormat(
                GetParam().code, &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            payments::PaymentsValidators::IsValidCurrencyCodeFormat(
                GetParam().code, nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    CurrencyCodes,
    PaymentsCurrencyValidatorTest,
    testing::Values(
        // The most common identifiers are three-letter alphabetic codes as
        // defined by [ISO4217] (for example, "USD" for US Dollars).
        // |system| is a URL that indicates the currency system that the
        // currency identifier belongs to. By default,
        // the value is urn:iso:std:iso:4217 indicating that currency is defined
        // by [[ISO4217]], however any string of at most 2048
        // characters is considered valid in other currencySystem. Returns false
        // if currency |code| is too long (greater than 2048).
        CurrencyCodeTestCase("USD", true),
        CurrencyCodeTestCase("US1", false),
        CurrencyCodeTestCase("US", false),
        CurrencyCodeTestCase("USDO", false),
        CurrencyCodeTestCase("usd", false),
        CurrencyCodeTestCase("ANYSTRING", false),
        CurrencyCodeTestCase("", false),
        CurrencyCodeTestCase(LongString2049(), false)));

struct TestCase {
  TestCase(const char* input, bool expected_valid)
      : input(input), expected_valid(expected_valid) {}
  ~TestCase() {}

  const char* input;
  bool expected_valid;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "'" << test_case.input << "' is expected to be "
      << (test_case.expected_valid ? "valid" : "invalid");
  return out;
}

class PaymentsAmountValidatorTest : public testing::TestWithParam<TestCase> {};

TEST_P(PaymentsAmountValidatorTest, IsValidAmountFormat) {
  std::string error_message;
  EXPECT_EQ(GetParam().expected_valid,
            payments::PaymentsValidators::IsValidAmountFormat(GetParam().input,
                                                              &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            payments::PaymentsValidators::IsValidAmountFormat(GetParam().input,
                                                              nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    Amounts,
    PaymentsAmountValidatorTest,
    testing::Values(TestCase("0", true),
                    TestCase("-0", true),
                    TestCase("1", true),
                    TestCase("10", true),
                    TestCase("-3", true),
                    TestCase("10.99", true),
                    TestCase("-3.00", true),
                    TestCase("01234567890123456789.0123456789", true),
                    TestCase("01234567890123456789012345678.9", true),
                    TestCase("012345678901234567890123456789", true),
                    TestCase("-01234567890123456789.0123456789", true),
                    TestCase("-01234567890123456789012345678.9", true),
                    TestCase("-012345678901234567890123456789", true),
                    // Invalid amount formats
                    TestCase("", false),
                    TestCase("-", false),
                    TestCase("notdigits", false),
                    TestCase("ALSONOTDIGITS", false),
                    TestCase("10.", false),
                    TestCase(".99", false),
                    TestCase("-10.", false),
                    TestCase("-.99", false),
                    TestCase("10-", false),
                    TestCase("1-0", false),
                    TestCase("1.0.0", false),
                    TestCase("1/3", false)));

class PaymentsRegionValidatorTest : public testing::TestWithParam<TestCase> {};

TEST_P(PaymentsRegionValidatorTest, IsValidCountryCodeFormat) {
  std::string error_message;
  EXPECT_EQ(GetParam().expected_valid,
            payments::PaymentsValidators::IsValidCountryCodeFormat(
                GetParam().input, &error_message))
      << error_message;
  EXPECT_EQ(GetParam().expected_valid, error_message.empty()) << error_message;

  EXPECT_EQ(GetParam().expected_valid,
            payments::PaymentsValidators::IsValidCountryCodeFormat(
                GetParam().input, nullptr));
}

INSTANTIATE_TEST_SUITE_P(CountryCodes,
                         PaymentsRegionValidatorTest,
                         testing::Values(TestCase("US", true),
                                         // Invalid country code formats
                                         TestCase("U1", false),
                                         TestCase("U", false),
                                         TestCase("us", false),
                                         TestCase("USA", false),
                                         TestCase("", false)));

struct ValidationErrorsTestCase {
  explicit ValidationErrorsTestCase(bool expected_valid)
      : expected_valid(expected_valid) {}

  const char* m_error = "";
  const char* m_payer_email = "";
  const char* m_payer_name = "";
  const char* m_payer_phone = "";
  const char* m_shipping_address_address_line = "";
  const char* m_shipping_address_city = "";
  const char* m_shipping_address_country = "";
  const char* m_shipping_address_dependent_locality = "";
  const char* m_shipping_address_organization = "";
  const char* m_shipping_address_phone = "";
  const char* m_shipping_address_postal_code = "";
  const char* m_shipping_address_recipient = "";
  const char* m_shipping_address_region = "";
  const char* m_shipping_address_sorting_code = "";
  bool expected_valid;
};

#define VALIDATION_ERRORS_TEST_CASE(field, value, expected_valid) \
  ([]() {                                                         \
    ValidationErrorsTestCase test_case(expected_valid);           \
    test_case.m_##field = value;                                  \
    return test_case;                                             \
  })()

mojom::PaymentValidationErrorsPtr toPaymentValidationErrors(
    ValidationErrorsTestCase test_case) {
  mojom::PaymentValidationErrorsPtr errors =
      mojom::PaymentValidationErrors::New();

  mojom::PayerErrorsPtr payer = mojom::PayerErrors::New();
  payer->email = test_case.m_payer_email;
  payer->name = test_case.m_payer_name;
  payer->phone = test_case.m_payer_phone;

  mojom::AddressErrorsPtr shipping_address = mojom::AddressErrors::New();
  shipping_address->address_line = test_case.m_shipping_address_address_line;
  shipping_address->city = test_case.m_shipping_address_city;
  shipping_address->country = test_case.m_shipping_address_country;
  shipping_address->dependent_locality =
      test_case.m_shipping_address_dependent_locality;
  shipping_address->organization = test_case.m_shipping_address_organization;
  shipping_address->phone = test_case.m_shipping_address_phone;
  shipping_address->postal_code = test_case.m_shipping_address_postal_code;
  shipping_address->recipient = test_case.m_shipping_address_recipient;
  shipping_address->region = test_case.m_shipping_address_region;
  shipping_address->sorting_code = test_case.m_shipping_address_sorting_code;

  errors->error = test_case.m_error;
  errors->payer = std::move(payer);
  errors->shipping_address = std::move(shipping_address);

  return errors;
}

class PaymentsErrorMessageValidatorTest
    : public testing::TestWithParam<ValidationErrorsTestCase> {};

TEST_P(PaymentsErrorMessageValidatorTest,
       IsValidPaymentValidationErrorsFormat) {
  mojom::PaymentValidationErrorsPtr errors =
      toPaymentValidationErrors(GetParam());

  std::string error_message;
  EXPECT_EQ(GetParam().expected_valid,
            PaymentsValidators::IsValidPaymentValidationErrorsFormat(
                errors, &error_message))
      << error_message;
}

INSTANTIATE_TEST_SUITE_P(
    PaymentValidationErrorss,
    PaymentsErrorMessageValidatorTest,
    testing::Values(
        VALIDATION_ERRORS_TEST_CASE(error, "test", true),
        VALIDATION_ERRORS_TEST_CASE(payer_email, "test", true),
        VALIDATION_ERRORS_TEST_CASE(payer_name, "test", true),
        VALIDATION_ERRORS_TEST_CASE(payer_phone, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_address_line,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_country, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_dependent_locality,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_organization,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_phone, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_postal_code, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_recipient, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_region, "test", true),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_sorting_code,
                                    "test",
                                    true),
        VALIDATION_ERRORS_TEST_CASE(error, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(payer_email, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(payer_name, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(payer_phone, LongString2049(), false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_address_line,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_city,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_country,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_dependent_locality,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_organization,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_phone,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_postal_code,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_recipient,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_region,
                                    LongString2049(),
                                    false),
        VALIDATION_ERRORS_TEST_CASE(shipping_address_sorting_code,
                                    LongString2049(),
                                    false)));

}  // namespace
}  // namespace payments
