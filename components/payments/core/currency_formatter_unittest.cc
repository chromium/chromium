// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/currency_formatter.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

struct CurrencyTestCase {
  CurrencyTestCase(const char* amount,
                   const char* currency_code,
                   const char* locale_name,
                   const std::string& expected_amount,
                   const char* expected_currency_code)
      : amount(amount),
        currency_code(currency_code),
        locale_name(locale_name),
        expected_amount(expected_amount),
        expected_currency_code(expected_currency_code) {}
  ~CurrencyTestCase() = default;

  const char* const amount;
  const char* const currency_code;
  const char* const locale_name;
  const std::string expected_amount;
  const char* const expected_currency_code;
};

class PaymentsCurrencyFormatterTest
    : public testing::TestWithParam<CurrencyTestCase> {};

TEST_P(PaymentsCurrencyFormatterTest, IsValidCurrencyFormat) {
  CurrencyFormatter formatter(GetParam().currency_code, GetParam().locale_name);
  std::u16string actual_output = formatter.Format(GetParam().amount);

  // Convenience so the test cases can use regular spaces.
  const std::u16string kSpace(u" ");
  const std::u16string kNonBreakingSpace(u"\u00a0");
  const std::u16string kNarrowNonBreakingSpace(u"\u202f");
  base::ReplaceChars(actual_output, kNonBreakingSpace, kSpace, &actual_output);
  base::ReplaceChars(actual_output, kNarrowNonBreakingSpace, kSpace,
                     &actual_output);
  std::u16string expected_output =
      base::UTF8ToUTF16(GetParam().expected_amount);

  EXPECT_EQ(expected_output, actual_output)
      << "Failed to convert " << GetParam().amount << " ("
      << GetParam().currency_code << ") in " << GetParam().locale_name;
  EXPECT_EQ(GetParam().expected_currency_code,
            formatter.formatted_currency_code());
}

INSTANTIATE_TEST_SUITE_P(
    CurrencyAmounts,
    PaymentsCurrencyFormatterTest,
    testing::Values(
        CurrencyTestCase("55.00", "USD", "en_US", "$55.00", "USD"),
        CurrencyTestCase("55.00", "USD", "en_CA", "$55.00", "USD"),
        CurrencyTestCase("55.00", "USD", "fr_CA", "55,00 $", "USD"),
        CurrencyTestCase("55.00", "USD", "fr_FR", "55,00 $", "USD"),
        CurrencyTestCase("1234", "USD", "fr_FR", "1 234,00 $", "USD"),
        // Known oddity about the en_AU formatting in ICU. It will strip the
        // currency symbol in non-AUD currencies. Useful to document in tests.
        // See crbug.com/739812.
        CurrencyTestCase("55.00", "AUD", "en_AU", "$55.00", "AUD"),
        CurrencyTestCase("55.00", "USD", "en_AU", "55.00", "USD"),
        CurrencyTestCase("55.00", "CAD", "en_AU", "55.00", "CAD"),
        CurrencyTestCase("55.00", "JPY", "en_AU", "55", "JPY"),
        CurrencyTestCase("-55.00", "USD", "en_AU", "-55.00", "USD"),

        CurrencyTestCase("55.5", "USD", "en_US", "$55.50", "USD"),
        CurrencyTestCase("55", "USD", "en_US", "$55.00", "USD"),
        CurrencyTestCase("123", "USD", "en_US", "$123.00", "USD"),
        CurrencyTestCase("1234", "USD", "en_US", "$1,234.00", "USD"),
        CurrencyTestCase("0.1234", "USD", "en_US", "$0.1234", "USD"),

        CurrencyTestCase("55.00", "EUR", "en_US", "€55.00", "EUR"),
        CurrencyTestCase("55.00", "EUR", "fr_CA", "55,00 €", "EUR"),
        CurrencyTestCase("55.00", "EUR", "fr_FR", "55,00 €", "EUR"),

        CurrencyTestCase("55.00", "CAD", "en_US", "$55.00", "CAD"),
        CurrencyTestCase("55.00", "CAD", "en_CA", "$55.00", "CAD"),
        CurrencyTestCase("55.00", "CAD", "fr_CA", "55,00 $", "CAD"),
        CurrencyTestCase("55.00", "CAD", "fr_FR", "55,00 $", "CAD"),

        CurrencyTestCase("55.00", "AUD", "en_US", "$55.00", "AUD"),
        CurrencyTestCase("55.00", "AUD", "en_CA", "$55.00", "AUD"),
        CurrencyTestCase("55.00", "AUD", "fr_CA", "55,00 $", "AUD"),
        CurrencyTestCase("55.00", "AUD", "fr_FR", "55,00 $", "AUD"),

        CurrencyTestCase("55.00", "BRL", "en_US", "R$55.00", "BRL"),
        CurrencyTestCase("55.00", "BRL", "fr_CA", "55,00 R$", "BRL"),
        CurrencyTestCase("55.00", "BRL", "pt_BR", "R$ 55,00", "BRL"),

        CurrencyTestCase("55.00", "RUB", "en_US", "55.00", "RUB"),
        CurrencyTestCase("55.00", "RUB", "fr_CA", "55,00", "RUB"),
        CurrencyTestCase("55.00", "RUB", "ru_RU", "55,00 ₽", "RUB"),

        CurrencyTestCase("55", "JPY", "ja_JP", "￥55", "JPY"),
        CurrencyTestCase("55.0", "JPY", "ja_JP", "￥55", "JPY"),
        CurrencyTestCase("55.00", "JPY", "ja_JP", "￥55", "JPY"),
        CurrencyTestCase("55.12", "JPY", "ja_JP", "￥55.12", "JPY"),
        CurrencyTestCase("55.49", "JPY", "ja_JP", "￥55.49", "JPY"),
        CurrencyTestCase("55.50", "JPY", "ja_JP", "￥55.5", "JPY"),
        CurrencyTestCase("55.9999", "JPY", "ja_JP", "￥55.9999", "JPY"),

        // Unofficial ISO 4217 currency code.
        CurrencyTestCase("55.00", "BTC", "en_US", "55.00", "BTC"),
        CurrencyTestCase("-0.0000000001",
                         "BTC",
                         "en_US",
                         "-0.0000000001",
                         "BTC"),
        CurrencyTestCase("-55.00", "BTC", "fr_FR", "-55,00", "BTC"),

        // Any string of at most 2048 characters can be a valid currency code.
        CurrencyTestCase("55.00", "", "en_US", "55.00", ""),
        CurrencyTestCase("55,00", "", "fr_CA", "55,00", ""),
        CurrencyTestCase("55,00", "", "fr-CA", "55,00", ""),
        CurrencyTestCase("55.00",
                         "ABCDEF",
                         "en_US",
                         "55.00",
                         "ABCDE\xE2\x80\xA6"),

        // Edge cases.
        CurrencyTestCase("", "", "", "", ""),
        CurrencyTestCase("-1", "", "", "-1.00", ""),
        CurrencyTestCase("-1.1255", "", "", "-1.1255", ""),

        // Handles big numbers.
        CurrencyTestCase(
            "123456789012345678901234567890.123456789012345678901234567890",
            "USD",
            "fr_FR",
            "123 456 789 012 345 678 901 234 567 890,123456789 $",
            "USD")));

}  // namespace
}  // namespace payments
