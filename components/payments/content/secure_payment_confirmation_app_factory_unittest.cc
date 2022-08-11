// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_app_factory.h"

#include <vector>

#include "base/base64.h"
#include "components/payments/content/mock_payment_app_factory_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

using ::testing::_;

static constexpr char kChallengeBase64[] = "aaaa";
static constexpr char kCredentialIdBase64[] = "cccc";

class SecurePaymentConfirmationAppFactoryTest : public testing::Test {
 protected:
  SecurePaymentConfirmationAppFactoryTest() = default;

  void SetUp() override {
    ASSERT_TRUE(base::Base64Decode(kChallengeBase64, &challenge_bytes_));
    ASSERT_TRUE(base::Base64Decode(kCredentialIdBase64, &credential_id_bytes_));
  }

  // Creates and returns a minimal SecurePaymentConfirmationRequest object with
  // only required fields filled in to pass parsing.
  //
  // Note that this method adds a payee_origin but *not* a payee_name, as only
  // one of the two are required.
  mojom::SecurePaymentConfirmationRequestPtr
  CreateSecurePaymentConfirmationRequest() {
    auto spc_request = mojom::SecurePaymentConfirmationRequest::New();

    spc_request->credential_ids.emplace_back(credential_id_bytes_.begin(),
                                             credential_id_bytes_.end());
    spc_request->challenge =
        std::vector<uint8_t>(challenge_bytes_.begin(), challenge_bytes_.end());
    spc_request->instrument = blink::mojom::PaymentCredentialInstrument::New();
    spc_request->instrument->display_name = "1234";
    spc_request->instrument->icon = GURL("https://site.example/icon.png");
    spc_request->payee_origin =
        url::Origin::Create(GURL("https://merchant.example"));
    spc_request->rp_id = "rp.example";

    return spc_request;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  SecurePaymentConfirmationAppFactory secure_payment_confirmation_app_factory_;
  std::string challenge_bytes_;
  std::string credential_id_bytes_;
};

// Test that parsing a valid SecureConfirmationPaymentRequest succeeds.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_IsValid) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _)).Times(0);
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// credentialIds field fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyCredentialIds) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->credential_ids.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty ID inside
// the credentialIds field fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyId) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->credential_ids.emplace_back();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty challenge
// fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyChallenge) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->challenge.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// displayName fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyDisplayName) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->display_name.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// icon fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->icon = GURL();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an invalid
// icon URL fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_InvalidIcon) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->instrument->icon =
      GURL("not-a-url");
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with an empty
// RP domain fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyRpId) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->rp_id.clear();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a missing payeeName
// and payeeOrigin fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_MissingPayeeNameAndPayeeOrigin) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->payee_name.reset();
  method_data->secure_payment_confirmation->payee_origin.reset();
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a present but empty
// payeeName fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_EmptyPayeeName) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->payee_name = "";
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}

// Test that parsing a SecurePaymentConfirmationRequest with a non-HTTPS
// payeeOrigin fails.
TEST_F(SecurePaymentConfirmationAppFactoryTest,
       SecureConfirmationPaymentRequest_NonHttpsPayeeOrigin) {
  auto method_data = mojom::PaymentMethodData::New();
  method_data->supported_method = "secure-payment-confirmation";
  method_data->secure_payment_confirmation =
      CreateSecurePaymentConfirmationRequest();
  method_data->secure_payment_confirmation->payee_origin =
      url::Origin::Create(GURL("http://site.example"));
  auto mock_delegate = std::make_unique<MockPaymentAppFactoryDelegate>(
      std::move(method_data), &context_);

  EXPECT_CALL(*mock_delegate, OnPaymentAppCreationError(_, _));
  secure_payment_confirmation_app_factory_.Create(mock_delegate->GetWeakPtr());
}
}  // namespace
}  // namespace payments
