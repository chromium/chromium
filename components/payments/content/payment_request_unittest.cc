// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
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

  bool IsSecurePaymentConfirmationEnabled() const { return GetParam(); }
};

// Tests that if "secure-payment-confirmation" is present in method_data
// alongside other methods:
// - If the SPC feature is enabled, this is invalid and the renderer is
// terminated.
// - If the SPC feature is disabled, this is allowed and no termination occurs.
TEST_P(PaymentRequestValidationTest, SecurePaymentConfirmationExclusivity) {
  base::test::ScopedFeatureList scoped_features;
  if (IsSecurePaymentConfirmationEnabled()) {
    scoped_features.InitAndEnableFeature(
        ::features::kSecurePaymentConfirmation);
  } else {
    scoped_features.InitAndDisableFeature(
        ::features::kSecurePaymentConfirmation);
  }

  GURL page_url("https://a.com");
  NavigateAndCommit(page_url);
  content::RenderFrameHost* rfh = main_rfh();

  PaymentRequestDisplayManager display_manager;

  auto delegate =
      std::make_unique<testing::NiceMock<MockContentPaymentRequestDelegate>>();
  ON_CALL(*delegate, GetRenderFrameHost()).WillByDefault(testing::Return(rfh));
  ON_CALL(*delegate, GetDisplayManager())
      .WillByDefault(testing::Return(&display_manager));
  ON_CALL(*delegate, GetApplicationLocale())
      .WillByDefault(testing::ReturnRefOfCopy(std::string("en-US")));
  ON_CALL(*delegate, GetLastCommittedURL())
      .WillByDefault(testing::ReturnRef(page_url));

  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<mojom::PaymentRequest> payment_request;
  new PaymentRequest(std::move(delegate),
                     payment_request.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::PaymentRequestClient> client_remote;
  auto dummy_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  std::vector<mojom::PaymentMethodDataPtr> method_data;

  auto spc_method = mojom::PaymentMethodData::New();
  spc_method->supported_method = "secure-payment-confirmation";
  spc_method->stringified_data = "{}";
  method_data.push_back(std::move(spc_method));

  auto dummy_method = mojom::PaymentMethodData::New();
  dummy_method->supported_method = "dummy-method";
  dummy_method->stringified_data = "{}";
  method_data.push_back(std::move(dummy_method));

  auto details = mojom::PaymentDetails::New();
  details->id = "12345";
  details->total = mojom::PaymentItem::New();
  details->total->label = "Total";
  details->total->amount = mojom::PaymentCurrencyAmount::New();
  details->total->amount->currency = "USD";
  details->total->amount->value = "1.00";

  auto options = mojom::PaymentOptions::New();

  payment_request->Init(std::move(client_remote), std::move(method_data),
                        std::move(details), std::move(options));

  if (IsSecurePaymentConfirmationEnabled()) {
    EXPECT_EQ(
        "If present, secure-payment-confirmation must be the only payment "
        "method",
        bad_message_observer.WaitForBadMessage());
  } else {
    // The Mojo pipe should remain open, and no bad message should be reported.
    EXPECT_TRUE(payment_request.is_connected());
    EXPECT_FALSE(bad_message_observer.got_bad_message());
  }
}

INSTANTIATE_TEST_SUITE_P(All, PaymentRequestValidationTest, testing::Bool());

}  // namespace
}  // namespace payments
