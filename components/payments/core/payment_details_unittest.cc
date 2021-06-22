// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromDictionaryValueSuccess) {
  PaymentDetails expected;
  expected.error = "Error in details";

  base::DictionaryValue details_dict;
  details_dict.SetString("error", "Error in details");
  PaymentDetails actual;
  EXPECT_TRUE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  expected.total = std::make_unique<PaymentItem>();
  expected.total->label = "TOTAL";
  expected.total->amount->currency = "GBP";
  expected.total->amount->value = "6.66";

  base::DictionaryValue total_dict;
  total_dict.SetString("label", "TOTAL");
  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "GBP");
  amount_dict.SetString("value", "6.66");
  total_dict.SetKey("amount", std::move(amount_dict));
  details_dict.SetKey("total", std::move(total_dict));

  EXPECT_TRUE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  EXPECT_TRUE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/true));
  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromDictionaryValueFailure) {
  PaymentDetails expected;
  expected.total = std::make_unique<PaymentItem>();
  expected.total->label = "TOTAL";
  expected.total->amount->currency = "GBP";
  expected.total->amount->value = "6.66";
  expected.error = "Error in details";

  base::DictionaryValue details_dict;
  details_dict.SetString("error", "Error in details");

  PaymentDetails actual;
  EXPECT_FALSE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/true));
}

// Tests that two payment details objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentDetailsEquality) {
  PaymentDetails details1;
  PaymentDetails details2;
  EXPECT_EQ(details1, details2);

  details1.id = "12345";
  EXPECT_NE(details1, details2);
  details2.id = "54321";
  EXPECT_NE(details1, details2);
  details2.id = details1.id;
  EXPECT_EQ(details1, details2);

  details1.total = std::make_unique<PaymentItem>();
  details1.total->label = "Total";
  EXPECT_NE(details1, details2);
  details2.total = std::make_unique<PaymentItem>();
  details2.total->label = "Shipping";
  EXPECT_NE(details1, details2);
  details2.total->label = "Total";
  EXPECT_EQ(details1, details2);

  details1.error = "Foo";
  EXPECT_NE(details1, details2);
  details2.error = "Bar";
  EXPECT_NE(details1, details2);
  details2.error = "Foo";
  EXPECT_EQ(details1, details2);

  PaymentItem payment_item;
  payment_item.label = "Tax";
  std::vector<PaymentItem> display_items1;
  display_items1.push_back(payment_item);
  details1.display_items = display_items1;
  EXPECT_NE(details1, details2);
  std::vector<PaymentItem> display_items2;
  display_items2.push_back(payment_item);
  display_items2.push_back(payment_item);
  details2.display_items = display_items2;
  EXPECT_NE(details1, details2);
  details2.display_items = display_items1;
  EXPECT_EQ(details1, details2);

  PaymentShippingOption shipping_option;
  shipping_option.label = "Overnight";
  std::vector<PaymentShippingOption> shipping_options1;
  shipping_options1.push_back(shipping_option);
  details1.shipping_options = shipping_options1;
  EXPECT_NE(details1, details2);
  std::vector<PaymentShippingOption> shipping_options2;
  shipping_options2.push_back(shipping_option);
  shipping_options2.push_back(shipping_option);
  details2.shipping_options = shipping_options2;
  EXPECT_NE(details1, details2);
  details2.shipping_options = shipping_options1;
  EXPECT_EQ(details1, details2);

  PaymentDetailsModifier details_modifier;

  details_modifier.total = std::make_unique<PaymentItem>();
  details_modifier.total->label = "Total";
  details1.modifiers.push_back(details_modifier);
  EXPECT_NE(details1, details2);
  details2.modifiers.push_back(details_modifier);
  EXPECT_EQ(details1, details2);
}

}  // namespace payments
