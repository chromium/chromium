// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/test_content_payment_request_delegate.h"
#include "components/payments/content/test_payment_app.h"
#include "components/payments/core/const_csp_checker.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {
namespace {

class TestAppFactory : public PaymentAppFactory {
 public:
  explicit TestAppFactory(const std::string& method)
      : PaymentAppFactory(PaymentApp::Type::SERVICE_WORKER_APP),
        method_(method) {}

  TestAppFactory(const TestAppFactory& other) = delete;
  TestAppFactory& operator=(const TestAppFactory& other) = delete;

  void Create(base::WeakPtr<Delegate> delegate) override {
    auto requested_methods =
        delegate->GetSpec()->payment_method_identifiers_set();
    if (requested_methods.find(method_) != requested_methods.end())
      delegate->OnPaymentAppCreated(std::make_unique<TestPaymentApp>(method_));
    delegate->OnDoneCreatingPaymentApps();
  }

 private:
  const std::string method_;
};

class PaymentRequestStateTest : public testing::Test,
                                public PaymentRequestState::Observer,
                                public PaymentRequestState::Delegate {
 protected:
  static constexpr char kMethodName[] = "https://www.chromium.org";

  PaymentRequestStateTest()
      : num_on_selected_information_changed_called_(0),
        test_payment_request_delegate_(/*task_executor=*/nullptr,
                                       &test_personal_data_manager_),
        journey_logger_(ukm::UkmRecorder::GetNewSourceID()),
        address_(autofill::test::GetFullProfile()) {
    web_contents_ = web_contents_factory_.CreateWebContents(&context_);

    test_personal_data_manager_.test_address_data_manager()
        .SetAutofillProfileEnabled(true);
    test_personal_data_manager_.test_payments_data_manager()
        .SetAutofillWalletImportEnabled(true);
    test_personal_data_manager_.address_data_manager().AddProfile(address_);
  }
  ~PaymentRequestStateTest() override = default;

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

  void RecreateState(mojom::PaymentOptionsPtr options,
                     mojom::PaymentDetailsPtr details,
                     std::vector<mojom::PaymentMethodDataPtr> method_data,
                     std::unique_ptr<PaymentAppService> app_service) {
    if (!details->total)
      details->total = mojom::PaymentItem::New();
    details->id = "test_id";
    // The spec will be based on the |options| and |details| passed in.
    spec_ = std::make_unique<PaymentRequestSpec>(
        std::move(options), std::move(details), std::move(method_data),
        /*observer=*/nullptr, "en-US");
    state_ = std::make_unique<PaymentRequestState>(
        std::move(app_service), web_contents_->GetPrimaryMainFrame(),
        GURL("https://example.test"), GURL("https://example.test/pay"),
        url::Origin::Create(GURL("https://example.test")), spec_->AsWeakPtr(),
        weak_ptr_factory_.GetWeakPtr(), "en-US", &test_personal_data_manager_,
        test_payment_request_delegate_.GetContentWeakPtr(),
        journey_logger_.GetWeakPtr(), const_csp_checker_.GetWeakPtr());
    state_->AddObserver(this);
  }

  // Convenience method to create a PaymentRequestState with default details
  // (one shipping option) and method data (only supports a url method).
  void RecreateStateWithOptions(mojom::PaymentOptionsPtr options) {
    RecreateStateWithRequestedMethodAndOptions(
        /*requested_method==*/installed_app_method_, std::move(options));
  }

  void RecreateStateWithRequestedMethodAndOptions(
      const std::string& requested_method,
      mojom::PaymentOptionsPtr options) {
    auto app_service = std::make_unique<PaymentAppService>(&context_);
    app_service->AddFactoryForTesting(
        std::make_unique<TestAppFactory>(installed_app_method_));
    RecreateState(std::move(options), CreateDefaultDetails(),
                  GetMethodDataForUrlMethod(requested_method),
                  std::move(app_service));
    base::RunLoop run_loop;
    state()->AreRequestedMethodsSupported(
        base::BindOnce(&OnMethodSupportResult, run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Convenience method that returns MethodData that supports a url method.
  std::vector<mojom::PaymentMethodDataPtr> GetMethodDataForUrlMethod(
      const std::string& requested_method = kMethodName) {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
    entry->supported_method = requested_method;
    method_data.push_back(std::move(entry));
    return method_data;
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
    details->id = "test_id";
    return details;
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

  const std::string& installed_app_method() const {
    return installed_app_method_;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
  std::unique_ptr<PaymentRequestState> state_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  int num_on_selected_information_changed_called_;
  mojom::PaymentResponsePtr payment_response_;
  mojom::PaymentAddressPtr selected_shipping_address_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  TestContentPaymentRequestDelegate test_payment_request_delegate_;
  JourneyLogger journey_logger_;
  ConstCSPChecker const_csp_checker_{/*allow=*/true};

  // Test data.
  autofill::AutofillProfile address_;

 private:
  static void OnMethodSupportResult(base::RepeatingClosure quit_closure,
                                    bool methods_supported,
                                    const std::string& error_message,
                                    AppCreationFailureReason error_reason) {
    quit_closure.Run();
  }

  const std::string installed_app_method_ = kMethodName;
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
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_method = installed_app_method();
  method_data.push_back(std::move(entry));
  auto app_service = std::make_unique<PaymentAppService>(&context_);
  app_service->AddFactoryForTesting(
      std::make_unique<TestAppFactory>(installed_app_method()));
  RecreateState(mojom::PaymentOptions::New(), mojom::PaymentDetails::New(),
                std::move(method_data), std::move(app_service));

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
  RecreateState(mojom::PaymentOptions::New(), mojom::PaymentDetails::New(),
                std::move(method_data),
                std::make_unique<PaymentAppService>(&context_));

  state()->HasEnrolledInstrument(
      base::BindOnce([](bool has_enrolled_instrument) {
        EXPECT_FALSE(has_enrolled_instrument);
      }));

  // CanMakePayment returns true because the requested method is supported, even
  // though the payment app is not ready to pay.
  state()->CanMakePayment(base::BindOnce(
      [](bool can_make_payment) { EXPECT_FALSE(can_make_payment); }));
}

// Test selecting a contact info profile will make the user ready to pay.
TEST_F(PaymentRequestStateTest, ReadyToPay_ContactInfo) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));
  int expected_changes = 2;
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Ready to pay because of default selections.
  EXPECT_TRUE(state()->is_ready_to_pay());

  // Unselecting contact profile.
  state()->SetSelectedContactProfile(/*profile=*/nullptr);
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());

  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedContactProfile(test_address());
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());

  // Ready to pay!
  EXPECT_TRUE(state()->is_ready_to_pay());
}

TEST_F(PaymentRequestStateTest, ReadyToPay_DefaultSelections) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));
  int expected_changes = 2;
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Because there are shipping options, no address is selected by default.
  // Therefore we are not ready to pay.
  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedShippingProfile(test_address());
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());

  // Not ready to pay since there's no selected shipping option.
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the website validates the shipping option.
  state()->SetSelectedShippingOption("option:1");
  auto details = CreateDefaultDetails();
  (*details->shipping_options)[0]->selected = true;
  spec()->UpdateWith(std::move(details));
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
  EXPECT_TRUE(state()->is_ready_to_pay());
}

