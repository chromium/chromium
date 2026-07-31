// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/payments/content/mock_content_payment_request_delegate.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {
namespace {

class PaymentRequestValidationTest : public content::RenderViewHostTestHarness,
                                     public testing::WithParamInterface<bool> {
 protected:
  PaymentRequestValidationTest() = default;
  ~PaymentRequestValidationTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    if (IsSecurePaymentConfirmationEnabled()) {
      scoped_features_.InitAndEnableFeature(
          ::features::kSecurePaymentConfirmation);
    } else {
      scoped_features_.InitAndDisableFeature(
          ::features::kSecurePaymentConfirmation);
    }
    personal_data_manager_.test_address_data_manager()
        .SetAutofillProfileEnabled(false);
    NavigateAndCommit(page_url_);
  }

  bool IsSecurePaymentConfirmationEnabled() const { return GetParam(); }

  std::unique_ptr<testing::NiceMock<MockContentPaymentRequestDelegate>>
  CreateMockDelegate() {
    auto delegate = std::make_unique<
        testing::NiceMock<MockContentPaymentRequestDelegate>>();
    ON_CALL(*delegate, GetRenderFrameHost())
        .WillByDefault(testing::Return(main_rfh()));
    ON_CALL(*delegate, GetDisplayManager())
        .WillByDefault(testing::Return(&display_manager_));
    ON_CALL(*delegate, GetApplicationLocale())
        .WillByDefault(testing::ReturnRefOfCopy(std::string("en-US")));
    ON_CALL(*delegate, GetLastCommittedURL())
        .WillByDefault(testing::ReturnRef(page_url_));
    ON_CALL(*delegate, GetPersonalDataManager())
        .WillByDefault(testing::Return(&personal_data_manager_));
    ON_CALL(*delegate, IsBrowserWindowActive())
        .WillByDefault(testing::Return(true));
    return delegate;
  }

  mojom::PaymentMethodDataPtr CreateSpcMethodData() {
    auto spc_method = mojom::PaymentMethodData::New();
    spc_method->supported_method = "secure-payment-confirmation";
    spc_method->stringified_data = "{}";
    if (IsSecurePaymentConfirmationEnabled()) {
      spc_method->secure_payment_confirmation =
          mojom::SecurePaymentConfirmationRequest::New();
      spc_method->secure_payment_confirmation->credential_ids.push_back(
          {1, 2, 3, 4});
      spc_method->secure_payment_confirmation->challenge = {1, 2, 3, 4};
      spc_method->secure_payment_confirmation->rp_id = "rp.id";
      spc_method->secure_payment_confirmation->instrument =
          blink::mojom::PaymentCredentialInstrument::New();
      spc_method->secure_payment_confirmation->instrument->display_name =
          "Instrument";
      spc_method->secure_payment_confirmation->instrument->icon =
          GURL("https://a.com/icon.png");
      spc_method->secure_payment_confirmation->payee_origin =
          url::Origin::Create(GURL("https://merchant.example"));
    }
    return spc_method;
  }

  mojom::PaymentDetailsPtr CreateDummyDetails() {
    auto details = mojom::PaymentDetails::New();
    details->id = "12345";
    details->total = mojom::PaymentItem::New();
    details->total->label = "Total";
    details->total->amount = mojom::PaymentCurrencyAmount::New();
    details->total->amount->currency = "USD";
    details->total->amount->value = "1.00";
    return details;
  }

  const GURL page_url_{"https://a.com"};
  autofill::TestPersonalDataManager personal_data_manager_;
  PaymentRequestDisplayManager display_manager_;
  base::test::ScopedFeatureList scoped_features_;
};

