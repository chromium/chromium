// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_info_retrieval_enrolled_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_base.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif


namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
using autofill_metrics::CreditCardFormEventLogger;

class CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest
    : public CreditCardAccessManagerTestBase {
 public:
  CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableCardInfoRuntimeRetrieval,
                              features::
                                  kAutofillEnableFpanRiskBasedAuthentication},
        /*disabled_features=*/{});
  }
  ~CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest() override =
      default;

  base::test::ScopedFeatureList feature_list_;

  void MockRiskBasedAuthSucceedsWithoutPanReturned(
      const CreditCard* card,
      std::string context_token = "fake_context_token") {
    FetchCreditCard(card);

    // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
    // invoked.
    EXPECT_TRUE(autofill_client()
                    .GetPaymentsAutofillClient()
                    ->risk_based_authentication_invoked());
    EXPECT_TRUE(autofill_client()
                    .GetPaymentsAutofillClient()
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
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId);

  FetchCreditCard(masked_server_card);

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
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
  EXPECT_EQ(accessor().number(), base::UTF8ToUTF16(test_number));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client().GetFormDataImporter())
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
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);

  FetchCreditCard(masked_server_card);

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a red path.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::kError));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationError, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardInfoRetrievalEnrolled.Result",
      autofill_metrics::CardInfoRetrievalEnrolledUnmaskResult::
          kAuthenticationError,
      1);
}

// Ensures that the masked server card risk-based unmasking response is
// handled correctly if the flow is cancelled.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_FlowCancelled) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);

  FetchCreditCard(masked_server_card);

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock the authentication is cancelled.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationCancelled));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardInfoRetrievalEnrolled.Result",
      autofill_metrics::CardInfoRetrievalEnrolledUnmaskResult::kFlowCancelled,
      1);
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

  FetchCreditCard(masked_server_card);

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client()
                   .GetPaymentsAutofillClient()
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
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
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

  FetchCreditCard(masked_server_card);

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
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
      test_api(*autofill_client().GetFormDataImporter())
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
  PrepareToFetchCreditCardAndWaitForCallbacks();

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.CvcThenFido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor().number(), std::u16string());
  EXPECT_EQ(accessor().cvc(), std::u16string());

  // Mock user response.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kFollowupAfterCvcAuthFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

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
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
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
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  FetchCreditCard(masked_server_card);

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
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId, u"");
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  FetchCreditCard(masked_server_card);

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
  CreditCard masked_server_card =
      *personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card.SetExpirationYearFromString(u"2010");
  personal_data().payments_data_manager().UpdateCreditCard(masked_server_card);

  FetchCreditCard(&masked_server_card);
  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client()
                   .GetPaymentsAutofillClient()
                   ->risk_based_authentication_invoked());
}

// Test the green path flow when the masked server card enrolled in card info
// retrieval is successfully returned from the server during a risk-based
// retrieval.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       CardInfoRetrievalUnmasking_Success) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  std::string test_number = "4444333322221111";
  const CreditCard* enrolled_card =
      CreateServerCard(kTestGUID, test_number, kTestServerId, /*cvc=*/u"",
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);

  credit_card_access_manager().FetchCreditCard(
      enrolled_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                    accessor().GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*enrolled_card));

  // Ensure the accessor received the correct response.
  EXPECT_EQ(accessor().number(), base::UTF8ToUTF16(test_number));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  EXPECT_THAT(type, testing::Optional(
                        NonInteractivePaymentMethodType::kMaskedServerCard));
}

// Testing metrics for cards enroilled in runtime retrieval.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       CardInfoRetrievalUnmasking_Success_Metrics) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  const CreditCard* enrolled_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, /*cvc=*/u"",
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);

  credit_card_access_manager().FetchCreditCard(
      enrolled_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                    accessor().GetWeakPtr()));

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*enrolled_card));

  test_api(*autofill_client().GetFormDataImporter())
      .payment_method_type_if_non_interactive_authentication_flow_completed();

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardInfoRetrievalEnrolled.Result",
      autofill_metrics::CardInfoRetrievalEnrolledUnmaskResult::
          kRiskBasedUnmasked,
      1);
}

