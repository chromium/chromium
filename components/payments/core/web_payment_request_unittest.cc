// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/web_payment_request.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests that populating a WebPaymentRequest from an empty dictionary fails.
TEST(PaymentRequestTest, ParsingEmptyRequestDictionaryFails) {
  WebPaymentRequest output_request;
  base::DictionaryValue request_dict;
  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
}

// Tests that populating a WebPaymentRequest from a dictionary without all
// required values fails.
TEST(PaymentRequestTest, ParsingPartiallyPopulatedRequestDictionaryFails) {
  WebPaymentRequest expected_request;
  WebPaymentRequest output_request;
  base::DictionaryValue request_dict;

  // An empty methodData list alone is insufficient.
  auto method_data_list = std::make_unique<base::ListValue>();
  request_dict.Set("methodData", std::move(method_data_list));

  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);

  // A non-dictionary value in the methodData list is incorrect.
  method_data_list = std::make_unique<base::ListValue>();
  method_data_list->AppendString("fake method data dictionary");
  request_dict.Set("methodData", std::move(method_data_list));

  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);

  // An empty dictionary in the methodData list is still insufficient.
  method_data_list = std::make_unique<base::ListValue>();
  auto method_data_dict = std::make_unique<base::DictionaryValue>();
  method_data_list->Append(std::move(method_data_dict));
  request_dict.Set("methodData", std::move(method_data_list));

  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);
}

// Tests that populating a WebPaymentRequest from a dictionary with all required
// elements succeeds and produces the expected result.
TEST(PaymentRequestTest, ParsingFullyPopulatedRequestDictionarySucceeds) {
  WebPaymentRequest expected_request;
  WebPaymentRequest output_request;
  base::DictionaryValue request_dict;

  // Add the expected values to expected_request.
  expected_request.payment_request_id = "123456789";
  expected_request.details.id = "12345";
  expected_request.details.total = std::make_unique<PaymentItem>();
  expected_request.details.total->label = "TOTAL";
  expected_request.details.total->amount->currency = "GBP";
  expected_request.details.total->amount->value = "6.66";
  expected_request.details.error = "Error in details";

  PaymentMethodData method_data;
  method_data.supported_method = "Visa";
  expected_request.method_data.push_back(method_data);

  // Add the same values to the dictionary to be parsed.
  auto details_dict = std::make_unique<base::DictionaryValue>();
  auto total_dict = std::make_unique<base::DictionaryValue>();
  total_dict->SetString("label", "TOTAL");
  auto amount_dict = std::make_unique<base::DictionaryValue>();
  amount_dict->SetString("currency", "GBP");
  amount_dict->SetString("value", "6.66");
  total_dict->Set("amount", std::move(amount_dict));
  details_dict->Set("total", std::move(total_dict));
  details_dict->SetString("id", "12345");
  details_dict->SetString("error", "Error in details");
  request_dict.Set("details", std::move(details_dict));

  auto method_data_list = std::make_unique<base::ListValue>();
  auto method_data_dict = std::make_unique<base::DictionaryValue>();
  method_data_dict->SetString("supportedMethods", "Visa");
  method_data_list->Append(std::move(method_data_dict));
  request_dict.Set("methodData", std::move(method_data_list));
  request_dict.SetString("id", "123456789");

  // With the required values present, parsing should succeed.
  EXPECT_TRUE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);

  // If payment options are present, parse those as well.
  auto options_dict = std::make_unique<base::DictionaryValue>();
  options_dict->SetBoolean("requestPayerPhone", true);
  options_dict->SetBoolean("requestShipping", true);
  options_dict->SetString("shippingType", "delivery");
  request_dict.Set("options", std::move(options_dict));

  PaymentOptions payment_options;
  payment_options.request_payer_phone = true;
  payment_options.request_shipping = true;
  payment_options.shipping_type = PaymentShippingType::DELIVERY;
  expected_request.options = payment_options;

  EXPECT_TRUE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);
}

// Tests that two payment request objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, WebPaymentRequestEquality) {
  WebPaymentRequest request1;
  WebPaymentRequest request2;
  EXPECT_EQ(request1, request2);

  request1.payment_request_id = "12345";
  EXPECT_NE(request1, request2);
  request2.payment_request_id = "54321";
  EXPECT_NE(request1, request2);
  request2.payment_request_id = request1.payment_request_id;
  EXPECT_EQ(request1, request2);

  mojom::PaymentAddressPtr address1 = mojom::PaymentAddress::New();
  address1->recipient = "Jessica Jones";
  request1.shipping_address = address1.Clone();
  EXPECT_NE(request1, request2);
  mojom::PaymentAddressPtr address2 = mojom::PaymentAddress::New();
  address2->recipient = "Luke Cage";
  request2.shipping_address = address2.Clone();
  EXPECT_NE(request1, request2);
  request2.shipping_address = address1.Clone();
  EXPECT_EQ(request1, request2);

  request1.shipping_option = "2-Day";
  EXPECT_NE(request1, request2);
  request2.shipping_option = "3-Day";
  EXPECT_NE(request1, request2);
  request2.shipping_option = "2-Day";
  EXPECT_EQ(request1, request2);

  PaymentMethodData method_datum;
  method_datum.data = "{merchantId: '123456'}";
  std::vector<PaymentMethodData> method_data1;
  method_data1.push_back(method_datum);
  request1.method_data = method_data1;
  EXPECT_NE(request1, request2);
  std::vector<PaymentMethodData> method_data2;
  request2.method_data = method_data2;
  EXPECT_NE(request1, request2);
  request2.method_data = method_data1;
  EXPECT_EQ(request1, request2);

  PaymentDetails details1;
  details1.total = std::make_unique<PaymentItem>();
  details1.total->label = "Total";
  request1.details = details1;
  EXPECT_NE(request1, request2);
  PaymentDetails details2;
  details2.total = std::make_unique<PaymentItem>();
  details2.total->amount->value = "0.01";
  request2.details = details2;
  EXPECT_NE(request1, request2);
  request2.details = details1;
  EXPECT_EQ(request1, request2);

  PaymentOptions options;
  options.request_shipping = true;
  request1.options = options;
  EXPECT_NE(request1, request2);
  request2.options = options;
  EXPECT_EQ(request1, request2);
}

}  // namespace payments
