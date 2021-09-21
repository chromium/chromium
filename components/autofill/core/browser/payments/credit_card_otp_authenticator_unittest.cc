// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
const char kTestChallengeId[] = "arbitrary challenge id";
const char kTestNumber[] = "4234567890123456";
const char16_t kTestNumber16[] = u"4234567890123456";
const char16_t kMaskedPhoneNumber[] = u"(***)-***-5678";
}  // namespace

class CreditCardOtpAuthenticatorTest : public testing::Test {
 public:
  CreditCardOtpAuthenticatorTest() = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_manager_.Init(/*profile_database=*/nullptr,
                                /*account_database=*/nullptr,
                                /*pref_service=*/autofill_client_.GetPrefs(),
                                /*local_state=*/autofill_client_.GetPrefs(),
                                /*identity_manager=*/nullptr,
                                /*client_profile_validator=*/nullptr,
                                /*history_service=*/nullptr,
                                /*strike_database=*/nullptr,
                                /*image_fetcher=*/nullptr,
                                /*is_off_the_record=*/false);
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());

    requester_ = std::make_unique<TestAuthenticationRequester>();
    autofill_driver_ = std::make_unique<TestAutofillDriver>();

    payments_client_ = new payments::TestPaymentsClient(
        autofill_driver_->GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client_));
    authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&autofill_client_);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();
    personal_data_manager_.SetPrefService(nullptr);
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.real_pan = real_pan;
    response.dcvv = "123";
    response.expiration_month = test::NextMonth();
    response.expiration_year = test::NextYear();
    response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
    authenticator_->OnDidGetRealPan(result, response);
  }

  std::string OtpAuthenticatorContextToken() {
    return authenticator_->context_token_;
  }

 protected:
  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  TestPersonalDataManager personal_data_manager_;
  payments::TestPaymentsClient* payments_client_;
  std::unique_ptr<CreditCardOtpAuthenticator> authenticator_;
};

TEST_F(CreditCardOtpAuthenticatorTest, AuthenticateServerCardSuccess) {
  CreditCard card = test::GetMaskedServerCard();
  card.set_record_type(CreditCard::VIRTUAL_CARD);
  CardUnmaskChallengeOption selected_otp_challenge_option;
  selected_otp_challenge_option.type = CardUnmaskChallengeOptionType::kSmsOtp;
  selected_otp_challenge_option.id = kTestChallengeId;
  selected_otp_challenge_option.challenge_info = kMaskedPhoneNumber;

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card, selected_otp_challenge_option, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/1111111);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  // TODO(crbug.com/1243475): Verify the otp dialog is shown.

  // Simulate user provide the OTP and click 'Confirm' in the OTP dialog.
  // TestPaymentsClient just stores the unmask request detail, won't invoke the
  // callback. OnDidGetRealPan below will manually invoke the callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"123456");
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"123456");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());

  // Simulate server returns success and invoke the callback.
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber);
  EXPECT_TRUE(requester_->did_succeed());
  EXPECT_EQ(kTestNumber16, requester_->number());
}

// TODO(crbug.com/1243475): Add more tests for error cases once error handling
// is implemented.

}  // namespace autofill
