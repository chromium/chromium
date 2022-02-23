// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromValueSuccess) {
  PaymentDetails expected;
  expected.error = "Error in details";

  base::Value details_dict(base::Value::Type::DICTIONARY);
  details_dict.SetStringKey("error", "Error in details");
  PaymentDetails actual;
  EXPECT_TRUE(actual.FromValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  expected.total = std::make_unique<PaymentItem>();
  expected.total->label = "TOTAL";
  expected.total->amount->currency = "GBP";
  expected.total->amount->value = "6.66";

  base::Value total_dict(base::Value::Type::DICTIONARY);
  total_dict.SetStringKey("label", "TOTAL");
  base::Value amount_dict(base::Value::Type::DICTIONARY);
  amount_dict.SetStringKey("currency", "GBP");
  amount_dict.SetStringKey("value", "6.66");
  total_dict.SetKey("amount", std::move(amount_dict));
  details_dict.SetKey("total", std::move(total_dict));

  PaymentItem display1;
  display1.label = "Handling";
  display1.amount->currency = "GBP";
  display1.amount->value = "1.23";
  display1.pending = true;
  expected.display_items.push_back(display1);

  base::Value display_items_list(base::Value::Type::LIST);
  display_items_list.Append(display1.ToValue());
  details_dict.SetKey("displayItems", std::move(display_items_list));

  PaymentShippingOption expect_shipping_option;
  expect_shipping_option.id = "Post office";
  expect_shipping_option.label = "Post office, one-week ground";
  expect_shipping_option.amount = mojom::PaymentCurrencyAmount::New();
  expect_shipping_option.amount->currency = "USD";
  expect_shipping_option.amount->value = "5.0";
  expected.shipping_options.push_back(std::move(expect_shipping_option));

  base::Value shipping_options_list(base::Value::Type::LIST);
  base::Value shipping_option(base::Value::Type::DICTIONARY);
  shipping_option.SetStringKey("id", "Post office");
  shipping_option.SetStringKey("label", "Post office, one-week ground");
  base::Value shipping_amount(base::Value::Type::DICTIONARY);
  shipping_amount.SetStringKey("currency", "USD");
  shipping_amount.SetStringKey("value", "5.0");
  shipping_option.SetKey("amount", std::move(shipping_amount));
  shipping_options_list.Append(std::move(shipping_option));
  details_dict.SetKey("shippingOptions", std::move(shipping_options_list));

  EXPECT_TRUE(actual.FromValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  EXPECT_TRUE(actual.FromValue(details_dict, /*requires_total=*/true));
  EXPECT_EQ(expected, actual);

  // Test specifying ID.
  details_dict.SetStringKey("id", "1234");
  expected.id = "1234";
  EXPECT_TRUE(actual.FromValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  // Test without total when not requiring it.
  {
    PaymentDetails actual2;
    expected.total.reset();

    details_dict.RemoveKey("total");
    EXPECT_TRUE(actual2.FromValue(details_dict, /*requires_total=*/false));
    EXPECT_EQ(expected, actual2);
  }
}

// Tests the failure case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromValueFailure) {
  base::Value details_dict(base::Value::Type::DICTIONARY);
  details_dict.SetStringKey("error", "Error in details");

  PaymentDetails actual;
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/true));

  // Invalid total.
  base::Value total_dict(base::Value::Type::DICTIONARY);
  details_dict.SetKey("total", std::move(total_dict));
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/false));
  details_dict.RemoveKey("total");

  // Invalid display item.
  base::Value display_items_list(base::Value::Type::LIST);
  display_items_list.Append("huh");
  details_dict.SetKey("displayItems", std::move(display_items_list));
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/false));
  details_dict.RemoveKey("displayItems");

  // Invalid shipping option.
  base::Value shipping_options_list(base::Value::Type::LIST);
  shipping_options_list.Append("nonsense");
  details_dict.SetKey("shippingOptions", std::move(shipping_options_list));
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/false));
  details_dict.RemoveKey("shippingOptions");

  // Invalid modifiers.
  base::Value modifiers_list(base::Value::Type::LIST);
  modifiers_list.Append("not a payment method dict");
  details_dict.SetKey("modifiers", std::move(modifiers_list));
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/false));

  // Invalid modifier total.
  details_dict.FindKey("modifiers")->ClearList();
  base::Value payment_method(base::Value::Type::DICTIONARY);
  payment_method.SetStringKey("supportedMethods", "MuenterCard");
  base::Value invalid_total_dict(base::Value::Type::DICTIONARY);
  payment_method.SetKey("total", std::move(invalid_total_dict));
  details_dict.FindKey("modifiers")->Append(std::move(payment_method));
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/false));
  details_dict.FindKey("modifiers")->GetListDeprecated()[0].RemoveKey("total");

  // Invalid additional_display_item in modifiers.
  base::Value additional_display_items_list(base::Value::Type::LIST);
  additional_display_items_list.Append("not a payment item");
  details_dict.FindKey("modifiers")
      ->GetListDeprecated()[0]
      .SetKey("additionalDisplayItems",
              std::move(additional_display_items_list));
  EXPECT_FALSE(actual.FromValue(details_dict, /*requires_total=*/false));

  // Check error-handling of non-dictionary input value.
  EXPECT_FALSE(actual.FromValue(base::Value("hi"), /*requires_total=*/false));
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
