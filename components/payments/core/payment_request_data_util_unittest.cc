// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/basic_card_response.h"
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

static const char kBasicCardMethodName[] = "basic-card";
static const char* kBasicCardNetworks[] = {"amex",     "diners",     "discover",
                                           "jcb",      "mastercard", "mir",
                                           "unionpay", "visa"};

// Tests that the serialized version of the PaymentAddress is according to the
// PaymentAddress spec.
TEST(PaymentRequestDataUtilTest, GetPaymentAddressFromAutofillProfile) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  std::unique_ptr<base::DictionaryValue> address_value =
      payments::PaymentAddressToDictionaryValue(
          *payments::data_util::GetPaymentAddressFromAutofillProfile(address,
                                                                     "en-US"));
  std::string json_address;
  base::JSONWriter::Write(*address_value, &json_address);
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

// Tests that the basic card response constructed from a credit card with
// associated billing address has the right structure once serialized.
TEST(PaymentRequestDataUtilTest, GetBasicCardResponseFromAutofillCreditCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  std::unique_ptr<base::DictionaryValue> response_value =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          card, base::ASCIIToUTF16("123"), address, "en-US")
          ->ToDictionaryValue();
  std::string json_response;
  base::JSONWriter::Write(*response_value, &json_response);
  EXPECT_EQ(
      "{\"billingAddress\":"
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"dependentLocality\":\"\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\","
      "\"sortingCode\":\"\"},"
      "\"cardNumber\":\"4111111111111111\","
      "\"cardSecurityCode\":\"123\","
      "\"cardholderName\":\"Test User\","
      "\"expiryMonth\":\"11\","
      "\"expiryYear\":\"2022\"}",
      json_response);
}

// A test fixture to check ParseSupportedMethods() returns empty supported
// networks when // input is an unsupported payment method.
typedef ::testing::TestWithParam<const char*> InvalidSupportedMethodTest;
TEST_P(InvalidSupportedMethodTest, Test) {
  PaymentMethodData method_data;
  // GetParam() is expected to be an unsupported payment method identifier.
  const char* network = GetParam();
  method_data.supported_method = network;

  std::vector<PaymentMethodData> method_data_list{method_data};
  std::vector<std::string> supported_networks;
  std::set<std::string> basic_card_specified_networks;
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(
      method_data_list, &supported_networks, &basic_card_specified_networks,
      &url_payment_method_identifiers, &payment_method_identifiers);

  EXPECT_TRUE(supported_networks.empty());
  EXPECT_TRUE(basic_card_specified_networks.empty());
  EXPECT_TRUE(url_payment_method_identifiers.empty());
  EXPECT_THAT(payment_method_identifiers, ElementsAre(network));
}

// Tests that card networks are not recognized as valid |supported_methods|.
INSTANTIATE_TEST_SUITE_P(
    PaymentRequestDataUtil_ParseSupportedMethods_CardNetworks,
    InvalidSupportedMethodTest,
    ::testing::ValuesIn(kBasicCardNetworks));

// Tests that an arbitrary string is not a valid |supported_methods|.
INSTANTIATE_TEST_SUITE_P(
    PaymentRequestDataUtil_ParseSupportedMethods_ArbitraryPMI,
    InvalidSupportedMethodTest,
    ::testing::Values("foo"));

// A test fixture to check ParseSupportedMethods() correctly returns the card
// networks for the "basic-card" payment method.
typedef ::testing::TestWithParam<const char*> SupportedNetworksTest;
#if defined(OS_IOS) && !TARGET_OS_SIMULATOR
// TODO(crbug.com/1008023): Enable this test on iOS devices.
#define MAYBE_SupportedNetworks DISABLED_SupportedNetworks
#else
#define MAYBE_SupportedNetworks SupportedNetworks
#endif  // defined(OS_IOS) && !TARGET_OS_SIMULATOR
TEST_P(SupportedNetworksTest, MAYBE_SupportedNetworks) {
  PaymentMethodData method_data;
  method_data.supported_method = kBasicCardMethodName;
  // GetParam() is expected to be a basic-card network.
  std::string network = GetParam();
  method_data.supported_networks.push_back(network);

  std::vector<PaymentMethodData> method_data_list{method_data};
  std::vector<std::string> supported_networks;
  std::set<std::string> basic_card_specified_networks;
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(
      method_data_list, &supported_networks, &basic_card_specified_networks,
      &url_payment_method_identifiers, &payment_method_identifiers);

  EXPECT_THAT(supported_networks, ElementsAre(network));
  EXPECT_THAT(basic_card_specified_networks, ElementsAre(network));
  EXPECT_TRUE(url_payment_method_identifiers.empty());
  EXPECT_THAT(payment_method_identifiers, ElementsAre(kBasicCardMethodName));
}

// Tests that the card networks are valid |supported_networks| for "basic-card".
INSTANTIATE_TEST_SUITE_P(PaymentRequestDataUtil_ParseSupportedMethods_BasicCard,
                         SupportedNetworksTest,
                         ::testing::ValuesIn(kBasicCardNetworks));

