// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_response_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/test_payment_request_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class PaymentResponseHelperTest : public testing::Test,
                                  public PaymentResponseHelper::Delegate {
 protected:
  PaymentResponseHelperTest()
      : test_payment_request_delegate_(&test_personal_data_manager_),
        address_(autofill::test::GetFullProfile()),
        billing_addresses_({&address_}) {
    test_personal_data_manager_.AddProfile(address_);

    // Set up the autofill payment app.
    autofill::CreditCard visa_card = autofill::test::GetCreditCard();
    visa_card.set_billing_address_id(address_.guid());
    visa_card.set_use_count(5u);
    autofill_app_ = std::make_unique<AutofillPaymentApp>(
        "visa", visa_card, /*matches_merchant_card_type_exactly=*/true,
        billing_addresses_, "en-US", &test_payment_request_delegate_);
  }
  ~PaymentResponseHelperTest() override {}

  // PaymentRequestState::Delegate:
  void OnPaymentResponseReady(mojom::PaymentResponsePtr response) override {
    payment_response_ = std::move(response);
  }

  // PaymentRequestState::Delegate:
  void OnPaymentResponseError(const std::string& error_message) override {}

  // Convenience method to create a PaymentRequestSpec with specified |details|
  // and |method_data|.
  void RecreateSpecWithOptionsAndDetails(
      mojom::PaymentOptionsPtr options,
      mojom::PaymentDetailsPtr details,
      std::vector<mojom::PaymentMethodDataPtr> method_data) {
    // The spec will be based on the |options| and |details| passed in.
    spec_ = std::make_unique<PaymentRequestSpec>(
        std::move(options), std::move(details), std::move(method_data), nullptr,
        "en-US");
  }

  // Convenience method to create a PaymentRequestSpec with default details
  // (one shipping option) and method data (only supports visa).
  void RecreateSpecWithOptions(mojom::PaymentOptionsPtr options) {
    // Create dummy PaymentDetails with a single shipping option.
    std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
    mojom::PaymentShippingOptionPtr option =
        mojom::PaymentShippingOption::New();
    option->id = "option:1";
    shipping_options.push_back(std::move(option));
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->shipping_options = std::move(shipping_options);

    RecreateSpecWithOptionsAndDetails(std::move(options), std::move(details),
                                      GetMethodDataForVisa());
  }

  // Convenience method that returns MethodData that supports Visa.
  std::vector<mojom::PaymentMethodDataPtr> GetMethodDataForVisa() {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
    entry->supported_method = "visa";
    method_data.push_back(std::move(entry));
    return method_data;
  }

  PaymentRequestSpec* spec() { return spec_.get(); }
  const mojom::PaymentResponsePtr& response() { return payment_response_; }
  autofill::AutofillProfile* test_address() { return &address_; }
  PaymentApp* test_app() { return autofill_app_.get(); }
  PaymentRequestDelegate* test_payment_request_delegate() {
    return &test_payment_request_delegate_;
  }

 private:
  std::unique_ptr<PaymentRequestSpec> spec_;
  mojom::PaymentResponsePtr payment_response_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  TestPaymentRequestDelegate test_payment_request_delegate_;

  // Test data.
  autofill::AutofillProfile address_;
  const std::vector<autofill::AutofillProfile*> billing_addresses_;
  std::unique_ptr<AutofillPaymentApp> autofill_app_;
};

// Test generating a PaymentResponse.
TEST_F(PaymentResponseHelperTest, GeneratePaymentResponse_SupportedMethod) {
  // Default options (no shipping, no contact info).
  RecreateSpecWithOptions(mojom::PaymentOptions::New());

  // "visa" is specified directly in the supportedMethods so it is returned
  // as the method name.
  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);
  EXPECT_EQ("visa", response()->method_name);
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
      response()->stringified_details);
}

