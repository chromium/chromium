// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_base.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
using autofill_metrics::CreditCardFormEventLogger;

class CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest
    : public CreditCardAccessManagerTestBase {
 public:
  CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest() = default;
  ~CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest() override =
      default;

  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableFpanRiskBasedAuthentication};

  void MockRiskBasedAuthSucceedsWithoutPanReturned(
      const CreditCard* card,
      std::string context_token = "fake_context_token") {
    credit_card_access_manager().FetchCreditCard(
        card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                             accessor_->GetWeakPtr()));

    // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
    // invoked.
    EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                    ->risk_based_authentication_invoked());
    EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                    ->autofill_progress_dialog_shown());

    // Mock that
    // CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
    // indicates a yellow path with context token returned.
    credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
        CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
            .with_result(CreditCardRiskBasedAuthenticator::
                             RiskBasedAuthenticationResponse::Result::
                                 kAuthenticationRequired)
            .with_context_token(context_token));
  }
};

// Test the flow when the masked server card is successfully returned from
// the server during a risk-based retrieval.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_Success) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Ensure the accessor received the correct response.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(accessor_->number(), base::UTF8ToUTF16(test_number));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  EXPECT_THAT(type, testing::Optional(
                        NonInteractivePaymentMethodType::kMaskedServerCard));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
}

// Ensures that the masked server card risk-based unmasking response is
// handled correctly if the retrieval failed.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_RetrievalError) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a red path.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::kError));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kUnexpectedError, 1);
}

// Ensures that the masked server card risk-based unmasking response is
// handled correctly if the flow is cancelled.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_FlowCancelled) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock the authentication is cancelled.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationCancelled));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
}

// Ensures that the masked server card risk-based authentication is not invoked
// when the feature is disabled.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_RiskBasedAuthenticationNotInvoked_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(
      features::kAutofillEnableFpanRiskBasedAuthentication);
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->risk_based_authentication_invoked());
}

TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_CvcAuthenticationRequired_ContextTokenSetCorrectly) {
  std::string context_token = "context_token";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card,
                                              context_token);

  // Expect the context_token is set in the full card request.
  EXPECT_EQ(GetCvcAuthenticator()
                .GetFullCardRequest()
                ->GetUnmaskRequestDetailsForTesting()
                ->context_token,
            context_token);
}

// Ensures the authentication is delegated to the CVC authenticator when
// `fido_request_options` is not returned.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_CvcOnly) {
  base::HistogramTester histogram_tester;

  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId);

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number));
  // Ensure that the form is filled.
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Ensures the masked server card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// `fido_request_options` is returned.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_FidoOnly) {
  base::HistogramTester histogram_tester;

  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId);
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(true);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a yellow path when `fido_request_options` and `context_token` are
  // returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Fido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(fido_authenticator().card().number(),
            base::UTF8ToUTF16(test_number));
  EXPECT_EQ(fido_authenticator().card().record_type(),
            CreditCard::RecordType::kMaskedServerCard);
  ASSERT_TRUE(fido_authenticator().context_token().has_value());
  EXPECT_EQ(fido_authenticator().context_token().value(), "fake_context_token");

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.CardUnmaskTypeDecision",
      autofill_metrics::CardUnmaskTypeDecisionMetric::kFidoOnly, 1);
}

// Ensures that use of new card invokes authorization flow when user is
// opted-in to FIDO.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_CvcThenFido) {
  base::HistogramTester histogram_tester;

  OptUserInToFido();
  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId);

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.CvcThenFido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::FOLLOWUP_AFTER_CVC_AUTH_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Mock OptChange payments call.
  OptChange(PaymentsRpcResult::kSuccess, true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.CardUnmaskTypeDecision",
      autofill_metrics::CardUnmaskTypeDecisionMetric::kCvcThenFido, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.WebauthnResult.AuthenticationAfterCVC",
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.BetterAuth.FlowEvents.CvcThenFido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that the kCvc instead of kCvcThenFido flow is invoked if
// GetUnmaskDetails preflight call is not finished.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_PreflightCallNotFinished) {
  base::HistogramTester histogram_tester;

  OptUserInToFido();
  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId);

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);
  credit_card_access_manager().PrepareToFetchCreditCard();

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number));
  // Ensure that the form is filled.
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

// Ensures that CVC filling gets logged after masked server card risk-based
// unmasking success if the card has CVC.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       LogCvcFilling_RiskBasedMaskedServerCardUnmaskingSuccess) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->set_cvc(kTestCvc16);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Mock CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse to
  // successfully return the valid card number.
  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 1);
}

// Ensures that CVC filling doesn't get logged after after masked server card
// risk-based unmasking success if the card doesn't have CVC.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       DoNotLogCvcFilling_RiskBasedMaskedServerCardUnmaskingSuccess) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->set_cvc(u"");

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Mock CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse to
  // successfully return the valid card number.
  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 0);
}

// Ensures that the masked server card risk-based authentication is not invoked
// when the card is expired.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_RiskBasedAuthenticationNotInvoked_CardExpired) {
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->SetExpirationYearFromString(u"2010");

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->risk_based_authentication_invoked());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Params of the
// CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest:
// -- bool fido_opted_in;
// -- bool preflight_call_returned;
class
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest
    : public CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest() =
      default;
  ~CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest()
      override = default;

  bool FidoOptedIn() { return std::get<0>(GetParam()); }
  bool PreflightCallReturned() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest,
    testing::Combine(testing::Bool(), testing::Bool()));

// Ensures that the metric for if the preflight call's response is received
// before card selection is logged correctly.
TEST_P(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest,
    Metrics_LogPreflightCallResponseReceivedOnCardSelection) {
  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  if (FidoOptedIn()) {
    OptUserInToFido();
  }
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(
      PreflightCallReturned());

  std::string histogram_name =
      "Autofill.BetterAuth.PreflightCallResponseReceivedOnCardSelection.";
  histogram_name += FidoOptedIn() ? "OptedIn." : "OptedOut.";
  histogram_name += "ServerCard";

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));
  WaitForCallbacks();

  histogram_tester.ExpectUniqueSample(
      histogram_name,
      PreflightCallReturned() ? autofill_metrics::PreflightCallEvent::
                                    kPreflightCallReturnedBeforeCardChosen
                              : autofill_metrics::PreflightCallEvent::
                                    kCardChosenBeforePreflightCallReturned,
      1);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
