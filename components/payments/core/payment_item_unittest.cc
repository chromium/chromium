// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_item.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromValueDictSuccess) {
  PaymentItem expected;
  expected.label = "Payment Total";
  expected.amount->currency = "NZD";
  expected.amount->value = "2,242,093.00";

  base::Value::Dict item_dict;
  item_dict.Set("label", "Payment Total");
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "NZD");
  amount_dict.Set("value", "2,242,093.00");
  item_dict.Set("amount", std::move(amount_dict));

  PaymentItem actual;
  EXPECT_TRUE(actual.FromValueDict(item_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromValueDictFailure) {
  PaymentItem actual;

  // Both a label and an amount are required.
  base::Value::Dict item_dict;
  EXPECT_FALSE(actual.FromValueDict(item_dict));

  item_dict.Set("label", "Payment Total");
  EXPECT_FALSE(actual.FromValueDict(item_dict));

  // Even with both present, the label must be a string.
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "NZD");
  amount_dict.Set("value", "2,242,093.00");
  item_dict.Set("amount", std::move(amount_dict));
  item_dict.Set("label", 42);
  EXPECT_FALSE(actual.FromValueDict(item_dict));

  // Test with invalid mount.
  item_dict.Set("label", "Some label");
  item_dict.FindDict("amount")->Remove("currency");
  EXPECT_FALSE(actual.FromValueDict(item_dict));
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
  base::Value::Dict expected_value;

  expected_value.Set("label", "");
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "");
  amount_dict.Set("value", "");
  expected_value.Set("amount", std::move(amount_dict));
  expected_value.Set("pending", false);

  PaymentItem payment_item;
  EXPECT_EQ(expected_value, payment_item.ToValueDict());
}

// Tests that serializing a populated PaymentItem yields the expected result.
TEST(PaymentRequestTest, PopulatedPaymentItemDictionary) {
  base::Value::Dict expected_value;

  expected_value.Set("label", "Payment Total");
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "NZD");
  amount_dict.Set("value", "2,242,093.00");
  expected_value.Set("amount", std::move(amount_dict));
  expected_value.Set("pending", true);

  PaymentItem payment_item;
  payment_item.label = "Payment Total";
  payment_item.amount->currency = "NZD";
  payment_item.amount->value = "2,242,093.00";
  payment_item.pending = true;

  EXPECT_EQ(expected_value, payment_item.ToValueDict());
}

}  // namespace payments