// Test the yellow path flow when the masked server card enrolled in card info
// retrieval is retrieved from the server with Sms Otp authentication.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       CardInfoRetrievalUnmasking_AuthenticationRequired_OtpOnly) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0,
      CreditCard::RecordType::kMaskedServerCard,
      /*is_card_info_retrieval_enrolled=*/true);

  const CreditCard* enrolled_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);
  credit_card_access_manager().OnOtpAuthenticationComplete(
      CreditCardOtpAuthenticator::OtpAuthenticationResponse()
          .with_result(CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                           Result::kSuccess)
          .with_card(enrolled_card)
          .with_cvc(kTestCvc16));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect accessor to successfully retrieve the CVC.
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Test the yellow path flow when the masked server card enrolled in card info
// retrieval is retrieved from the server with Sms Otp authentication via
// multiple phone number options.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    CardInfoRetrievalUnmasking_AuthenticationRequired_OtpOnly_MultiplePhoneNumbers) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0,
      CreditCard::RecordType::kMaskedServerCard,
      /*is_card_info_retrieval_enrolled=*/true);

  const CreditCard* enrolled_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);
  credit_card_access_manager().OnOtpAuthenticationComplete(
      CreditCardOtpAuthenticator::OtpAuthenticationResponse()
          .with_result(CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                           Result::kSuccess)
          .with_card(enrolled_card)
          .with_cvc(kTestCvc16));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect accessor to successfully retrieve the CVC.
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Params of the CardInfoRetrievalUnmaskingYellowPathMetricsTest:
// -- CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result result
class CardInfoRetrievalUnmaskingYellowPathMetricsTest
    : public CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
      public testing::WithParamInterface<
          CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result> {
 public:
  CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result result() {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CardInfoRetrievalUnmaskingYellowPathMetricsTest,
    testing::Values(
        CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess,
        CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
            kFlowCancelled,
        CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
            kGenericError,
        CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
            kAuthenticationError));

// Testing autofill metrics for runtime retrieval cards in otp authentication
// flow.
TEST_P(CardInfoRetrievalUnmaskingYellowPathMetricsTest,
       CardInfoRetrievalUnmasking_AuthenticationRequired_Metrics) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0,
      CreditCard::RecordType::kMaskedServerCard,
      /*is_card_info_retrieval_enrolled=*/true);

  const CreditCard* enrolled_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);
  credit_card_access_manager().OnOtpAuthenticationComplete(
      CreditCardOtpAuthenticator::OtpAuthenticationResponse()
          .with_result(result())
          .with_card(enrolled_card)
          .with_cvc(kTestCvc16));

  autofill_metrics::CardInfoRetrievalEnrolledUnmaskResult
      expected_retrieval_enrolled_result;
  // Expect the metrics are logged correctly.
  if (result() ==
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.ServerCardUnmask.VirtualCard.Attempt", false, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
        autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ServerCardUnmask.ServerCard.Attempt", true, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ServerCardUnmask.ServerCard.Result.Otp",
        autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
    expected_retrieval_enrolled_result = autofill_metrics::
        CardInfoRetrievalEnrolledUnmaskResult::kAuthenticationUnmasked;

  } else if (result() == CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                             Result::kFlowCancelled) {
    expected_retrieval_enrolled_result =
        autofill_metrics::CardInfoRetrievalEnrolledUnmaskResult::kFlowCancelled;
  } else if (result() == CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                             Result::kGenericError) {
    expected_retrieval_enrolled_result = autofill_metrics::
        CardInfoRetrievalEnrolledUnmaskResult::kUnexpectedError;
  } else {
    expected_retrieval_enrolled_result = autofill_metrics::
        CardInfoRetrievalEnrolledUnmaskResult::kAuthenticationError;
  }
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardInfoRetrievalEnrolled.Result",
      expected_retrieval_enrolled_result, 1);
}

// Test the yellow path flow when the masked server card enrolled in card info
// retrieval is retrieved and server returns non SMS OTP Challenge which is not
// yet supported. Then the card info retrieval authentication is skipped.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       CardInfoRetrievalUnmasking_NonSmsOtpChallenge_SelectionDialogSkipped) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // 3DS is the unsupported challenge option here.
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kThreeDomainSecure});

  // Mocking card unmask flow.
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                   CreditCard::RecordType::kMaskedServerCard,
                   /*is_card_info_retrieval_enrolled=*/true);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor().GetWeakPtr()));

  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse response;
  response.result = CreditCardRiskBasedAuthenticator::
      RiskBasedAuthenticationResponse::Result::kAuthenticationRequired;
  response.context_token = "fake_context_token";
  response.card_unmask_challenge_options = challenge_options;

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      response);

  // Selection dialog should not be shown if an unsupported authentication
  // method was returned for the card.
  EXPECT_FALSE(
      payments_autofill_client().unmask_authenticator_selection_dialog_shown());
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
  FetchCreditCard(masked_server_card);
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
