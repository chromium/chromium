// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_app_service_factory.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/test_content_payment_request_delegate.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class PaymentRequestStateTest : public testing::Test,
                                public PaymentRequestState::Observer,
                                public PaymentRequestState::Delegate {
 protected:
  PaymentRequestStateTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)),
        num_on_selected_information_changed_called_(0),
        test_payment_request_delegate_(/*task_executor=*/nullptr,
                                       &test_personal_data_manager_),
        journey_logger_(test_payment_request_delegate_.IsOffTheRecord(),
                        ukm::UkmRecorder::GetNewSourceID()),
        address_(autofill::test::GetFullProfile()),
        credit_card_visa_(autofill::test::GetCreditCard()) {
    scoped_feature_list_.InitAndDisableFeature(
        features::kServiceWorkerPaymentApps);
    test_personal_data_manager_.SetAutofillCreditCardEnabled(true);
    test_personal_data_manager_.SetAutofillProfileEnabled(true);
    test_personal_data_manager_.SetAutofillWalletImportEnabled(true);
    test_personal_data_manager_.AddProfile(address_);
    credit_card_visa_.set_billing_address_id(address_.guid());
    credit_card_visa_.set_use_count(5u);
    test_personal_data_manager_.AddCreditCard(credit_card_visa_);
  }
  ~PaymentRequestStateTest() override {}

  // PaymentRequestState::Observer:
  void OnGetAllPaymentAppsFinished() override {}
  void OnSelectedInformationChanged() override {
    num_on_selected_information_changed_called_++;
  }

  // PaymentRequestState::Delegate:
  void OnPaymentResponseAvailable(mojom::PaymentResponsePtr response) override {
    payment_response_ = std::move(response);
  }
  void OnPaymentResponseError(const std::string& error_message) override {}
  void OnShippingOptionIdSelected(std::string shipping_option_id) override {}
  void OnShippingAddressSelected(mojom::PaymentAddressPtr address) override {
    selected_shipping_address_ = std::move(address);
  }
  void OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) override {}

  void RecreateStateWithOptionsAndDetails(
      mojom::PaymentOptionsPtr options,
      mojom::PaymentDetailsPtr details,
      std::vector<mojom::PaymentMethodDataPtr> method_data) {
    if (!details->total)
      details->total = mojom::PaymentItem::New();
    // The spec will be based on the |options| and |details| passed in.
    spec_ = std::make_unique<PaymentRequestSpec>(
        std::move(options), std::move(details), std::move(method_data),
        /*observer=*/nullptr, "en-US");
    PaymentAppServiceFactory::SetForTesting(
        std::make_unique<PaymentAppService>(&context_));
    state_ = std::make_unique<PaymentRequestState>(
        web_contents_->GetMainFrame(), GURL("https://example.com"),
        GURL("https://example.com/pay"),
        url::Origin::Create(GURL("https://example.com")), spec_->AsWeakPtr(),
        weak_ptr_factory_.GetWeakPtr(), "en-US", &test_personal_data_manager_,
        &test_payment_request_delegate_, &journey_logger_);
    state_->AddObserver(this);
  }

  // Convenience method to create a PaymentRequestState with default details
  // (one shipping option) and method data (only supports visa).
  void RecreateStateWithOptions(mojom::PaymentOptionsPtr options) {
    RecreateStateWithOptionsAndDetails(
        std::move(options), CreateDefaultDetails(), GetMethodDataForVisa());
  }

  // Convenience method that returns a dummy PaymentDetails with a single
  // shipping option.
  mojom::PaymentDetailsPtr CreateDefaultDetails() {
    std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
    mojom::PaymentShippingOptionPtr option =
        mojom::PaymentShippingOption::New();
    option->id = "option:1";
    shipping_options.push_back(std::move(option));
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->shipping_options = std::move(shipping_options);
    return details;
  }

  // Convenience method that returns MethodData that supports Visa.
  std::vector<mojom::PaymentMethodDataPtr> GetMethodDataForVisa() {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
    entry->supported_method = "basic-card";
    entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
    method_data.push_back(std::move(entry));
    return method_data;
  }

  PaymentRequestState* state() { return state_.get(); }
  PaymentRequestSpec* spec() { return spec_.get(); }
  const mojom::PaymentResponsePtr& response() { return payment_response_; }
  const mojom::PaymentAddressPtr& selected_shipping_address() {
    return selected_shipping_address_;
  }
  int num_on_selected_information_changed_called() {
    return num_on_selected_information_changed_called_;
  }

  autofill::AutofillProfile* test_address() { return &address_; }
  TestContentPaymentRequestDelegate* test_payment_request_delegate() {
    return &test_payment_request_delegate_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;  // Owned by `web_contents_factory_`.
  std::unique_ptr<PaymentRequestState> state_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  int num_on_selected_information_changed_called_;
  mojom::PaymentResponsePtr payment_response_;
  mojom::PaymentAddressPtr selected_shipping_address_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  TestContentPaymentRequestDelegate test_payment_request_delegate_;
  JourneyLogger journey_logger_;

  // Test data.
  autofill::AutofillProfile address_;
  autofill::CreditCard credit_card_visa_;
  base::WeakPtrFactory<PaymentRequestStateTest> weak_ptr_factory_{this};
};

