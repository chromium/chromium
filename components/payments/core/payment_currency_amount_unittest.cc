// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_currency_amount.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentCurrencyAmount from a
// dictionary.
TEST(PaymentRequestTest, PaymentCurrencyAmountFromValueDictSuccess) {
  mojom::PaymentCurrencyAmount expected;
  expected.currency = "AUD";
  expected.value = "-438.23";

  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "AUD");
  amount_dict.Set("value", "-438.23");

  mojom::PaymentCurrencyAmount actual;
  EXPECT_TRUE(PaymentCurrencyAmountFromValueDict(amount_dict, &actual));

  EXPECT_TRUE(expected.Equals(actual));

  EXPECT_TRUE(PaymentCurrencyAmountFromValueDict(amount_dict, &actual));
  EXPECT_TRUE(expected.Equals(actual));
}

// Tests the failure case when populating a PaymentCurrencyAmount from a
// dictionary.
TEST(PaymentRequestTest, PaymentCurrencyAmountFromValueDictFailure) {
  mojom::PaymentCurrencyAmount actual;

  // Both a currency and a value are required.
  base::Value::Dict amount_dict;
  EXPECT_FALSE(PaymentCurrencyAmountFromValueDict(amount_dict, &actual));

  // Both values must be strings.
  amount_dict.Set("currency", 842);
  amount_dict.Set("value", "-438.23");
  EXPECT_FALSE(PaymentCurrencyAmountFromValueDict(amount_dict, &actual));

  amount_dict.Set("currency", "NZD");
  amount_dict.Set("value", -438.23);
  EXPECT_FALSE(PaymentCurrencyAmountFromValueDict(amount_dict, &actual));
}

// Tests that two currency amount objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentCurrencyAmountEquality) {
  mojom::PaymentCurrencyAmount currency_amount1;
  mojom::PaymentCurrencyAmount currency_amount2;
  EXPECT_TRUE(currency_amount1.Equals(currency_amount2));

  currency_amount1.currency = "HKD";
  EXPECT_FALSE(currency_amount1.Equals(currency_amount2));
  currency_amount2.currency = "USD";
  EXPECT_FALSE(currency_amount1.Equals(currency_amount2));
  currency_amount2.currency = "HKD";
  EXPECT_TRUE(currency_amount1.Equals(currency_amount2));

  currency_amount1.value = "49.89";
  EXPECT_FALSE(currency_amount1.Equals(currency_amount2));
  currency_amount2.value = "49.99";
  EXPECT_FALSE(currency_amount1.Equals(currency_amount2));
  currency_amount2.value = "49.89";
  EXPECT_TRUE(currency_amount1.Equals(currency_amount2));
}

// Tests that serializing a default PaymentCurrencyAmount yields the expected
// result.
TEST(PaymentRequestTest, EmptyPaymentCurrencyAmountDictionary) {
  base::Value::Dict expected_value;

  expected_value.Set("currency", "");
  expected_value.Set("value", "");

  mojom::PaymentCurrencyAmount payment_currency_amount;
  EXPECT_EQ(expected_value,
            PaymentCurrencyAmountToValueDict(payment_currency_amount));
}

// Tests that serializing a populated PaymentCurrencyAmount yields the expected
// result.
TEST(PaymentRequestTest, PopulatedCurrencyAmountDictionary) {
  base::Value::Dict expected_value;

  expected_value.Set("currency", "AUD");
  expected_value.Set("value", "-438.23");

  mojom::PaymentCurrencyAmount payment_currency_amount;
  payment_currency_amount.currency = "AUD";
  payment_currency_amount.value = "-438.23";

  EXPECT_EQ(expected_value,
            PaymentCurrencyAmountToValueDict(payment_currency_amount));
}

}  // namespace payments
