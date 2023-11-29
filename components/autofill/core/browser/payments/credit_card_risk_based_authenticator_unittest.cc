// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
constexpr std::string_view kTestNumber = "4234567890123456";
// Base64 encoding of "This is a test challenge".
constexpr std::string_view kTestChallenge = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
// Base64 encoding of "This is a test Credential ID".
constexpr std::string_view kCredentialId =
    "VGhpcyBpcyBhIHRlc3QgQ3JlZGVudGlhbCBJRC4=";
constexpr std::string_view kGooglePaymentsRpid = "google.com";
}  // namespace

class CreditCardRiskBasedAuthenticatorTest : public testing::Test {
 public:
  CreditCardRiskBasedAuthenticatorTest() = default;
  ~CreditCardRiskBasedAuthenticatorTest() override = default;

  void SetUp() override {
    requester_ = std::make_unique<TestAuthenticationRequester>();
    autofill_client_.set_test_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            autofill_client_.GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data_manager_));
    authenticator_ =
        std::make_unique<CreditCardRiskBasedAuthenticator>(&autofill_client_);
    card_ = test::GetMaskedServerCard();
  }

  base::Value::Dict GetTestRequestOptions() {
    base::Value::Dict request_options;
    request_options.Set("challenge", base::Value(kTestChallenge));
    request_options.Set("relying_party_id", base::Value(kGooglePaymentsRpid));

    base::Value::Dict key_info;
    key_info.Set("credential_id", base::Value(kCredentialId));
    request_options.Set("key_info", base::Value(base::Value::Type::LIST));
    request_options.FindList("key_info")->Append(std::move(key_info));
    return request_options;
  }

 protected:
  payments::TestPaymentsNetworkInterface* payments_network_interface() {
    return static_cast<payments::TestPaymentsNetworkInterface*>(
        autofill_client_.GetPaymentsNetworkInterface());
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

  EXPECT_TRUE(
      payments_network_interface()->unmask_request()->context_token.empty());
  EXPECT_FALSE(
      payments_network_interface()->unmask_request()->risk_data.empty());
}

// Ensures that ServerCard authentication attempts are logged correctly.
TEST_F(CreditCardRiskBasedAuthenticatorTest,
       AuthServerCardAttemptLoggedCorrectly) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.ServerCard.Attempt", true, 1);
}

// Test that risk-based authentication returns the full PAN upon success.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardSuccess) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  // Mock server response with valid masked server card information.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.card_type = AutofillClient::PaymentsRpcCardType::kServerCard;
  response.real_pan = kTestNumber;

  authenticator_->OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);
  EXPECT_EQ(requester_->risk_based_authentication_response().result,
            CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
                Result::kNoAuthenticationRequired);
  EXPECT_EQ(
      kTestNumber,
      base::UTF16ToUTF8(
          requester_->risk_based_authentication_response().card->number()));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.ServerCard.Result",
      autofill_metrics::RiskBasedAuthEvent::kNoAuthenticationRequired, 1);
}

// Test that risk-based authentication doesn't return the full PAN when the
// server call fails.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardFailure) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  // Payment server response when unmask request fails is empty.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;

  authenticator_->OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, response);
  EXPECT_EQ(requester_->risk_based_authentication_response().result,
            CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
                Result::kError);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.ServerCard.Result",
      autofill_metrics::RiskBasedAuthEvent::kUnexpectedError, 1);
}

// Test that the requester receives `kAuthenticationCancelled` response when the
// risk-based authentication was cancelled.
TEST_F(CreditCardRiskBasedAuthenticatorTest, AuthenticateServerCardCancelled) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  authenticator_->OnUnmaskCancelled();
  EXPECT_EQ(requester_->risk_based_authentication_response().result,
            CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
                Result::kAuthenticationCancelled);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.ServerCard.Result",
      autofill_metrics::RiskBasedAuthEvent::kAuthenticationCancelled, 1);
}

// Test that risk-based authentication determines authentication is required
// when the server call succeeds and the PAN is not returned.
TEST_F(CreditCardRiskBasedAuthenticatorTest,
       AuthenticateServerCardSuccess_PanNotReturned) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  // Mock server response with context token.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";

  authenticator_->OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);
  EXPECT_EQ(requester_->risk_based_authentication_response().result,
            CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
                Result::kAuthenticationRequired);
  EXPECT_FALSE(
      requester_->risk_based_authentication_response().card.has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.ServerCard.Result",
      autofill_metrics::RiskBasedAuthEvent::kAuthenticationRequired, 1);
}

// Test that risk-based authentication determines authentication is required
// when the server call succeeds and the `fido_request_options` is returned.
TEST_F(CreditCardRiskBasedAuthenticatorTest,
       AuthenticateServerCardSuccess_FidoReturned) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(card_, requester_->GetWeakPtr());

  // Mock server response with FIDO request options.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.fido_request_options = GetTestRequestOptions();

  authenticator_->OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);
  EXPECT_EQ(requester_->risk_based_authentication_response().result,
            CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse::
                Result::kAuthenticationRequired);
  EXPECT_FALSE(
      requester_->risk_based_authentication_response().card.has_value());
  EXPECT_TRUE(requester_->risk_based_authentication_response()
                  .fido_request_options.has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.ServerCard.Result",
      autofill_metrics::RiskBasedAuthEvent::kAuthenticationRequired, 1);
}

// Ensures that VirtualCard authentication attempts are logged correctly.
TEST_F(CreditCardRiskBasedAuthenticatorTest,
       AuthVirtualCardAttemptLoggedCorrectly) {
  base::HistogramTester histogram_tester;
  authenticator_->Authenticate(test::GetVirtualCard(),
                               requester_->GetWeakPtr());

  histogram_tester.ExpectUniqueSample(
      "Autofill.RiskBasedAuth.VirtualCard.Attempt", true, 1);
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
  EXPECT_TRUE(
      payments_network_interface()->unmask_request()->context_token.empty());
  EXPECT_FALSE(
      payments_network_interface()->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_network_interface()
                  ->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with valid virtual card information.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails mocked_response;
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
  EXPECT_TRUE(
      payments_network_interface()->unmask_request()->context_token.empty());
  EXPECT_FALSE(
      payments_network_interface()->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_network_interface()
                  ->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());
  // Payment server response when unmask request fails is empty.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails mocked_response;

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
  EXPECT_TRUE(
      payments_network_interface()->unmask_request()->context_token.empty());
  EXPECT_FALSE(
      payments_network_interface()->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_network_interface()
                  ->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());
  std::vector<ClientBehaviorConstants> signals =
      payments_network_interface()->unmask_request()->client_behavior_signals;
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