TEST_F(PaymentRequestStateTest, CanMakePayment) {
  // Default options.
  RecreateStateWithOptions(mojom::PaymentOptions::New());

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_TRUE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_TRUE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest, CanMakePayment_NoEnrolledInstrument) {
  // The method data requires MasterCard.
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "basic-card";
  entry->supported_networks.push_back(mojom::BasicCardNetwork::MASTERCARD);
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_FALSE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported, even
  // though the payment app is not ready to pay.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_TRUE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest, CanMakePayment_UnsupportedPaymentMethod) {
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "unsupported_method";
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_FALSE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported, even
  // though the payment app is not ready to pay.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_FALSE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest, CanMakePayment_OnlyBasicCard) {
  // The method data supports everything in basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "basic-card";
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_TRUE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_TRUE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest, CanMakePayment_BasicCard_SpecificAvailable) {
  // The method data supports visa through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "basic-card";
  entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_TRUE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_TRUE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest,
       CanMakePayment_BasicCard_SpecificAvailableButInvalid) {
  // The method data supports jcb through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "basic-card";
  entry->supported_networks.push_back(mojom::BasicCardNetwork::JCB);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_FALSE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported, even
  // though there is no enrolled instrument.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_TRUE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest, CanMakePayment_BasicCard_SpecificUnavailable) {
  // The method data supports mastercard through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = "basic-card";
  entry->supported_networks.push_back(mojom::BasicCardNetwork::MASTERCARD);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_FALSE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported, even
  // though there is no enrolled instrument.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_TRUE(can_make_payment); }));
}

TEST_F(PaymentRequestStateTest, ReadyToPay_DefaultSelections) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));

  // Because there are shipping options, no address is selected by default.
  // Therefore we are not ready to pay.
  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedShippingProfile(test_address());
  EXPECT_EQ(0, num_on_selected_information_changed_called());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(1, num_on_selected_information_changed_called());

  // Not ready to pay since there's no selected shipping option.
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the website validates the shipping option.
  state()->SetSelectedShippingOption("option:1");
  auto details = CreateDefaultDetails();
  (*details->shipping_options)[0]->selected = true;
  spec()->UpdateWith(std::move(details));
  EXPECT_EQ(2, num_on_selected_information_changed_called());
  EXPECT_TRUE(state()->is_ready_to_pay());
}

// Testing that only supported intruments are shown. In this test the merchant
// only supports Visa.
TEST_F(PaymentRequestStateTest, UnsupportedCardAreNotAvailable) {
  // Default options.
  RecreateStateWithOptions(mojom::PaymentOptions::New());

  // Ready to pay because the default app is selected and supported.
  EXPECT_TRUE(state()->is_ready_to_pay());

  // There's only one app available, even though there's an Amex in
  // PersonalDataManager.
  EXPECT_EQ(1u, state()->available_apps().size());
}

// Test selecting a contact info profile will make the user ready to pay.
TEST_F(PaymentRequestStateTest, ReadyToPay_ContactInfo) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));

  // Ready to pay because of default selections.
  EXPECT_TRUE(state()->is_ready_to_pay());

  // Unselecting contact profile.
  state()->SetSelectedContactProfile(/*profile=*/nullptr);
  EXPECT_EQ(1, num_on_selected_information_changed_called());

  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedContactProfile(test_address());
  EXPECT_EQ(2, num_on_selected_information_changed_called());

  // Ready to pay!
  EXPECT_TRUE(state()->is_ready_to_pay());
}