// Tests that empty |supported_networks| means all networks are supported.
TEST(PaymentRequestDataUtil,
     ParseSupportedMethods_AllNetworksSupportedByDefault) {
  PaymentMethodData method_data;
  method_data.supported_method = kBasicCardMethodName;

  std::vector<PaymentMethodData> method_data_list{method_data};
  std::vector<std::string> supported_networks;
  std::set<std::string> basic_card_specified_networks;
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(
      method_data_list, &supported_networks, &basic_card_specified_networks,
      &url_payment_method_identifiers, &payment_method_identifiers);

  EXPECT_THAT(supported_networks, ElementsAreArray(kBasicCardNetworks));
  EXPECT_THAT(basic_card_specified_networks,
              UnorderedElementsAreArray(kBasicCardNetworks));
  EXPECT_TRUE(url_payment_method_identifiers.empty());
  EXPECT_THAT(payment_method_identifiers, ElementsAre(kBasicCardMethodName));
}

// Tests that a unrecognized |supported_networks| is ignored.
TEST(PaymentRequestDataUtil, ParseSupportedMethods_UnknownBasicCardNetwork) {
  PaymentMethodData method_data;
  method_data.supported_method = kBasicCardMethodName;
  method_data.supported_networks.push_back("foo");

  std::vector<PaymentMethodData> method_data_list{method_data};
  std::vector<std::string> supported_networks;
  std::set<std::string> basic_card_specified_networks;
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(
      method_data_list, &supported_networks, &basic_card_specified_networks,
      &url_payment_method_identifiers, &payment_method_identifiers);

  EXPECT_TRUE(supported_networks.empty());
  EXPECT_TRUE(basic_card_specified_networks.empty());
  EXPECT_TRUE(url_payment_method_identifiers.empty());
  EXPECT_THAT(payment_method_identifiers, ElementsAre(kBasicCardMethodName));
}

// Tests that |PaymentMethodData| with invalid |supported_methods| is ignored.
TEST(PaymentRequestDataUtil, ParseSupportedMethods_InvalidPaymentMethodData) {
  PaymentMethodData valid_method_data;
  valid_method_data.supported_method = kBasicCardMethodName;
  valid_method_data.supported_networks.push_back("visa");

  PaymentMethodData invalid_method_data;
  invalid_method_data.supported_method = "mastercard";

  std::vector<PaymentMethodData> method_data_list{valid_method_data,
                                                  invalid_method_data};
  std::vector<std::string> supported_networks;
  std::set<std::string> basic_card_specified_networks;
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(
      method_data_list, &supported_networks, &basic_card_specified_networks,
      &url_payment_method_identifiers, &payment_method_identifiers);

  EXPECT_THAT(supported_networks, ElementsAre("visa"));
  EXPECT_THAT(basic_card_specified_networks, ElementsAre("visa"));
  EXPECT_TRUE(url_payment_method_identifiers.empty());
  EXPECT_THAT(payment_method_identifiers,
              UnorderedElementsAre(kBasicCardMethodName, "mastercard"));
}

// Tests multiple payment methods are parsed correctly, and that if more than
// one "basic-card" entries exist, they are effectively merged with no
// duplicates.
TEST(PaymentRequestDataUtil, ParseSupportedMethods_MultipleEntries) {
  PaymentMethodData basic_card_data_1;
  basic_card_data_1.supported_method = kBasicCardMethodName;
  basic_card_data_1.supported_networks.push_back("visa");

  PaymentMethodData basic_card_data_2;
  basic_card_data_2.supported_method = kBasicCardMethodName;
  basic_card_data_2.supported_networks.push_back("mastercard");
  basic_card_data_2.supported_networks.push_back("visa");

  const char kBobPayMethod[] = "https://bobpay.xyz/";
  PaymentMethodData url_method;
  url_method.supported_method = kBobPayMethod;

  std::vector<PaymentMethodData> method_data_list{
      basic_card_data_1, basic_card_data_2, url_method};
  std::vector<std::string> supported_networks;
  std::set<std::string> basic_card_specified_networks;
  std::vector<GURL> url_payment_method_identifiers;
  std::set<std::string> payment_method_identifiers;

  payments::data_util::ParseSupportedMethods(
      method_data_list, &supported_networks, &basic_card_specified_networks,
      &url_payment_method_identifiers, &payment_method_identifiers);

  EXPECT_THAT(supported_networks, ElementsAre("visa", "mastercard"));
  EXPECT_THAT(basic_card_specified_networks,
              UnorderedElementsAre("visa", "mastercard"));
  EXPECT_THAT(url_payment_method_identifiers, ElementsAre(kBobPayMethod));
  EXPECT_THAT(payment_method_identifiers,
              UnorderedElementsAre(kBasicCardMethodName, kBobPayMethod));
}

}  // namespace data_util
}  // namespace payments
