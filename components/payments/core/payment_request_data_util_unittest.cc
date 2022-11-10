// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_method_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace data_util {

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

// These payment method identifiers are unsupported by ParseSupportedMethods.
// This does not mean they are unsupported by PaymentRequest in general.
static const char* kUnsupportedPaymentMethodIdentifiers[] = {
    "foo", "secure-payment-confirmation", "file://invalid_url"};

// Tests that the serialized version of the PaymentAddress is according to the
// PaymentAddress spec.
TEST(PaymentRequestDataUtilTest, GetPaymentAddressFromAutofillProfile) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  base::Value::Dict address_value = payments::PaymentAddressToValueDict(
      *payments::data_util::GetPaymentAddressFromAutofillProfile(address,
                                                                 "en-US"));
  std::string json_address;
  base::JSONWriter::Write(address_value, &json_address);
  EXPECT_EQ(
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"dependentLocality\":\"\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\","
      "\"sortingCode\":\"\"}",
      json_address);
}

// A test fixture to check ParseSupportedMethods() returns empty identifier
// lists when input is an unsupported payment method.
typedef ::testing::TestWithParam<const char*> InvalidSupportedMethodTest;
TEST_P(InvalidSupportedMethodTest, Test) {
  PaymentMethodData method_data;
  // GetParam() is expected to be an unsupported payment method identifier.
  const char* method = GetParam();
  method_data.supported_method = method;

  std::vector<PaymentMethodData> method_data_list{method_data};
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(method_data_list,
                                             &url_payment_method_identifiers,
                                             &payment_method_identifiers);

  EXPECT_TRUE(url_payment_method_identifiers.empty());
  EXPECT_THAT(payment_method_identifiers, ElementsAre(method));
}

INSTANTIATE_TEST_SUITE_P(
    PaymentRequestDataUtil_ParseSupportedMethods_InvalidIdentifiers,
    InvalidSupportedMethodTest,
    ::testing::ValuesIn(kUnsupportedPaymentMethodIdentifiers));

// Tests multiple payment methods are parsed correctly, and that URL-based
// methods are extracted correctly.
TEST(PaymentRequestDataUtil, ParseSupportedMethods_MultipleEntries) {
  const char kUnknownMethod[] = "unknown-method";
  PaymentMethodData method_data_1;
  method_data_1.supported_method = kUnknownMethod;

  const char kBobPayMethod[] = "https://bobpay.xyz/";
  PaymentMethodData method_data_2;
  method_data_2.supported_method = kBobPayMethod;

  std::vector<PaymentMethodData> method_data_list{method_data_1, method_data_2};
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(method_data_list,
                                             &url_payment_method_identifiers,
                                             &payment_method_identifiers);

  EXPECT_THAT(url_payment_method_identifiers, ElementsAre(kBobPayMethod));
  EXPECT_THAT(payment_method_identifiers,
              UnorderedElementsAre(kUnknownMethod, kBobPayMethod));
}

TEST(PaymentRequestDataUtil, FilterStringifiedMethodData) {
  std::map<std::string, std::set<std::string>> requested;
  std::set<std::string> supported;
  EXPECT_TRUE(FilterStringifiedMethodData(requested, supported)->empty());

  requested["a"].insert("{\"b\": \"c\"}");
  EXPECT_TRUE(FilterStringifiedMethodData(requested, supported)->empty());

  requested["x"].insert("{\"y\": \"z\"}");
  EXPECT_TRUE(FilterStringifiedMethodData(requested, supported)->empty());

  supported.insert("x");
  std::map<std::string, std::set<std::string>> expected;
  expected["x"].insert("{\"y\": \"z\"}");
  EXPECT_EQ(expected, *FilterStringifiedMethodData(requested, supported));

  supported.insert("g");
  EXPECT_EQ(expected, *FilterStringifiedMethodData(requested, supported));
}

}  // namespace data_util
}  // namespace payments