TEST_F(PaymentRequestStateTest, RetryWithPayerErrors) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));
  int expected_changes = 2;
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  state()->SetSelectedContactProfile(test_address());
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
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
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  EXPECT_FALSE(state()->selected_contact_profile());
  EXPECT_TRUE(state()->invalid_contact_profile());
}

TEST_F(PaymentRequestStateTest, RetryWithShippingAddressErrors) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateStateWithOptions(std::move(options));
  int expected_changes = 2;
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Because there are shipping options, no address is selected by default.
  // Therefore we are not ready to pay.
  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedShippingProfile(test_address());
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());

  // Not ready to pay since there's no selected shipping option.
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the website validates the shipping option.
  state()->SetSelectedShippingOption("option:1");
  auto details = CreateDefaultDetails();
  (*details->shipping_options)[0]->selected = true;
  spec()->UpdateWith(std::move(details));
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
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
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  EXPECT_FALSE(state()->selected_shipping_profile());
  EXPECT_TRUE(state()->invalid_shipping_profile());
}

// Testing that only supported instruments are shown. In this test the merchant
// requests https://payments.example payment method, which is not supported.
TEST_F(PaymentRequestStateTest, UnsupportedMethod) {
  RecreateStateWithRequestedMethodAndOptions(
      /*requested_method=*/"https://payments.example",
      mojom::PaymentOptions::New());
  EXPECT_FALSE(state()->is_ready_to_pay());
  EXPECT_EQ(0u, state()->available_apps().size());
}

TEST_F(PaymentRequestStateTest, SelectedShippingAddressMessage_Normalized) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateStateWithOptions(std::move(options));

  // Make the normalization not be instantaneous.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->DelayNormalization();

  int expected_changes = 2;
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Select an address, nothing should happen until the normalization is
  // completed and the merchant has validated the address.
  state()->SetSelectedShippingProfile(test_address());
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Complete the normalization.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->CompleteAddressNormalization();
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
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
  int expected_changes = 2;

  // Make the normalization not be instantaneous.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->DelayNormalization();

  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());

  // Select an address, nothing should happen until the normalization is
  // completed and the merchant has validated the address.
  autofill::AutofillProfile profile(AddressCountryCode("JP"));
  autofill::test::SetProfileInfo(&profile, "Jon", "V.", "Doe",
                                 "jon.doe@exampl.com", "Example Inc",
                                 "Roppongi", "6 Chrome-10-1", "Tokyo", "",
                                 "106-6126", "JP", "+81363849000");

  state()->SetSelectedShippingProfile(&profile);
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Complete the normalization.
  test_payment_request_delegate()
      ->test_address_normalizer()
      ->CompleteAddressNormalization();
  EXPECT_EQ(expected_changes, num_on_selected_information_changed_called());
  EXPECT_FALSE(state()->is_ready_to_pay());

  // Simulate that the merchant has validated the shipping address change.
  spec()->UpdateWith(CreateDefaultDetails());
  EXPECT_EQ(++expected_changes, num_on_selected_information_changed_called());
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

}  // namespace
}  // namespace payments
