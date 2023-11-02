// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_shipping_option.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentShippingOption from a
// dictionary.
TEST(PaymentRequestTest, PaymentShippingOptionFromValueDictSuccess) {
  PaymentShippingOption expected;
  expected.id = "123";
  expected.label = "Ground Shipping";
  expected.amount->currency = "BRL";
  expected.amount->value = "4,000.32";
  expected.selected = true;

  base::Value::Dict shipping_option_dict;
  shipping_option_dict.Set("id", "123");
  shipping_option_dict.Set("label", "Ground Shipping");
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "BRL");
  amount_dict.Set("value", "4,000.32");
  shipping_option_dict.Set("amount", std::move(amount_dict));
  shipping_option_dict.Set("selected", true);

  PaymentShippingOption actual;
  EXPECT_TRUE(actual.FromValueDict(shipping_option_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentShippingOption from a
// dictionary.
TEST(PaymentRequestTest, PaymentShippingOptionFromValueDictFailure) {
  PaymentShippingOption expected;
  expected.id = "123";
  expected.label = "Ground Shipping";
  expected.amount->currency = "BRL";
  expected.amount->value = "4,000.32";
  expected.selected = true;

  PaymentShippingOption actual;

  base::Value::Dict shipping_option_dict;

  // Id, Label, and amount are required.
  shipping_option_dict.Set("id", "123");
  EXPECT_FALSE(actual.FromValueDict(shipping_option_dict));

  shipping_option_dict.Set("label", "Ground Shipping");
  EXPECT_FALSE(actual.FromValueDict(shipping_option_dict));

  // Id must be a string.
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "BRL");
  amount_dict.Set("value", "4,000.32");
  shipping_option_dict.Set("amount", std::move(amount_dict));
  shipping_option_dict.Set("id", 123);
  EXPECT_FALSE(actual.FromValueDict(shipping_option_dict));

  // Label must be a string.
  shipping_option_dict.Set("id", "123");
  shipping_option_dict.Set("label", 123);
  EXPECT_FALSE(actual.FromValueDict(shipping_option_dict));

  // Check for trouble with amount.
  shipping_option_dict.Set("label", "123");
  shipping_option_dict.Set("amount", "123.49 USD");
  EXPECT_FALSE(actual.FromValueDict(shipping_option_dict));

  base::Value::Dict bad_amount_dict;
  shipping_option_dict.Set("amount", std::move(bad_amount_dict));
  EXPECT_FALSE(actual.FromValueDict(shipping_option_dict));
}

// Tests that two shipping option objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentShippingOptionEquality) {
  PaymentShippingOption shipping_option1;
  PaymentShippingOption shipping_option2;
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.id = "a8df2";
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.id = "k42jk";
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.id = "a8df2";
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.label = "Overnight";
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.label = "Ground";
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.label = "Overnight";
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.amount->currency = "AUD";
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.amount->currency = "HKD";
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.amount->currency = "AUD";
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.selected = true;
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.selected = true;
  EXPECT_EQ(shipping_option1, shipping_option2);
}

TEST(PaymentRequestTest, Assign) {
  PaymentShippingOption shipping_option1, shipping_option2;

  shipping_option1.id = "abc";
  shipping_option1.label = "Ground";
  shipping_option1.amount.reset();

  // All this should be overwritten.
  shipping_option2.id = "def";
  shipping_option2.label = "Air";
  shipping_option2.amount = mojom::PaymentCurrencyAmount::New();
  shipping_option2.amount->currency = "USD";
  shipping_option2.amount->value = "50.0";

  shipping_option2 = shipping_option1;
  EXPECT_EQ(shipping_option2, shipping_option1);

  // Also test with amount.
  shipping_option1.amount = mojom::PaymentCurrencyAmount::New();
  shipping_option1.amount->currency = "CAD";
  shipping_option1.amount->value = "10.0";
  shipping_option2 = shipping_option1;
  EXPECT_EQ(shipping_option2, shipping_option1);
}

}  // namespace payments
