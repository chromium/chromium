// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_method_data.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentMethodData from a dictionary
// when the supportedMethods is a string.
TEST(PaymentMethodData, FromDictionaryValueSuccess_SupportedMethodsString) {
  PaymentMethodData expected;
  expected.supported_method = "basic-card";
  expected.data = "{\"supportedNetworks\":[\"mastercard\"]}";
  expected.supported_networks.push_back("mastercard");

  base::DictionaryValue method_data_dict;
  method_data_dict.SetString("supportedMethods", "basic-card");
  base::DictionaryValue data_dict;
  base::ListValue supported_networks_list;
  supported_networks_list.AppendString("mastercard");
  data_dict.SetKey("supportedNetworks", std::move(supported_networks_list));
  method_data_dict.SetKey("data", std::move(data_dict));

  PaymentMethodData actual;
  EXPECT_TRUE(actual.FromDictionaryValue(method_data_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure cases when populating a PaymentMethodData from a
// dictionary.
TEST(PaymentMethodData, FromDictionaryValueFailure) {
  // At least one supported method is required.
  PaymentMethodData actual;
  base::DictionaryValue method_data_dict;
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));

  // The value in the supported methods list must be a string.
  base::ListValue supported_methods_list1;
  supported_methods_list1.AppendInteger(13);
  method_data_dict.SetKey("supportedMethods",
                          std::move(supported_methods_list1));
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));

  // The value in the supported methods list must be a non-empty string.
  base::ListValue supported_methods_list2;
  supported_methods_list2.AppendString("");
  method_data_dict.SetKey("supportedMethods",
                          std::move(supported_methods_list2));
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));

  // The value in the supported methods must be a string.
  method_data_dict.SetInteger("supportedMethods", 13);
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));

  // The value in the supported methods must be a non-empty string.
  method_data_dict.SetString("supportedMethods", "");
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));
}

// Tests that two method data objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
TEST(PaymentMethodData, Equality) {
  PaymentMethodData method_data1;
  PaymentMethodData method_data2;
  EXPECT_EQ(method_data1, method_data2);

  method_data1.supported_method = "basic-card";
  EXPECT_NE(method_data1, method_data2);

  method_data2.supported_method = "http://bobpay.com";
  EXPECT_NE(method_data1, method_data2);

  method_data2.supported_method = "basic-card";
  ;
  EXPECT_EQ(method_data1, method_data2);

  method_data1.data = "{merchantId: '123456'}";
  EXPECT_NE(method_data1, method_data2);
  method_data2.data = "{merchantId: '9999-88'}";
  EXPECT_NE(method_data1, method_data2);
  method_data2.data = "{merchantId: '123456'}";
  EXPECT_EQ(method_data1, method_data2);

  std::vector<std::string> supported_networks1{"visa"};
  method_data1.supported_networks = supported_networks1;
  EXPECT_NE(method_data1, method_data2);
  std::vector<std::string> supported_networks2{"jcb"};
  method_data2.supported_networks = supported_networks2;
  EXPECT_NE(method_data1, method_data2);
  method_data2.supported_networks = supported_networks1;
  EXPECT_EQ(method_data1, method_data2);
}

}  // namespace payments
