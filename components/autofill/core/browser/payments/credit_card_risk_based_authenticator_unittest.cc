// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
constexpr std::string_view kTestNumber = "4234567890123456";
constexpr int64_t kTestBillingCustomerNumber = 123456;
}  // namespace

class CreditCardRiskBasedAuthenticatorTest : public testing::Test {
 public:
  CreditCardRiskBasedAuthenticatorTest() = default;
  ~CreditCardRiskBasedAuthenticatorTest() override = default;

  void SetUp() override {
    requester_ = std::make_unique<TestAuthenticationRequester>();
    autofill_client_.set_test_payments_client(
        std::make_unique<payments::TestPaymentsClient>(
            autofill_client_.GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data_manager_));
    authenticator_ =
        std::make_unique<CreditCardRiskBasedAuthenticator>(&autofill_client_);
    card_ = test::GetMaskedServerCard();
  }

  void OnUnmaskResponseReceived(AutofillClient::PaymentsRpcResult result,
                                absl::string_view real_pan) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
      response.real_pan = real_pan;
      response.card_type = AutofillClient::PaymentsRpcCardType::kServerCard;
    }
    authenticator_->OnUnmaskResponseReceivedForTesting(result, response);
  }

 protected:
  payments::TestPaymentsClient* payments_client() {
    return static_cast<payments::TestPaymentsClient*>(
        autofill_client_.GetPaymentsClient());
  }

  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<CreditCardRiskBasedAuthenticator> authenticator_;
  CreditCard card_;
};

// Ensure the UnmaskRequestDetails is populated with the correct contents when
// we initiate a risk based authentication flow.
TEST_F(CreditCardRiskBasedAuthenticatorTest, UnmaskRequestSetCorrectly) {
  authenticator_->Authenticate(card_, kTestBillingCustomerNumber,
                               requester_->GetWeakPtr());

  EXPECT_TRUE(payments_client()->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client()->unmask_request()->risk_data.empty());
}

// Test that risk-based authentication returns the full PAN upon success.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardSuccess) {
  authenticator_->Authenticate(card_, kTestBillingCustomerNumber,
                               requester_->GetWeakPtr());

  // Simulate server returns success with card returned and invoke the callback.
  OnUnmaskResponseReceived(AutofillClient::PaymentsRpcResult::kSuccess,
                           kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(requester_->did_succeed().value());
  EXPECT_EQ(kTestNumber, base::UTF16ToUTF8(requester_->number()));
}

// Test that risk-based authentication doesn't return the full PAN when the
// server call fails.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardFailure) {
  authenticator_->Authenticate(card_, kTestBillingCustomerNumber,
                               requester_->GetWeakPtr());

  // Simulate server returns failure and invoke the callback.
  OnUnmaskResponseReceived(AutofillClient::PaymentsRpcResult::kPermanentFailure,
                           kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(requester_->did_succeed().value());
  EXPECT_TRUE(requester_->number().empty());
}

// Test that risk-based authentication doesn't return the full PAN when the user
// cancels risk-based authentication.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticationCancelled) {
  authenticator_->Authenticate(card_, kTestBillingCustomerNumber,
                               requester_->GetWeakPtr());

  // Simulate user cancels the authentication.
  authenticator_->OnCardUnmaskCancelledForTesting();
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(requester_->did_succeed().value());
  EXPECT_TRUE(requester_->number().empty());
}

}  // namespace autofill