// Test generating a PaymentResponse when the method is specified through
// "basic-card".
TEST_F(PaymentResponseHelperTest, GeneratePaymentResponse_BasicCard) {
  // The method data supports visa through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "basic-card";
  entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateSpecWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                    mojom::PaymentDetails::New(),
                                    std::move(method_data));

  // "basic-card" is specified so it is returned as the method name.
  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);
  EXPECT_EQ("basic-card", response()->method_name);
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
      response()->stringified_details);
}

// Tests the the generated PaymentResponse has the correct values for the
// shipping address.
TEST_F(PaymentResponseHelperTest, GeneratePaymentResponse_ShippingAddress) {
  // Setup so that a shipping address is requested.
  std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
  mojom::PaymentShippingOptionPtr option = mojom::PaymentShippingOption::New();
  option->id = "option:1";
  option->selected = true;
  shipping_options.push_back(std::move(option));
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  details->shipping_options = std::move(shipping_options);
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateSpecWithOptionsAndDetails(std::move(options), std::move(details),
                                    GetMethodDataForVisa());

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);

  // Check that all the expected values were set.
  EXPECT_EQ("US", response()->shipping_address->country);
  EXPECT_EQ("666 Erebus St.", response()->shipping_address->address_line[0]);
  EXPECT_EQ("Apt 8", response()->shipping_address->address_line[1]);
  EXPECT_EQ("CA", response()->shipping_address->region);
  EXPECT_EQ("Elysium", response()->shipping_address->city);
  EXPECT_EQ("", response()->shipping_address->dependent_locality);
  EXPECT_EQ("91111", response()->shipping_address->postal_code);
  EXPECT_EQ("", response()->shipping_address->sorting_code);
  EXPECT_EQ("Underworld", response()->shipping_address->organization);
  EXPECT_EQ("John H. Doe", response()->shipping_address->recipient);
  EXPECT_EQ("16502111111", response()->shipping_address->phone);
}

// Tests the the generated PaymentResponse has the correct values for the
// contact details when all values are requested.
TEST_F(PaymentResponseHelperTest, GeneratePaymentResponse_ContactDetails_All) {
  // Request all contact detail values.
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateSpecWithOptions(std::move(options));

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);

  // Check that all the expected values were set.
  EXPECT_EQ("John H. Doe", response()->payer->name.value());
  EXPECT_EQ("+16502111111", response()->payer->phone.value());
  EXPECT_EQ("johndoe@hades.com", response()->payer->email.value());
}

// Tests the the generated PaymentResponse has the correct values for the
// contact details when all values are requested.
TEST_F(PaymentResponseHelperTest, GeneratePaymentResponse_ContactDetails_Some) {
  // Request one contact detail value.
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  RecreateSpecWithOptions(std::move(options));

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);

  // Check that the name was set, but not the other values.
  EXPECT_EQ("John H. Doe", response()->payer->name.value());
  EXPECT_FALSE(response()->payer->phone.has_value());
  EXPECT_FALSE(response()->payer->email.has_value());
}

// Tests the the generated PaymentResponse has phone number formatted to E.164
// if the number is valid.
TEST_F(PaymentResponseHelperTest,
       GeneratePaymentResponse_ContactPhoneIsFormattedWhenValid) {
  // Request one contact detail value.
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_phone = true;
  test_address()->SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                             base::UTF8ToUTF16("(515) 223-1234"));
  RecreateSpecWithOptions(std::move(options));

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);

  // Check that the phone was formatted.
  EXPECT_EQ("+15152231234", response()->payer->phone.value());
}

// Tests the the generated PaymentResponse has phone number minimumly formatted
// (removing non-digit letters), if the number is invalid
TEST_F(PaymentResponseHelperTest,
       GeneratePaymentResponse_ContactPhoneIsMinimumlyFormattedWhenInvalid) {
  // Request one contact detail value.
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_phone = true;
  test_address()->SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                             base::UTF8ToUTF16("(515) 123-1234"));
  RecreateSpecWithOptions(std::move(options));

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), this);

  // Check that the phone was formatted.
  EXPECT_EQ("5151231234", response()->payer->phone.value());
}

}  // namespace payments