TEST_F(PaymentRequestStateTest, SelectedShippingAddressMessage_Normalized) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateStateWithOptions(std::move(options));

  // Make the normalization not be instantaneous.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->DelayNormalization();

  EXPECT_EQ(0, num_on_selected_information_changed_called());

  // Select an address, nothing should happen until the normalization is
  // completed and the merchant has validated the address.
  state()->SetSelectedShippingProfile(test_address());
  EXPECT_EQ(0, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Complete the normalization.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->CompleteAddressNormalization();
  EXPECT_EQ(0, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(1, num_on_selected_information_changed_called());
  // Not ready to pay because there's no selected shipping option.
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Check that all the expected values were set for the shipping address.
  EXPECT_EQ("US", selected_shipping_address()->country);
  EXPECT_EQ("666 Erebus St.", selected_shipping_address()->address_line[0]);
  EXPECT_EQ("Apt 8", selected_shipping_address()->address_line[1]);
  EXPECT_EQ("CA", selected_shipping_address()->region);
  EXPECT_EQ("Elysium", selected_shipping_address()->city);
  EXPECT_EQ("", selected_shipping_address()->dependent_locality);
  EXPECT_EQ("91111", selected_shipping_address()->postal_code);
  EXPECT_EQ("", selected_shipping_address()->sorting_code);
  EXPECT_EQ("Underworld", selected_shipping_address()->organization);
  EXPECT_EQ("John H. Doe", selected_shipping_address()->recipient);
  EXPECT_EQ("16502111111", selected_shipping_address()->phone);
}

TEST_F(PaymentRequestStateTest, JaLatnShippingAddress) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateStateWithOptions(std::move(options));

  // Make the normalization not be instantaneous.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->DelayNormalization();

  EXPECT_EQ(0, num_on_selected_information_changed_called());

  // Select an address, nothing should happen until the normalization is
  // completed and the merchant has validated the address.
  autofill::AutofillProfile profile(base::GenerateGUID(),
                                    "https://example.com");
  autofill::test::SetProfileInfo(&profile, "Jon", "V.", "Doe",
                                 "jon.doe@exampl.com", "Example Inc",
                                 "Roppongi", "6 Chrome-10-1", "Tokyo", "",
                                 "106-6126", "JP", "+81363849000");

  state()->SetSelectedShippingProfile(&profile);
  EXPECT_EQ(0, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Complete the normalization.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->CompleteAddressNormalization();
  EXPECT_EQ(0, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(1, num_on_selected_information_changed_called());
  // Not ready to pay because there's no selected shipping option.
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Check that all the expected values were set for the shipping address.
  EXPECT_EQ("JP", selected_shipping_address()->country);
  EXPECT_EQ("Roppongi", selected_shipping_address()->address_line[0]);
  EXPECT_EQ("6 Chrome-10-1", selected_shipping_address()->address_line[1]);
  EXPECT_EQ("", selected_shipping_address()->region);
  EXPECT_EQ("Tokyo", selected_shipping_address()->city);
  EXPECT_EQ("", selected_shipping_address()->dependent_locality);
  EXPECT_EQ("106-6126", selected_shipping_address()->postal_code);
  EXPECT_EQ("", selected_shipping_address()->sorting_code);
  EXPECT_EQ("Example Inc", selected_shipping_address()->organization);
  EXPECT_EQ("Jon V. Doe", selected_shipping_address()->recipient);
  EXPECT_EQ("+81363849000", selected_shipping_address()->phone);
}

TEST_F(PaymentRequestStateTest, RetryWithShippingAddressErrors) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateStateWithOptions(std::move(options));

  // Because there are shipping options, no address is selected by default.
  // Therefore we are not ready to pay.
  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedShippingProfile(test_address());
  EXPECT_EQ(0, num_on_selected_information_changed_called());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(1, num_on_selected_information_changed_called());

  // Not ready to pay since there's no selected shipping option.
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the website validates the shipping option.
  state()->SetSelectedShippingOption("option:1");
  auto details = CreateDefaultDetails();
  (*details->shipping_options)[0]->selected = true;
  spec()->UpdateWith(std::move(details));
  EXPECT_EQ(2, num_on_selected_information_changed_called());
  EXPECT_TRUE(state()->is_ready_to_pay());

  EXPECT_TRUE(state()->selected_shipping_profile());
  EXPECT_FALSE(state()->invalid_shipping_profile());

  mojom::AddressErrorsPtr shipping_address_errors = mojom::AddressErrors::New();
  shipping_address_errors->address_line = "Invalid address line";
  shipping_address_errors->city = "Invalid city";

  mojom::PaymentValidationErrorsPtr errors =
      mojom::PaymentValidationErrors::New();
  errors->shipping_address = std::move(shipping_address_errors);
  spec()->Retry(std::move(errors));
  EXPECT_EQ(3, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  EXPECT_FALSE(state()->selected_shipping_profile());
  EXPECT_TRUE(state()->invalid_shipping_profile());
}

TEST_F(PaymentRequestStateTest, RetryWithPayerErrors) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));

  state()->SetSelectedContactProfile(test_address());
  EXPECT_EQ(1, num_on_selected_information_changed_called());
  EXPECT_TRUE(state()->is_ready_to_pay());

  EXPECT_TRUE(state()->selected_contact_profile());
  EXPECT_FALSE(state()->invalid_contact_profile());

  mojom::PayerErrorsPtr payer_errors = mojom::PayerErrors::New();
  payer_errors->email = "Invalid email";
  payer_errors->name = "Invalid name";
  payer_errors->phone = "Invalid phone";

  mojom::PaymentValidationErrorsPtr errors =
      mojom::PaymentValidationErrors::New();
  errors->payer = std::move(payer_errors);
  spec()->Retry(std::move(errors));
  EXPECT_EQ(2, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  EXPECT_FALSE(state()->selected_contact_profile());
  EXPECT_TRUE(state()->invalid_contact_profile());
}

}  // namespace payments
