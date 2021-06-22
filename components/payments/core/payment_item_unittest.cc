// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_item.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromDictionaryValueSuccess) {
  PaymentItem expected;
  expected.label = "Payment Total";
  expected.amount->currency = "NZD";
  expected.amount->value = "2,242,093.00";

  base::DictionaryValue item_dict;
  item_dict.SetString("label", "Payment Total");
  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "NZD");
  amount_dict.SetString("value", "2,242,093.00");
  item_dict.SetKey("amount", std::move(amount_dict));

  PaymentItem actual;
  EXPECT_TRUE(actual.FromDictionaryValue(item_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromDictionaryValueFailure) {
  // Both a label and an amount are required.
  PaymentItem actual;
  base::DictionaryValue item_dict;
  EXPECT_FALSE(actual.FromDictionaryValue(item_dict));

  item_dict.SetString("label", "Payment Total");
  EXPECT_FALSE(actual.FromDictionaryValue(item_dict));

  // Even with both present, the label must be a string.
  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "NZD");
  amount_dict.SetString("value", "2,242,093.00");
  item_dict.SetKey("amount", std::move(amount_dict));
  item_dict.SetInteger("label", 42);
  EXPECT_FALSE(actual.FromDictionaryValue(item_dict));
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
  base::DictionaryValue expected_value;

  expected_value.SetString("label", "");
  std::unique_ptr<base::DictionaryValue> amount_dict =
      std::make_unique<base::DictionaryValue>();
  amount_dict->SetString("currency", "");
  amount_dict->SetString("value", "");
  expected_value.SetDictionary("amount", std::move(amount_dict));
  expected_value.SetBoolean("pending", false);

  PaymentItem payment_item;
  EXPECT_TRUE(expected_value.Equals(payment_item.ToDictionaryValue().get()));
}

// Tests that serializing a populated PaymentItem yields the expected result.
TEST(PaymentRequestTest, PopulatedPaymentItemDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("label", "Payment Total");
  std::unique_ptr<base::DictionaryValue> amount_dict =
      std::make_unique<base::DictionaryValue>();
  amount_dict->SetString("currency", "NZD");
  amount_dict->SetString("value", "2,242,093.00");
  expected_value.SetDictionary("amount", std::move(amount_dict));
  expected_value.SetBoolean("pending", true);

  PaymentItem payment_item;
  payment_item.label = "Payment Total";
  payment_item.amount->currency = "NZD";
  payment_item.amount->value = "2,242,093.00";
  payment_item.pending = true;

  EXPECT_TRUE(expected_value.Equals(payment_item.ToDictionaryValue().get()));
}

}  // namespace payments
