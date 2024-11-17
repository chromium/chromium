// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_response_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/test_payment_app.h"
#include "components/payments/core/test_payment_request_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class PaymentResponseHelperTest : public testing::Test,
                                  public PaymentResponseHelper::Delegate {
 protected:
  PaymentResponseHelperTest()
      : test_payment_request_delegate_(
            std::make_unique<base::SingleThreadTaskExecutor>(),
            &test_personal_data_manager_) {
    address_ = std::make_unique<autofill::AutofillProfile>(
        autofill::test::GetFullProfile());
    test_personal_data_manager_.address_data_manager().AddProfile(*address_);
    test_app_ = std::make_unique<TestPaymentApp>("method-name");
  }
  ~PaymentResponseHelperTest() override = default;

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

  base::WeakPtr<PaymentRequestSpec> spec() { return spec_->AsWeakPtr(); }
  const mojom::PaymentResponsePtr& response() { return payment_response_; }
  autofill::AutofillProfile* test_address() { return address_.get(); }
  base::WeakPtr<PaymentApp> test_app() { return test_app_->AsWeakPtr(); }
  base::WeakPtr<PaymentRequestDelegate> test_payment_request_delegate() {
    return test_payment_request_delegate_.GetWeakPtr();
  }

  base::WeakPtr<PaymentResponseHelperTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<PaymentRequestSpec> spec_;
  mojom::PaymentResponsePtr payment_response_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  TestPaymentRequestDelegate test_payment_request_delegate_;

  // Test data.
  std::unique_ptr<autofill::AutofillProfile> address_;
  std::unique_ptr<PaymentApp> test_app_;

  base::WeakPtrFactory<PaymentResponseHelperTest> weak_ptr_factory_{this};
};

// Test generating a PaymentResponse.
TEST_F(PaymentResponseHelperTest, GeneratePaymentResponse_SupportedMethod) {
  // Default options (no shipping, no contact info).
  RecreateSpecWithOptions(mojom::PaymentOptions::New());

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), GetWeakPtr());
  EXPECT_EQ("method-name", response()->method_name);
  EXPECT_EQ("{\"data\":\"details\"}", response()->stringified_details);
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
                               test_address(), GetWeakPtr());

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
                               test_address(), GetWeakPtr());

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
                               test_address(), GetWeakPtr());

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
                             u"(515) 223-1234");
  RecreateSpecWithOptions(std::move(options));

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), GetWeakPtr());

  // Check that the phone was formatted.
  EXPECT_EQ("+15152231234", response()->payer->phone.value());
}

// Tests the the generated PaymentResponse has phone number minimally formatted
// (removing non-digit letters), if the number is invalid
TEST_F(PaymentResponseHelperTest,
       GeneratePaymentResponse_ContactPhoneIsMinimallyFormattedWhenInvalid) {
  // Request one contact detail value.
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_phone = true;
  test_address()->SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                             u"(515) 123-1234");
  RecreateSpecWithOptions(std::move(options));

  PaymentResponseHelper helper("en-US", spec(), test_app(),
                               test_payment_request_delegate(), test_address(),
                               test_address(), GetWeakPtr());

  // Check that the phone was formatted.
  EXPECT_EQ(base::FeatureList::IsEnabled(
                autofill::features::kAutofillInferCountryCallingCode)
                ? "+15151231234"
                : "5151231234",
            response()->payer->phone.value());
}

}  // namespace payments
