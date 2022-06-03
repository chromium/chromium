// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_item.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromValueSuccess) {
  PaymentItem expected;
  expected.label = "Payment Total";
  expected.amount->currency = "NZD";
  expected.amount->value = "2,242,093.00";

  base::Value item_dict(base::Value::Type::DICTIONARY);
  item_dict.SetStringKey("label", "Payment Total");
  base::Value amount_dict(base::Value::Type::DICTIONARY);
  amount_dict.SetStringKey("currency", "NZD");
  amount_dict.SetStringKey("value", "2,242,093.00");
  item_dict.SetKey("amount", std::move(amount_dict));

  PaymentItem actual;
  EXPECT_TRUE(actual.FromValue(item_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromValueFailure) {
  PaymentItem actual;

  // Non-dictionary input fails cleanly.
  EXPECT_FALSE(actual.FromValue(base::Value("hi")));

  // Both a label and an amount are required.
  base::Value item_dict(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(actual.FromValue(item_dict));

  item_dict.SetStringKey("label", "Payment Total");
  EXPECT_FALSE(actual.FromValue(item_dict));

  // Even with both present, the label must be a string.
  base::Value amount_dict(base::Value::Type::DICTIONARY);
  amount_dict.SetStringKey("currency", "NZD");
  amount_dict.SetStringKey("value", "2,242,093.00");
  item_dict.SetKey("amount", std::move(amount_dict));
  item_dict.SetIntKey("label", 42);
  EXPECT_FALSE(actual.FromValue(item_dict));

  // Test with invalid mount.
  item_dict.SetStringKey("label", "Some label");
  item_dict.FindKey("amount")->RemoveKey("currency");
  EXPECT_FALSE(actual.FromValue(item_dict));
}

// Tests that two payment item objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentItemEquality) {
  PaymentItem item1;
  PaymentItem item2;
  EXPECT_EQ(item1, item2);

  item1.label = "Subtotal";
  EXPECT_NE(item1, item2);
  item2.label = "Total";
  EXPECT_NE(item1, item2);
  item2.label = "Subtotal";
  EXPECT_EQ(item1, item2);

  item1.amount->value = "104.34";
  EXPECT_NE(item1, item2);
  item2.amount->value = "104";
  EXPECT_NE(item1, item2);
  item2.amount->value = "104.34";
  EXPECT_EQ(item1, item2);

  item1.pending = true;
  EXPECT_NE(item1, item2);
  item2.pending = true;
  EXPECT_EQ(item1, item2);
}

// Tests that serializing a default PaymentItem yields the expected result.
TEST(PaymentRequestTest, EmptyPaymentItemDictionary) {
  base::Value expected_value(base::Value::Type::DICTIONARY);

  expected_value.SetStringKey("label", "");
  base::Value amount_dict(base::Value::Type::DICTIONARY);
  amount_dict.SetStringKey("currency", "");
  amount_dict.SetStringKey("value", "");
  expected_value.SetKey("amount", std::move(amount_dict));
  expected_value.SetBoolKey("pending", false);

  PaymentItem payment_item;
  EXPECT_EQ(expected_value, payment_item.ToValue());
}

// Tests that serializing a populated PaymentItem yields the expected result.
TEST(PaymentRequestTest, PopulatedPaymentItemDictionary) {
  base::Value expected_value(base::Value::Type::DICTIONARY);

  expected_value.SetStringKey("label", "Payment Total");
  base::Value amount_dict(base::Value::Type::DICTIONARY);
  amount_dict.SetStringKey("currency", "NZD");
  amount_dict.SetStringKey("value", "2,242,093.00");
  expected_value.SetKey("amount", std::move(amount_dict));
  expected_value.SetBoolKey("pending", true);

  PaymentItem payment_item;
  payment_item.label = "Payment Total";
  payment_item.amount->currency = "NZD";
  payment_item.amount->value = "2,242,093.00";
  payment_item.pending = true;

  EXPECT_EQ(expected_value, payment_item.ToValue());
}

}  // namespace payments