// Tests that if "secure-payment-confirmation" is present in method_data
// alongside other methods:
// - If the SPC feature is enabled, this is invalid and the renderer is
// terminated.
// - If the SPC feature is disabled, this is allowed and no termination occurs.
TEST_P(PaymentRequestValidationTest, SecurePaymentConfirmationExclusivity) {
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<mojom::PaymentRequest> payment_request;
  new PaymentRequest(CreateMockDelegate(),
                     payment_request.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::PaymentRequestClient> client_remote;
  auto dummy_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(CreateSpcMethodData());

  auto dummy_method = mojom::PaymentMethodData::New();
  dummy_method->supported_method = "dummy-method";
  dummy_method->stringified_data = "{}";
  method_data.push_back(std::move(dummy_method));

  auto options = mojom::PaymentOptions::New();

  payment_request->Init(std::move(client_remote), std::move(method_data),
                        CreateDummyDetails(), std::move(options));

  if (IsSecurePaymentConfirmationEnabled()) {
    bad_message_observer.WaitForBadMessage();
    EXPECT_TRUE(bad_message_observer.got_bad_message());
  } else {
    // The Mojo pipe should remain open, and no bad message should be reported.
    EXPECT_TRUE(payment_request.is_connected());
    EXPECT_FALSE(bad_message_observer.got_bad_message());
  }
}

// Tests that if "secure-payment-confirmation" is present in method_data
// with options requesting payer details or shipping details:
// - If the SPC feature is enabled, this is invalid and the renderer is
// terminated.
// - If the SPC feature is disabled, this is allowed and no termination occurs.
TEST_P(PaymentRequestValidationTest,
       SecurePaymentConfirmationWithPaymentOptions) {
  struct OptionsTestCase {
    bool request_shipping;
    bool request_payer_name;
    bool request_payer_phone;
    bool request_payer_email;
  };

  const std::vector<OptionsTestCase> test_cases = {
      {true, false, false, false}, {false, true, false, false},
      {false, false, true, false}, {false, false, false, true},
      {true, true, true, true},
  };

  for (const auto& test_case : test_cases) {
    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<mojom::PaymentRequest> payment_request;
    new PaymentRequest(CreateMockDelegate(),
                       payment_request.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::PaymentRequestClient> client_remote;
    auto dummy_receiver = client_remote.InitWithNewPipeAndPassReceiver();

    std::vector<mojom::PaymentMethodDataPtr> method_data;
    method_data.push_back(CreateSpcMethodData());

    auto options = mojom::PaymentOptions::New();
    options->request_shipping = test_case.request_shipping;
    options->request_payer_name = test_case.request_payer_name;
    options->request_payer_phone = test_case.request_payer_phone;
    options->request_payer_email = test_case.request_payer_email;

    payment_request->Init(std::move(client_remote), std::move(method_data),
                          CreateDummyDetails(), std::move(options));

    if (IsSecurePaymentConfirmationEnabled()) {
      bad_message_observer.WaitForBadMessage();
      EXPECT_TRUE(bad_message_observer.got_bad_message());
    } else {
      // The Mojo pipe should remain open, and no bad message should be
      // reported.
      EXPECT_TRUE(payment_request.is_connected());
      EXPECT_FALSE(bad_message_observer.got_bad_message());
    }
  }
}

// Tests that the payment details cannot be updated for a
// "secure-payment-confirmation" request after it has been shown, because the
// dialog displays a snapshot of the details and would not reflect changes made
// while it was visible.
TEST_P(PaymentRequestValidationTest,
       SecurePaymentConfirmationRejectsUpdateWith) {
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<mojom::PaymentRequest> payment_request;
  new PaymentRequest(CreateMockDelegate(),
                     payment_request.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::PaymentRequestClient> client_remote;
  auto dummy_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(CreateSpcMethodData());

  payment_request->Init(std::move(client_remote), std::move(method_data),
                        CreateDummyDetails(), mojom::PaymentOptions::New());
  payment_request->Show(/*wait_for_updated_details=*/false,
                        /*had_user_activation=*/true);

  auto updated_details = mojom::PaymentDetails::New();
  updated_details->total = mojom::PaymentItem::New();
  updated_details->total->label = "Total";
  updated_details->total->amount = mojom::PaymentCurrencyAmount::New();
  updated_details->total->amount->currency = "USD";
  updated_details->total->amount->value = "5000.00";
  payment_request->UpdateWith(std::move(updated_details));

  payment_request.FlushForTesting();
  EXPECT_TRUE(bad_message_observer.got_bad_message());
}

// Tests that updateWith() is allowed when resolving the promise passed into
// show() for "secure-payment-confirmation", because that update happens before
// the dialog displays the details.
TEST_P(PaymentRequestValidationTest,
       SecurePaymentConfirmationAllowsUpdateWithForShowPromise) {
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<mojom::PaymentRequest> payment_request;
  new PaymentRequest(CreateMockDelegate(),
                     payment_request.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::PaymentRequestClient> client_remote;
  auto dummy_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(CreateSpcMethodData());

  payment_request->Init(std::move(client_remote), std::move(method_data),
                        CreateDummyDetails(), mojom::PaymentOptions::New());
  payment_request->Show(/*wait_for_updated_details=*/true,
                        /*had_user_activation=*/true);

  auto updated_details = mojom::PaymentDetails::New();
  updated_details->total = mojom::PaymentItem::New();
  updated_details->total->label = "Total";
  updated_details->total->amount = mojom::PaymentCurrencyAmount::New();
  updated_details->total->amount->currency = "USD";
  updated_details->total->amount->value = "2.00";
  payment_request->UpdateWith(std::move(updated_details));

  payment_request.FlushForTesting();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

INSTANTIATE_TEST_SUITE_P(All, PaymentRequestValidationTest, testing::Bool());

}  // namespace
}  // namespace payments
