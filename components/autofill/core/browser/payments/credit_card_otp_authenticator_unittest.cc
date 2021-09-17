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
const char kTestNumber[] = "4234567890123456";
const char16_t kTestNumber16[] = u"4234567890123456";
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

    payments::TestPaymentsClient* payments_client =
        new payments::TestPaymentsClient(
            autofill_driver_->GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client));
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
    authenticator_->otp_ = u"123456";
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.real_pan = real_pan;
    response.dcvv = "123";
    response.expiration_month = test::NextMonth();
    response.expiration_year = test::NextYear();
    response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
    authenticator_->OnDidGetRealPan(result, response);
  }

 protected:
  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<CreditCardOtpAuthenticator> authenticator_;
};

TEST_F(CreditCardOtpAuthenticatorTest, AuthenticateServerCardSuccess) {
  CreditCard card = test::GetMaskedServerCard();
  authenticator_->Authenticate(&card, requester_->GetWeakPtr(),
                               /*context_token=*/"",
                               /*billing_customer_number=*/0);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber);
  EXPECT_TRUE(requester_->did_succeed());
  EXPECT_EQ(kTestNumber16, requester_->number());
}

// TODO(crbug.com/1243475): Add more tests for error cases once error handling
// is implemented.

}  // namespace autofill
