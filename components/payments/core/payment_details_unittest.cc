// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromValueSuccessDict) {
  PaymentDetails expected;
  expected.error = "Error in details";

  base::Value::Dict details_dict;
  details_dict.Set("error", "Error in details");
  PaymentDetails actual;
  EXPECT_TRUE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  expected.total = std::make_unique<PaymentItem>();
  expected.total->label = "TOTAL";
  expected.total->amount->currency = "GBP";
  expected.total->amount->value = "6.66";

  base::Value::Dict total_dict;
  total_dict.Set("label", "TOTAL");
  base::Value::Dict amount_dict;
  amount_dict.Set("currency", "GBP");
  amount_dict.Set("value", "6.66");
  total_dict.Set("amount", std::move(amount_dict));
  details_dict.Set("total", std::move(total_dict));

  PaymentItem display1;
  display1.label = "Handling";
  display1.amount->currency = "GBP";
  display1.amount->value = "1.23";
  display1.pending = true;
  expected.display_items.push_back(display1);

  base::Value::List display_items_list;
  display_items_list.Append(display1.ToValueDict());
  details_dict.Set("displayItems", std::move(display_items_list));

  PaymentShippingOption expect_shipping_option;
  expect_shipping_option.id = "Post office";
  expect_shipping_option.label = "Post office, one-week ground";
  expect_shipping_option.amount = mojom::PaymentCurrencyAmount::New();
  expect_shipping_option.amount->currency = "USD";
  expect_shipping_option.amount->value = "5.0";
  expected.shipping_options.push_back(std::move(expect_shipping_option));

  base::Value::List shipping_options_list;
  base::Value::Dict shipping_option;
  shipping_option.Set("id", "Post office");
  shipping_option.Set("label", "Post office, one-week ground");
  base::Value::Dict shipping_amount;
  shipping_amount.Set("currency", "USD");
  shipping_amount.Set("value", "5.0");
  shipping_option.Set("amount", std::move(shipping_amount));
  shipping_options_list.Append(std::move(shipping_option));
  details_dict.Set("shippingOptions", std::move(shipping_options_list));

  EXPECT_TRUE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  EXPECT_TRUE(actual.FromValueDict(details_dict, /*requires_total=*/true));
  EXPECT_EQ(expected, actual);

  // Test specifying ID.
  details_dict.Set("id", "1234");
  expected.id = "1234";
  EXPECT_TRUE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  // Test without total when not requiring it.
  {
    PaymentDetails actual2;
    expected.total.reset();

    details_dict.Remove("total");
    EXPECT_TRUE(actual2.FromValueDict(details_dict, /*requires_total=*/false));
    EXPECT_EQ(expected, actual2);
  }
}

// Tests the failure case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromValueFailureDict) {
  base::Value::Dict details_dict;
  details_dict.Set("error", "Error in details");

  PaymentDetails actual;
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/true));

  // Invalid total.
  base::Value::Dict total_dict;
  details_dict.Set("total", std::move(total_dict));
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  details_dict.Remove("total");

  // Invalid display item.
  base::Value::List display_items_list;
  display_items_list.Append("huh");
  details_dict.Set("displayItems", std::move(display_items_list));
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  details_dict.Remove("displayItems");

  // Invalid shipping option.
  base::Value::List shipping_options_list;
  shipping_options_list.Append("nonsense");
  details_dict.Set("shippingOptions", std::move(shipping_options_list));
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  details_dict.Remove("shippingOptions");

  // Invalid modifiers.
  base::Value::List modifiers_list;
  modifiers_list.Append("not a payment method dict");
  details_dict.Set("modifiers", std::move(modifiers_list));
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/false));

  // Invalid modifier total.
  details_dict.FindList("modifiers")->clear();
  base::Value::Dict payment_method;
  payment_method.Set("supportedMethods", "MuenterCard");
  base::Value::Dict invalid_total_dict;
  payment_method.Set("total", std::move(invalid_total_dict));
  details_dict.FindList("modifiers")->Append(std::move(payment_method));
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/false));
  details_dict.FindList("modifiers")->front().GetDict().Remove("total");

  // Invalid additional_display_item in modifiers.
  base::Value::List additional_display_items_list;
  additional_display_items_list.Append("not a payment item");
  details_dict.FindList("modifiers")
      ->front()
      .GetDict()
      .Set("additionalDisplayItems", std::move(additional_display_items_list));
  EXPECT_FALSE(actual.FromValueDict(details_dict, /*requires_total=*/false));
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
