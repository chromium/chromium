// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
constexpr std::string_view kTestNumber = "4234567890123456";
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
                                absl::string_view real_pan,
                                AutofillClient::PaymentsRpcCardType card_type) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
      response.real_pan = real_pan;
      response.card_type = card_type;
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
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  EXPECT_TRUE(payments_client()->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client()->unmask_request()->risk_data.empty());
}

// Test that risk-based authentication returns the full PAN upon success.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardSuccess) {
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  // Simulate server returns success with card returned and invoke the callback.
  OnUnmaskResponseReceived(AutofillClient::PaymentsRpcResult::kSuccess,
                           kTestNumber,
                           AutofillClient::PaymentsRpcCardType::kServerCard);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(requester_->did_succeed().value());
  EXPECT_EQ(kTestNumber, base::UTF16ToUTF8(requester_->number()));
}

// Test that risk-based authentication doesn't return the full PAN when the
// server call fails.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardFailure) {
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  // Simulate server returns failure and invoke the callback.
  OnUnmaskResponseReceived(AutofillClient::PaymentsRpcResult::kPermanentFailure,
                           kTestNumber,
                           AutofillClient::PaymentsRpcCardType::kServerCard);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(requester_->did_succeed().value());
  EXPECT_TRUE(requester_->number().empty());
}

// Test a success risk based virtual card unmask request.
TEST_F(CreditCardRiskBasedAuthenticatorTest, VirtualCardUnmaskSuccess) {
  // Name on Card: Lorem Ipsum;
  // Card Number: 5555555555554444, Mastercard;
  // Expiration Year: The next year of current time;
  // Expiration Month: 10;
  // CVC: 123;
  constexpr std::string_view kTestVirtualCardNumber = "4234567890123456";
  CreditCard card = test::GetVirtualCard();
  card.set_cvc(u"123");
  card.SetExpirationYearFromString(base::UTF8ToUTF16(test::NextYear()));
  authenticator_->Authenticate(card, requester_->GetWeakPtr());
  EXPECT_TRUE(payments_client()->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client()->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client()
                  ->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with valid virtual card information.
  payments::PaymentsClient::UnmaskResponseDetails mocked_response;
  mocked_response.real_pan = kTestVirtualCardNumber;
  mocked_response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
  mocked_response.expiration_year = test::NextYear();
  mocked_response.expiration_month = "10";
  mocked_response.dcvv = "123";

  authenticator_->OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, mocked_response);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(requester_->did_succeed().value());
  EXPECT_EQ(mocked_response.real_pan, requester_->response_details().real_pan);
  EXPECT_EQ(mocked_response.card_type,
            requester_->response_details().card_type);
  EXPECT_EQ(mocked_response.expiration_year,
            requester_->response_details().expiration_year);
  EXPECT_EQ(mocked_response.expiration_month,
            requester_->response_details().expiration_month);
  EXPECT_EQ(mocked_response.dcvv, requester_->response_details().dcvv);
}

// Test a failed risk based virtual card unmask request.
TEST_F(CreditCardRiskBasedAuthenticatorTest, VirtualCardUnmaskFailure) {
  // Name on Card: Lorem Ipsum;
  // Card Number: 5555555555554444, Mastercard;
  // Expiration Year: The next year of current time;
  // Expiration Month: 10;
  // CVC: 123;
  CreditCard card = test::GetVirtualCard();
  card.set_cvc(u"123");
  card.SetExpirationYearFromString(base::UTF8ToUTF16(test::NextYear()));
  authenticator_->Authenticate(card, requester_->GetWeakPtr());
  EXPECT_TRUE(payments_client()->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client()->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client()
                  ->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());
  // Payment server response when unmask request fails is empty.
  payments::PaymentsClient::UnmaskResponseDetails mocked_response;

  authenticator_->OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, mocked_response);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(requester_->did_succeed().value());
  EXPECT_TRUE(requester_->response_details().real_pan.empty());
  EXPECT_EQ(AutofillClient::PaymentsRpcCardType::kUnknown,
            requester_->response_details().card_type);
  EXPECT_TRUE(requester_->response_details().expiration_year.empty());
  EXPECT_TRUE(requester_->response_details().expiration_month.empty());
  EXPECT_TRUE(requester_->response_details().dcvv.empty());
}

// Params of the CreditCardRiskBasedAuthenticatorCardMetadataTest:
// -- bool card_name_available;
// -- bool card_art_available;
// -- bool metadata_enabled;
class CreditCardRiskBasedAuthenticatorCardMetadataTest
    : public CreditCardRiskBasedAuthenticatorTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  CreditCardRiskBasedAuthenticatorCardMetadataTest() = default;
  ~CreditCardRiskBasedAuthenticatorCardMetadataTest() override = default;

  bool CardNameAvailable() { return std::get<0>(GetParam()); }
  bool CardArtAvailable() { return std::get<1>(GetParam()); }
  bool MetadataEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardRiskBasedAuthenticatorCardMetadataTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(CreditCardRiskBasedAuthenticatorCardMetadataTest, MetadataSignal) {
  base::test::ScopedFeatureList metadata_feature_list;
  CreditCard virtual_card = test::GetVirtualCard();
  if (MetadataEnabled()) {
    metadata_feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableCardProductName,
                              features::kAutofillEnableCardArtImage},
        /*disabled_features=*/{});
  } else {
    metadata_feature_list.InitWithFeaturesAndParameters(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillEnableCardProductName,
                               features::kAutofillEnableCardArtImage});
  }
  if (CardNameAvailable()) {
    virtual_card.set_product_description(u"Fake card product name");
  }
  if (CardArtAvailable()) {
    virtual_card.set_card_art_url(GURL("https://www.example.com"));
  }

  authenticator_->Authenticate(virtual_card, requester_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client()->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client()->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client()
                  ->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());
  std::vector<ClientBehaviorConstants> signals =
      payments_client()->unmask_request()->client_behavior_signals;
  if (MetadataEnabled() && CardNameAvailable() && CardArtAvailable()) {
    EXPECT_NE(
        signals.end(),
        base::ranges::find(
            signals,
            ClientBehaviorConstants::kShowingCardArtImageAndCardProductName));
  } else {
    EXPECT_TRUE(signals.empty());
  }
}

}  // namespace autofill
