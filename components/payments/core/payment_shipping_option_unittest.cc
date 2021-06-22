// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_shipping_option.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentShippingOption from a
// dictionary.
TEST(PaymentRequestTest, PaymentShippingOptionFromDictionaryValueSuccess) {
  PaymentShippingOption expected;
  expected.id = "123";
  expected.label = "Ground Shipping";
  expected.amount->currency = "BRL";
  expected.amount->value = "4,000.32";
  expected.selected = true;

  base::DictionaryValue shipping_option_dict;
  shipping_option_dict.SetString("id", "123");
  shipping_option_dict.SetString("label", "Ground Shipping");
  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "BRL");
  amount_dict.SetString("value", "4,000.32");
  shipping_option_dict.SetKey("amount", std::move(amount_dict));
  shipping_option_dict.SetBoolean("selected", true);

  PaymentShippingOption actual;
  EXPECT_TRUE(actual.FromDictionaryValue(shipping_option_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentShippingOption from a
// dictionary.
TEST(PaymentRequestTest, PaymentShippingOptionFromDictionaryValueFailure) {
  PaymentShippingOption expected;
  expected.id = "123";
  expected.label = "Ground Shipping";
  expected.amount->currency = "BRL";
  expected.amount->value = "4,000.32";
  expected.selected = true;

  PaymentShippingOption actual;
  base::DictionaryValue shipping_option_dict;

  // Id, Label, and amount are required.
  shipping_option_dict.SetString("id", "123");
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));

  shipping_option_dict.SetString("label", "Ground Shipping");
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));

  // Id must be a string.
  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "BRL");
  amount_dict.SetString("value", "4,000.32");
  shipping_option_dict.SetKey("amount", std::move(amount_dict));
  shipping_option_dict.SetInteger("id", 123);
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));

  // Label must be a string.
  shipping_option_dict.SetString("id", "123");
  shipping_option_dict.SetInteger("label", 123);
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));
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

}  // namespace payments
