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
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer_test_api.h"
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
#include <optional>

#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
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
        /*enabled_features=*/{features::
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
                .full_card_request_for_testing()
                .GetUnmaskRequestDetailsForTesting()
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
      enrolled_card, /*require_user_confirmation=*/false,
      base::BindOnce(&TestAccessor::OnCreditCardFetched,
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
  const CreditCard* enrolled_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, /*cvc=*/u"",
                       CreditCard::RecordType::kMaskedServerCard,
                       /*is_card_info_retrieval_enrolled=*/true);

  credit_card_access_manager().FetchCreditCard(
      enrolled_card, /*require_user_confirmation=*/false,
      base::BindOnce(&TestAccessor::OnCreditCardFetched,
                     accessor().GetWeakPtr()));

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*enrolled_card));

  test_api(
      autofill_client().GetFormDataImporter()->GetPaymentsFormDataImporter())
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
      test_api(autofill_client()
                   .GetFormDataImporter()
                   ->GetPaymentsFormDataImporter())
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
      card, /*require_user_confirmation=*/false,
      base::BindOnce(&TestAccessor::OnCreditCardFetched,
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

constexpr char kRiskBasedTimedOutHistogram[] =
    "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.RiskBased."
    "TimedOut";
constexpr char kTimedOutCvcFallbackHistogram[] =
    "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
    "TimedOutCvcFallback";

// Tests the preflight wait and CVC enforcement that gate the authentication of
// a masked server card when card-on-device verification enforcement is enabled.
class CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest
    : public CreditCardAccessManagerTestBase {
 public:
  CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableFpanRiskBasedAuthentication,
         features::kAutofillEnableCardOnDeviceVerificationEnforcement},
        /*disabled_features=*/{});
  }

 protected:
  // Creates a masked server card and starts a preflight call whose response is
  // delayed until InvokeDelayedGetUnmaskDetailsResponse() is called.
  const CreditCard* SetUpCardWithPendingPreflightCall(
      bool card_is_verified = true) {
    const CreditCard* masked_server_card =
        CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
    if (card_is_verified) {
      payments_network_interface()
          .unmask_details()
          ->verified_instrument_ids.insert(kTestInstrumentId);
    }
    GetFIDOAuthenticator()->SetUserVerifiable(true);
    payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);
    PrepareToFetchCreditCardAndWaitForCallbacks();
    return masked_server_card;
  }

  bool risk_based_authentication_invoked() {
    return autofill_client()
        .GetPaymentsAutofillClient()
        ->risk_based_authentication_invoked();
  }

  bool autofill_progress_dialog_shown() {
    return autofill_client()
        .GetPaymentsAutofillClient()
        ->autofill_progress_dialog_shown();
  }

  bool autofill_progress_dialog_closed() {
    return autofill_client()
        .GetPaymentsAutofillClient()
        ->autofill_progress_dialog_closed();
  }

  base::test::ScopedFeatureList feature_list_;
};

// Ensures the progress dialog is shown and the risk-based authentication is
// held back while the preflight call is still in flight, and starts once the
// preflight call returns with the card in `verified_instrument_ids`.
TEST_F(
    CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
    PreflightCallPending_VerifiedCard_RiskBasedAuthWaitsBehindProgressDialog) {
  const CreditCard* masked_server_card =
      SetUpCardWithPendingPreflightCall(/*card_is_verified=*/true);

  FetchCreditCard(masked_server_card);

  // The progress dialog is shown while waiting, but the risk-based
  // authentication has not started yet.
  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_FALSE(risk_based_authentication_invoked());

  // Once the preflight call returns with the card marked as verified, the
  // risk-based authentication starts and the progress dialog stays open.
  InvokeDelayedGetUnmaskDetailsResponse();

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_FALSE(autofill_progress_dialog_closed());
}

// Ensures that a preflight call response that arrives while the risk-based
// authentication is waiting for it is recorded as not timed out.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       PreflightCallReturnsDuringWait_RecordsNoTimeout) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card = SetUpCardWithPendingPreflightCall();

  FetchCreditCard(masked_server_card);
  InvokeDelayedGetUnmaskDetailsResponse();

  EXPECT_TRUE(risk_based_authentication_invoked());
  histogram_tester.ExpectUniqueSample(kRiskBasedTimedOutHistogram, false, 1);
  // The risk-based authentication does not fall back to CVC authentication.
  histogram_tester.ExpectTotalCount(kTimedOutCvcFallbackHistogram, 0);
}

// Ensures that if the preflight call returns and the card is absent from
// `verified_instrument_ids`, risk-based authentication is skipped, the progress
// dialog is closed, and CVC authentication is immediately mandated.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       PreflightCallReturns_UnverifiedCard_MandatesCvcAuthImmediately) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      SetUpCardWithPendingPreflightCall(/*card_is_verified=*/false);

  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(autofill_progress_dialog_shown());
  ASSERT_FALSE(risk_based_authentication_invoked());

  InvokeDelayedGetUnmaskDetailsResponse();

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_progress_dialog_closed());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Ensures that once the wait for the preflight call times out, risk-based
// authentication is skipped, the progress dialog is closed, and CVC
// authentication is immediately mandated.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       PreflightCallTimesOut_MandatesCvcAuthImmediately) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      SetUpCardWithPendingPreflightCall(/*card_is_verified=*/true);

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_FALSE(risk_based_authentication_invoked());

  FastForwardBy(CreditCardAccessManager::kUnmaskDetailsResponseTimeout);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_progress_dialog_closed());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);
  // The user perceived the latency of the preflight call, so the base event
  // has to be logged alongside the timeout, reporting that the card was chosen
  // before the preflight call returned.
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedOut",
      autofill_metrics::PreflightCallEvent::
          kCardChosenBeforePreflightCallReturned,
      1);
  // The pre-existing risk-based metric keeps being logged as well.
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.PreflightCallResponseReceivedOnCardSelection."
      "OptedOut.ServerCard",
      autofill_metrics::PreflightCallEvent::
          kCardChosenBeforePreflightCallReturned,
      1);
  // The timeout is recorded for the risk-based authentication, not as a CVC
  // fallback.
  histogram_tester.ExpectUniqueSample(kRiskBasedTimedOutHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kTimedOutCvcFallbackHistogram, 0);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Ensures that a preflight call that returned before the card was selected does
// not delay the risk-based authentication when the card is verified.
TEST_F(
    CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
    PreflightCallAlreadyReturned_VerifiedCard_RiskBasedAuthStartsImmediately) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  payments_network_interface().unmask_details()->verified_instrument_ids.insert(
      kTestInstrumentId);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_TRUE(risk_based_authentication_invoked());
  // The user did not have to wait, but the base event is still logged so that
  // the `.Duration` samples can be put in relation to the total number of card
  // selections.
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedOut",
      autofill_metrics::PreflightCallEvent::
          kPreflightCallReturnedBeforeCardChosen,
      1);
  // There was no wait, so there is no timeout outcome to record.
  histogram_tester.ExpectTotalCount(kRiskBasedTimedOutHistogram, 0);
}

// Ensures that when a preflight call has already returned and the card is
// absent from `verified_instrument_ids`, CVC authentication is mandated
// immediately without invoking risk-based authentication or showing the
// progress dialog, even if the user is opted in to FIDO or the card is enrolled
// in CardInfoRetrieval.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       PreflightCallAlreadyReturned_UnverifiedCard_MandatesCvcAuthImmediately) {
  base::HistogramTester histogram_tester;
  OptUserInToFido();
  const CreditCard* masked_server_card = CreateServerCard(
      kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
      CreditCard::RecordType::kMaskedServerCard,
      /*is_card_info_retrieval_enrolled=*/true, kTestInstrumentId);
  payments_network_interface().AddFidoEligibleCard(kTestServerId, kCredentialId,
                                                   kGooglePaymentsRpid);
  // Only another card is verified on this device.
  payments_network_interface().unmask_details()->verified_instrument_ids.insert(
      kTestInstrumentId2);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(autofill_progress_dialog_shown());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);
}

// Ensures that if the wait times out while `IsUserVerifiable()` is still
// pending, risk-based authentication is skipped, the progress dialog is closed,
// and CVC authentication is immediately mandated.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       IsUserVerifiablePending_TimesOut_MandatesCvcAuthImmediately) {
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);
  // Start `PrepareToFetchCreditCard()` without running callbacks so
  // `IsUserVerifiable()` remains pending when the card is selected.
  credit_card_access_manager().PrepareToFetchCreditCard();

  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(autofill_progress_dialog_shown());
  ASSERT_FALSE(risk_based_authentication_invoked());

  FastForwardBy(CreditCardAccessManager::kUnmaskDetailsResponseTimeout);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_progress_dialog_closed());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
}

// Ensures that on a non-FIDO-verifiable device (where no preflight call is
// issued), Chrome proceeds with risk-based authentication so the server can
// enforce CVC verification or return a CardInfoRetrieval challenge.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       UserNotVerifiable_ProceedsToRiskBasedAuth) {
  const CreditCard* masked_server_card = CreateServerCard(
      kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
      CreditCard::RecordType::kMaskedServerCard,
      /*is_card_info_retrieval_enrolled=*/true, kTestInstrumentId);
  GetFIDOAuthenticator()->SetUserVerifiable(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_TRUE(risk_based_authentication_invoked());

  // When the server returns an SMS OTP challenge for a CardInfoRetrieval card,
  // Chrome shows the authenticator selection dialog rather than overriding with
  // client-side CVC enforcement.
  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse response;
  response.result = CreditCardRiskBasedAuthenticator::
      RiskBasedAuthenticationResponse::Result::kAuthenticationRequired;
  response.context_token = "fake_context_token";
  response.card_unmask_challenge_options = test::GetCardUnmaskChallengeOptions(
      {CardUnmaskChallengeOptionType::kSmsOtp});
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      response);
  EXPECT_TRUE(
      payments_autofill_client().unmask_authenticator_selection_dialog_shown());
}

// Ensures that on a non-FIDO-verifiable device (where no preflight call is
// issued), a regular masked server card (`is_card_info_retrieval_enrolled` =
// false) proceeds with risk-based authentication and undergoes CVC
// authentication when challenged by the server.
TEST_F(
    CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
    UserNotVerifiable_RegularServerCard_ProceedsToRiskBasedAuthCvcChallenge) {
  const CreditCard* masked_server_card = CreateServerCard(
      kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
      CreditCard::RecordType::kMaskedServerCard,
      /*is_card_info_retrieval_enrolled=*/false, kTestInstrumentId);
  GetFIDOAuthenticator()->SetUserVerifiable(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_TRUE(risk_based_authentication_invoked());

  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse response;
  response.result = CreditCardRiskBasedAuthenticator::
      RiskBasedAuthenticationResponse::Result::kAuthenticationRequired;
  response.context_token = "fake_context_token";
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      response);
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
}

// Ensures that cancelling the progress dialog while waiting for the preflight
// call ends the flow instead of starting the risk-based authentication.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       CancelWhileWaitingForPreflightCall_FlowIsCancelled) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card = SetUpCardWithPendingPreflightCall();

  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(autofill_progress_dialog_shown());
  ASSERT_FALSE(risk_based_authentication_invoked());

  autofill_client().GetPaymentsAutofillClient()->CancelAutofillProgressDialog();

  EXPECT_FALSE(IsAuthenticationInProgress());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);

  // A late preflight call response or timeout must not revive the flow.
  InvokeDelayedGetUnmaskDetailsResponse();
  FastForwardBy(CreditCardAccessManager::kUnmaskDetailsResponseTimeout);

  EXPECT_FALSE(risk_based_authentication_invoked());
  // The wait was cancelled rather than ended, so no timeout outcome is
  // recorded.
  histogram_tester.ExpectTotalCount(kRiskBasedTimedOutHistogram, 0);
}

// Ensures that `Autofill.ServerControlledReauth.CardVerificationStatus` logs
// `kVerified` once the preflight call returns with the card in
// `verified_instrument_ids`.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       CardVerificationStatusMetric_VerifiedCard) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      SetUpCardWithPendingPreflightCall(/*card_is_verified=*/true);

  FetchCreditCard(masked_server_card);
  histogram_tester.ExpectTotalCount(
      "Autofill.ServerControlledReauth.CardVerificationStatus", 0);

  InvokeDelayedGetUnmaskDetailsResponse();

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerControlledReauth.CardVerificationStatus",
      autofill_metrics::CardVerificationStatus::kVerified, 1);
}

// Ensures that `Autofill.ServerControlledReauth.CardVerificationStatus` logs
// `kUnverified` when the preflight call returns without the card in
// `verified_instrument_ids`.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       CardVerificationStatusMetric_UnverifiedCard) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      SetUpCardWithPendingPreflightCall(/*card_is_verified=*/false);

  FetchCreditCard(masked_server_card);
  InvokeDelayedGetUnmaskDetailsResponse();

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerControlledReauth.CardVerificationStatus",
      autofill_metrics::CardVerificationStatus::kUnverified, 1);
}

// Ensures that `Autofill.ServerControlledReauth.CardVerificationStatus` logs
// `kTimedOut` when the wait for the preflight call times out.
TEST_F(CreditCardAccessManagerCardOnDeviceVerificationEnforcementTest,
       CardVerificationStatusMetric_TimedOut) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      SetUpCardWithPendingPreflightCall(/*card_is_verified=*/true);

  FetchCreditCard(masked_server_card);
  FastForwardBy(CreditCardAccessManager::kUnmaskDetailsResponseTimeout);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerControlledReauth.CardVerificationStatus",
      autofill_metrics::CardVerificationStatus::kTimedOut, 1);
}

// Ensures that `Autofill.ServerControlledReauth.CardVerificationStatus` is not
// logged when card-on-device verification enforcement is disabled.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       CardVerificationStatusMetric_NotLoggedWhenFeatureDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kAutofillEnableCardOnDeviceVerificationEnforcement);
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  histogram_tester.ExpectTotalCount(
      "Autofill.ServerControlledReauth.CardVerificationStatus", 0);
}

// Ensures that the risk-based authentication is not delayed when the feature is
// disabled, even if the preflight call has not returned.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       FeatureDisabled_PreflightCallPending_RiskBasedAuthStartsImmediately) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kAutofillEnableCardOnDeviceVerificationEnforcement);
  const CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // The legacy risk-based flow does not wait for the preflight call, so no
  // latency is perceived and only the risk-based metric is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.PreflightCallResponseReceivedOnCardSelection."
      "OptedOut.ServerCard",
      autofill_metrics::PreflightCallEvent::
          kCardChosenBeforePreflightCallReturned,
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedOut", 0);
  histogram_tester.ExpectTotalCount(kRiskBasedTimedOutHistogram, 0);
}

constexpr char kFidoUnmaskResultHistogram[] =
    "Autofill.ServerCardUnmask.ServerCard.Result.Fido";
constexpr char kFidoEnrollmentUnmaskResultHistogram[] =
    "Autofill.ServerCardUnmask.ServerCard.Result.FidoEnrollment";
constexpr char kUnrecoverableErrorRecoveryHistogram[] =
    "Autofill.ServerControlledReauth.UnrecoverableErrorRecovery";

// Enables the features of CreditCardAccessManagerServerControlledReauthTest.
// As a base class that precedes CreditCardAccessManagerTestBase, it enables
// them before the TaskEnvironment of the latter starts its threads, which read
// the features when they start (e.g. on Android), and restores them only after
// those threads are joined.
class ServerControlledReauthFeatures {
 protected:
  base::test::ScopedFeatureList feature_list_{
      /*enabled_features=*/
      {features::kAutofillEnableFpanRiskBasedAuthentication,
       features::kAutofillEnableCardOnDeviceVerificationEnforcement,
       features::kAutofillEnableServerControlledMandatoryReauth},
      /*disabled_features=*/{}};
};

// Tests how a masked server card that is verified on this device is
// authenticated when the mandatory re-authentication setting stored on the
// Payments server decides it.
class CreditCardAccessManagerServerControlledReauthTest
    : private ServerControlledReauthFeatures,
      public CreditCardAccessManagerTestBase {
 protected:
  using MandatoryReauthStatus = payments::UnmaskDetails::MandatoryReauthStatus;

  // Creates a masked server card that is verified on this device and makes the
  // preflight call report `status` as the mandatory re-authentication status
  // of this device.
  const CreditCard* SetUpVerifiedCard(
      std::optional<MandatoryReauthStatus> status,
      bool is_card_info_retrieval_enrolled = false) {
    const CreditCard* masked_server_card =
        CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                         CreditCard::RecordType::kMaskedServerCard,
                         is_card_info_retrieval_enrolled, kTestInstrumentId);
    unmask_details().verified_instrument_ids.insert(kTestInstrumentId);
    unmask_details().mandatory_reauth_status = status;
    GetFIDOAuthenticator()->SetUserVerifiable(true);
    return masked_server_card;
  }

  // Makes the preflight call respond the way the server does for a device that
  // is enrolled into FIDO authentication: it suggests FIDO authentication and
  // provides the challenge to sign.
  void SetUpPreflightCallForEnrolledDevice() {
    unmask_details().unmask_auth_method =
        payments::PaymentsAutofillClient::UnmaskAuthMethod::kFido;
    payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
        kCredentialId, kGooglePaymentsRpid);
  }

  // Selects a card for which FIDO authentication is mandated on a device that
  // is enrolled into FIDO authentication.
  void FetchCardWithMandatedFidoVerification() {
    const CreditCard* masked_server_card =
        SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
    SetUpPreflightCallForEnrolledDevice();
    PrepareToFetchCreditCardAndWaitForCallbacks();
    FetchCreditCard(masked_server_card);
  }

  // Selects a card for which FIDO authentication is mandated on a device that
  // has to enroll into FIDO authentication by creating a credential.
  void FetchCardWithMandatedFidoEnrollment() {
    const CreditCard* masked_server_card =
        SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
    unmask_details().fido_creation_options = GetTestCreationOptions();
    PrepareToFetchCreditCardAndWaitForCallbacks();
    FetchCreditCard(masked_server_card);
  }

  payments::UnmaskDetails& unmask_details() {
    return *payments_network_interface().unmask_details();
  }

  // The request that unmasks the card, if it has been sent.
  const std::optional<payments::UnmaskRequestDetails>& unmask_request() {
    return payments_network_interface().unmask_request();
  }

  // The request that changes the FIDO authentication setting stored on the
  // server, if it has been sent.
  const std::optional<payments::OptChangeRequestDetails>& opt_change_request() {
    return payments_network_interface().opt_change_request();
  }

  bool risk_based_authentication_invoked() {
    return payments_autofill_client().risk_based_authentication_invoked();
  }

  bool autofill_progress_dialog_shown() {
    return payments_autofill_client().autofill_progress_dialog_shown();
  }

  bool autofill_progress_dialog_closed() {
    return payments_autofill_client().autofill_progress_dialog_closed();
  }

  // Whether the CVC prompt was shown as the only authentication.
  bool IsCvcAuthenticationInProgress() {
    return GetUnmaskAuthFlowType() == UnmaskAuthFlowType::kCvc;
  }

  // Expects that the unmasking ended without the card and without falling back
  // to another authentication.
  void ExpectUnmaskingAbortedWithoutFallback() {
    EXPECT_FALSE(IsAuthenticationInProgress());
    EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kNone);
    EXPECT_FALSE(
        GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
    EXPECT_FALSE(risk_based_authentication_invoked());
    EXPECT_TRUE(accessor().number().empty());
  }
};

// Ensures FIDO authentication is mandated when the setting is on: neither the
// progress dialog nor risk-based authentication is involved, the challenge of
// the preflight call is signed, and the assertion alone unmasks the card.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_MandatesFidoVerification) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(autofill_progress_dialog_shown());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kFido);
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_FALSE(fido_authenticator().context_token().has_value());

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  ASSERT_TRUE(unmask_request().has_value());
  EXPECT_TRUE(unmask_request()->fido_assertion_info.has_value());
  EXPECT_FALSE(unmask_request()->fido_authenticator_response.has_value());
  EXPECT_TRUE(unmask_request()->context_token.empty());

  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
  EXPECT_FALSE(IsAuthenticationInProgress());
}

// Ensures the progress dialog that covers the wait for the preflight call is
// closed before the mandated FIDO authentication starts.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_PreflightCallPending_ClosesProgressDialogBeforeFido) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(autofill_progress_dialog_shown());
  ASSERT_FALSE(fido_authenticator().authenticate_invoked());

  InvokeDelayedGetUnmaskDetailsResponse();

  EXPECT_TRUE(autofill_progress_dialog_closed());
  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_TRUE(fido_authenticator().authenticate_invoked());
}

// Ensures that nothing else is offered when the browser ends the WebAuthn
// prompt of the mandated FIDO authentication, e.g. because the page lost
// focus, which says nothing about the authenticator.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_FidoPromptEndedByBrowser_AbortsWithoutFallback) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::GetAssertionEndedByBrowser(
      GetFIDOAuthenticator());

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_FALSE(opt_change_request().has_value());
  EXPECT_FALSE(payments_autofill_client().autofill_error_dialog_shown());
}

// Ensures an error dialog, and no other authentication, follows a mandated
// FIDO authentication that the server declines.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_ServerDeclinesAssertion_ShowsErrorDialogWithoutFallback) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kPermanentFailure, ""));

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  EXPECT_EQ(
      payments_autofill_client().autofill_error_dialog_context().type,
      AutofillErrorDialogType::kMaskedServerCardFidoUnmaskingPermanentError);
}

// Ensures the error dialog tells the user to check their connection when the
// server could not be reached after the mandated FIDO authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_NetworkError_ShowsNetworkErrorDialogWithoutFallback) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kNetworkError, ""));

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  EXPECT_EQ(
      payments_autofill_client().autofill_error_dialog_context().type,
      AutofillErrorDialogType::kMaskedServerCardFidoUnmaskingNetworkError);
}

// Ensures an unexpected virtual card retrieval error in the response to the
// unmasking of a masked server card after the mandated FIDO authentication is
// handled like any other error instead of crashing Chrome.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_VirtualCardRetrievalTryAgainError_ShowsNetworkErrorDialog) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  EXPECT_TRUE(GetRealPanForFIDOAuth(
      PaymentsRpcResult::kVcnRetrievalTryAgainFailure, ""));

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  EXPECT_EQ(
      payments_autofill_client().autofill_error_dialog_context().type,
      AutofillErrorDialogType::kMaskedServerCardFidoUnmaskingNetworkError);
}

// Ensures the same for a permanent virtual card retrieval error.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_VirtualCardRetrievalPermanentError_ShowsPermanentErrorDialog) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  EXPECT_TRUE(GetRealPanForFIDOAuth(
      PaymentsRpcResult::kVcnRetrievalPermanentFailure, ""));

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  EXPECT_EQ(
      payments_autofill_client().autofill_error_dialog_context().type,
      AutofillErrorDialogType::kMaskedServerCardFidoUnmaskingPermanentError);
}

// Ensures CVC authentication is used when the setting is on but the preflight
// call did not provide a challenge that could be signed: the server would
// reject risk-based authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_NoValidRequestOptions_MandatesCvc) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  unmask_details().unmask_auth_method =
      payments::PaymentsAutofillClient::UnmaskAuthMethod::kFido;
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(autofill_progress_dialog_shown());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
  EXPECT_TRUE(IsCvcAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures risk-based authentication is used when the setting is off, whatever
// else the preflight call provided.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Disabled_UsesRiskBasedAuth) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kDisabled);
  SetUpPreflightCallForEnrolledDevice();
  unmask_details().fido_creation_options = GetTestCreationOptions();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
  EXPECT_FALSE(fido_authenticator().make_credential_invoked());
}

// Ensures a device that enrolled into FIDO authentication before the setting
// existed is treated as if the setting were on: FIDO authentication is
// mandated and the assertion alone unmasks the card.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_EnrolledDevice_MandatesFidoVerification) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  ASSERT_TRUE(unmask_request().has_value());
  EXPECT_TRUE(unmask_request()->fido_assertion_info.has_value());
  EXPECT_FALSE(unmask_request()->fido_authenticator_response.has_value());

  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures a device that is not enrolled into FIDO authentication has to enroll
// when the setting is unknown and the preflight call provided a challenge to
// create a credential: the new credential is created without involving the
// progress dialog or risk-based authentication, and its attestation is sent
// with the request that unmasks the card.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_CreationOptions_EnrollsWithNewCredential) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  unmask_details().fido_creation_options = GetTestCreationOptions();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(autofill_progress_dialog_shown());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kFido);
  EXPECT_EQ(GetFIDOAuthenticator()->current_flow(),
            CreditCardFidoAuthenticator::Flow::kEnrollmentDuringUnmaskFlow);
  ASSERT_TRUE(fido_authenticator().make_credential_invoked());

  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/true);
  ASSERT_TRUE(unmask_request().has_value());
  ASSERT_TRUE(unmask_request()->fido_authenticator_response.has_value());
  EXPECT_TRUE(unmask_request()->fido_authenticator_response->contains(
      "fido_attestation_info"));
  EXPECT_FALSE(unmask_request()->fido_assertion_info.has_value());

  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
  EXPECT_FALSE(IsAuthenticationInProgress());
}

// Ensures a device that is not enrolled into FIDO authentication has to enroll
// with a credential that it already has when the setting is unknown and the
// preflight call provided a challenge to sign: the assertion is sent with the
// request that unmasks the card in the form that also enrolls the device.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_RequestOptions_EnrollsWithExistingCredential) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_EQ(GetFIDOAuthenticator()->current_flow(),
            CreditCardFidoAuthenticator::Flow::kEnrollmentDuringUnmaskFlow);
  ASSERT_TRUE(fido_authenticator().get_assertion_invoked());

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  ASSERT_TRUE(unmask_request().has_value());
  ASSERT_TRUE(unmask_request()->fido_authenticator_response.has_value());
  EXPECT_TRUE(unmask_request()->fido_authenticator_response->contains(
      "fido_assertion_info"));
  EXPECT_FALSE(unmask_request()->fido_assertion_info.has_value());

  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures that nothing else is offered when the user cancels or fails the
// mandated FIDO enrollment.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_EnrollmentPromptFails_AbortsWithoutFallback) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  unmask_details().fido_creation_options = GetTestCreationOptions();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/false);

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_FALSE(opt_change_request().has_value());
  EXPECT_FALSE(payments_autofill_client().autofill_error_dialog_shown());
}

// Ensures risk-based authentication is used when the setting is unknown and
// the preflight call provided no FIDO challenge, which is what the server does
// for a device that cannot enroll.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_NoFidoOptions_UsesRiskBasedAuth) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_progress_dialog_shown());
  EXPECT_EQ(GetFIDOAuthenticator()->current_flow(),
            CreditCardFidoAuthenticator::Flow::kNoneFlow);
}

// Ensures a device is not enrolled into FIDO authentication from an
// off-the-record session, and that the authentication the server mandates is
// not replaced by risk-based authentication: CVC authentication is used.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_OffTheRecord_UsesCvcInsteadOfEnrolling) {
  autofill_client().set_is_off_the_record(true);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  unmask_details().fido_creation_options = GetTestCreationOptions();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().make_credential_invoked());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures the same for the enrollment that would sign with a credential the
// platform enrolled on its own.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_OffTheRecord_ExistingCredential_UsesCvcInsteadOfEnrolling) {
  autofill_client().set_is_off_the_record(true);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
}

// Ensures the mandated FIDO authentication of an enrolled device, which does
// not change any setting, also applies to an off-the-record session.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_OffTheRecord_MandatesFidoVerification) {
  autofill_client().set_is_off_the_record(true);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_TRUE(fido_authenticator().authenticate_invoked());
}

// Ensures risk-based authentication keeps being used when the preflight call
// does not report a status, i.e. the server does not control mandatory
// re-authentication, even for a device that is enrolled into FIDO
// authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       StatusNotReported_UsesRiskBasedAuth) {
  const CreditCard* masked_server_card = SetUpVerifiedCard(std::nullopt);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
}

// Ensures the setting is ignored when the feature is disabled.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       FeatureDisabled_UsesRiskBasedAuth) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kAutofillEnableServerControlledMandatoryReauth);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
}

// Ensures the setting is ignored when card-on-device verification is not
// enforced, as the setting only applies to cards verified on this device.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       CardOnDeviceVerificationNotEnforced_UsesRiskBasedAuth) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kAutofillEnableCardOnDeviceVerificationEnforcement);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
}

// Ensures a card that is not verified on this device is authenticated with CVC
// first, whatever the setting is.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_UnverifiedCard_MandatesCvc) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  unmask_details().verified_instrument_ids.clear();
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_FALSE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
  EXPECT_TRUE(IsCvcAuthenticationInProgress());
}

// Ensures a card that is enrolled in card info retrieval keeps being retrieved
// through risk-based authentication, whatever the setting is.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_CardInfoRetrievalEnrolledCard_UsesRiskBasedAuth) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled,
                        /*is_card_info_retrieval_enrolled=*/true);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();

  FetchCreditCard(masked_server_card);

  EXPECT_TRUE(risk_based_authentication_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
}

// Ensures the user is not opted out of FIDO authentication after a CVC
// authentication because the local FIDO opt-in preference lags behind the
// server: the server is authoritative when it controls mandatory
// re-authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_LocalPrefNotOptedIn_DoesNotOptOutAfterCvcAuth) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  unmask_details().verified_instrument_ids.clear();
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(IsCvcAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  EXPECT_EQ(accessor().number(), kTestNumber16);
  EXPECT_FALSE(GetFIDOAuthenticator()->IsOptOutCalled());
}

// Ensures the opt-out that follows such a mismatch is kept when the server
// does not control mandatory re-authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       StatusNotReported_LocalPrefNotOptedIn_OptsOutAfterCvcAuth) {
  const CreditCard* masked_server_card = SetUpVerifiedCard(std::nullopt);
  unmask_details().verified_instrument_ids.clear();
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(IsCvcAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
}

// Ensures such a mismatch also keeps opting the user out when card-on-device
// verification is not enforced, as the setting is ignored then.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       CardOnDeviceVerificationNotEnforced_LocalPrefNotOptedIn_OptsOut) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kAutofillEnableCardOnDeviceVerificationEnforcement);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(false);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_context_token("fake_context_token"));
  ASSERT_TRUE(IsCvcAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
}

// Ensures the local FIDO opt-in pref follows the server when the server
// controls mandatory re-authentication: it is set for a device that the server
// reports as enrolled into FIDO authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_LocalPrefFollowsServer) {
  SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(false);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  EXPECT_TRUE(IsCreditCardFIDOAuthEnabled());
}

// Ensures the local FIDO opt-in pref is cleared for a device that the server
// does not report as enrolled into FIDO authentication.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_NotEnrolledDevice_LocalPrefFollowsServer) {
  SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  unmask_details().fido_creation_options = GetTestCreationOptions();
  SetCreditCardFIDOAuthEnabled(true);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

// Ensures the local FIDO opt-in pref is left alone when the server does not
// control mandatory re-authentication: the mismatch with the server then
// expresses that the user intends to opt out.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       StatusNotReported_LocalPrefNotChanged) {
  SetUpVerifiedCard(std::nullopt);
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(false);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

// Ensures the user is not locked out of their cards when the authenticator of
// an enrolled device cannot be used: the server is asked to stop mandating
// FIDO authentication for this device, and CVC authentication takes over right
// away.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_UnrecoverableAuthenticatorError_ForceDisablesAndFallsBackToCvc) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::GetAssertionWithUnrecoverableError(
      GetFIDOAuthenticator());

  ASSERT_TRUE(opt_change_request().has_value());
  EXPECT_EQ(
      opt_change_request()->reason,
      payments::OptChangeRequestDetails::Reason::kForceDisableMandatoryReauth);
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvcFallbackFromFido);
  EXPECT_FALSE(payments_autofill_client().autofill_error_dialog_shown());

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  ASSERT_TRUE(unmask_request().has_value());
  EXPECT_TRUE(unmask_request()->context_token.empty());
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures NOT_ALLOWED_ERROR is treated as a cancellation of the mandated FIDO
// authentication, as that is what it most often is: nothing else is offered,
// and the server is not asked to change anything. WebAuthn reports an
// authenticator that no longer has the credential of this device the same way,
// so the unmasking also just ends then.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_FidoPromptNotAllowed_AbortsWithoutFallback) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_FALSE(opt_change_request().has_value());
  EXPECT_FALSE(payments_autofill_client().autofill_error_dialog_shown());
}

// Ensures the same for the mandated FIDO enrollment that signs with a
// credential the platform enrolled on its own: it ends without a request
// carrying a FIDO response, and nothing else is offered.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_EnrollmentWithExistingCredentialNotAllowed_Aborts) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  ASSERT_EQ(GetFIDOAuthenticator()->current_flow(),
            CreditCardFidoAuthenticator::Flow::kEnrollmentDuringUnmaskFlow);

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_FALSE(autofill_progress_dialog_shown());
  EXPECT_FALSE(unmask_request().has_value());
  EXPECT_FALSE(opt_change_request().has_value());
}

// Ensures that nothing else is offered when the browser ends the WebAuthn
// prompt of the mandated FIDO enrollment that signs with a credential the
// platform enrolled on its own, which says nothing about the credential.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_EnrollmentWithExistingCredentialEndedByBrowser_Aborts) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::GetAssertionEndedByBrowser(
      GetFIDOAuthenticator());

  ExpectUnmaskingAbortedWithoutFallback();
  EXPECT_FALSE(autofill_progress_dialog_shown());
}

// Ensures the setting stored on the server is not changed from an
// off-the-record session: only CVC authentication takes over when the
// authenticator of an enrolled device cannot be used.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Enabled_OffTheRecord_UnrecoverableAuthenticatorError_FallsBackToCvc) {
  autofill_client().set_is_off_the_record(true);
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  SetUpPreflightCallForEnrolledDevice();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::GetAssertionWithUnrecoverableError(
      GetFIDOAuthenticator());

  EXPECT_FALSE(opt_change_request().has_value());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvcFallbackFromFido);
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures CVC authentication takes over when a device cannot enroll into FIDO
// authentication because its authenticator cannot be used. The server does not
// mandate FIDO authentication for this device yet, so it is not asked to
// change anything.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Unknown_UnrecoverableAuthenticatorErrorOnEnrollment_FallsBackToCvc) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  unmask_details().fido_creation_options = GetTestCreationOptions();
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::MakeCredentialWithUnrecoverableError(
      GetFIDOAuthenticator());

  EXPECT_FALSE(opt_change_request().has_value());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvcFallbackFromFido);
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor().number(), kTestNumber16);
}

// Ensures the server is not asked to change anything when FIDO authentication
// was not mandated: a FIDO authentication that follows a risk-based
// authentication falls back to CVC authentication for any failure.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       StatusNotReported_UnrecoverableAuthenticatorError_FallsBackToCvc) {
  const CreditCard* masked_server_card = SetUpVerifiedCard(std::nullopt);
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(true);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));
  ASSERT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kFido);

  TestCreditCardFidoAuthenticator::GetAssertionWithUnrecoverableError(
      GetFIDOAuthenticator());

  EXPECT_FALSE(opt_change_request().has_value());
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvcFallbackFromFido);
}

// Ensures a card unmasked by a mandated FIDO authentication is logged as
// unmasked with FIDO authentication only.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoVerification_Unmasked) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoVerification();
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  ASSERT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      kFidoUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures a mandated FIDO authentication whose WebAuthn prompt the browser
// ended is logged as a cancelled unmasking, without any recovery.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoVerification_PromptEndedByBrowser) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoVerification();

  TestCreditCardFidoAuthenticator::GetAssertionEndedByBrowser(
      GetFIDOAuthenticator());

  histogram_tester.ExpectUniqueSample(
      kFidoUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
  histogram_tester.ExpectTotalCount(kUnrecoverableErrorRecoveryHistogram, 0);
}

// Ensures a mandated FIDO authentication that ended with NOT_ALLOWED_ERROR is
// logged as a cancelled unmasking, without any recovery.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoVerification_PromptNotAllowed) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoVerification();

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  histogram_tester.ExpectUniqueSample(
      kFidoUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
  histogram_tester.ExpectTotalCount(kUnrecoverableErrorRecoveryHistogram, 0);
}

// Ensures a mandated FIDO authentication after which the server did not
// return the card is logged as an authentication error.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoVerification_ServerDeclines) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoVerification();
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  ASSERT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kPermanentFailure, ""));

  histogram_tester.ExpectUniqueSample(
      kFidoUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationError, 1);
}

// Ensures a card unmasked by the request that enrolls the device into FIDO
// authentication is logged with a flow type of its own.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoEnrollment_Unmasked) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoEnrollment();
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/true);

  ASSERT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      kFidoEnrollmentUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
  histogram_tester.ExpectTotalCount(kFidoUnmaskResultHistogram, 0);
}

// Ensures a mandated FIDO enrollment whose WebAuthn prompt the user cancelled
// or did not pass is logged as a cancelled unmasking.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoEnrollment_PromptFails) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoEnrollment();

  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/false);

  histogram_tester.ExpectUniqueSample(
      kFidoEnrollmentUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
  histogram_tester.ExpectTotalCount(kUnrecoverableErrorRecoveryHistogram, 0);
}

// Ensures a mandated FIDO enrollment that the server did not accept is logged
// as an authentication error.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoEnrollment_ServerDeclines) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoEnrollment();
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/true);

  ASSERT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kPermanentFailure, ""));

  histogram_tester.ExpectUniqueSample(
      kFidoEnrollmentUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationError, 1);
}

// Ensures it is logged that the server was asked to stop mandating FIDO
// authentication after the authenticator of an enrolled device could not be
// used. The unmasking goes on with CVC authentication, so it has no result
// yet.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_UnrecoverableAuthenticatorError_ForceDisableRequested) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoVerification();

  TestCreditCardFidoAuthenticator::GetAssertionWithUnrecoverableError(
      GetFIDOAuthenticator());

  histogram_tester.ExpectUniqueSample(kUnrecoverableErrorRecoveryHistogram,
                                      true, 1);
  histogram_tester.ExpectTotalCount(kFidoUnmaskResultHistogram, 0);
}

// Ensures it is logged that the server was not asked to change anything after
// the authenticator of an enrolled device could not be used in an
// off-the-record session.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_UnrecoverableAuthenticatorError_OffTheRecord) {
  base::HistogramTester histogram_tester;
  autofill_client().set_is_off_the_record(true);
  FetchCardWithMandatedFidoVerification();

  TestCreditCardFidoAuthenticator::GetAssertionWithUnrecoverableError(
      GetFIDOAuthenticator());

  histogram_tester.ExpectUniqueSample(kUnrecoverableErrorRecoveryHistogram,
                                      false, 1);
}

// Ensures it is logged that the server was not asked to change anything after
// a device could not enroll because its authenticator could not be used. The
// device is not enrolled into FIDO authentication, so the server does not
// mandate it for this device and there is nothing to force-disable; doing so
// would only wipe the CVC verifications of its cards. CVC authentication takes
// over.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_UnrecoverableAuthenticatorError_OnEnrollment) {
  base::HistogramTester histogram_tester;
  FetchCardWithMandatedFidoEnrollment();

  TestCreditCardFidoAuthenticator::MakeCredentialWithUnrecoverableError(
      GetFIDOAuthenticator());

  histogram_tester.ExpectUniqueSample(kUnrecoverableErrorRecoveryHistogram,
                                      false, 1);
  histogram_tester.ExpectTotalCount(kFidoEnrollmentUnmaskResultHistogram, 0);
}

// Ensures a mandated FIDO enrollment that signs with a credential the platform
// enrolled on its own and that ended with NOT_ALLOWED_ERROR is logged as a
// cancelled unmasking, without any recovery.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoEnrollmentWithExistingCredential_NotAllowed) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kUnknown);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  histogram_tester.ExpectUniqueSample(
      kFidoEnrollmentUnmaskResultHistogram,
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
  histogram_tester.ExpectTotalCount(kUnrecoverableErrorRecoveryHistogram, 0);
}

// Ensures a card unmasked by a FIDO authentication that was not mandated,
// because it follows a risk-based authentication, keeps being logged without
// a flow type.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       Metrics_FidoNotMandated) {
  base::HistogramTester histogram_tester;
  const CreditCard* masked_server_card = SetUpVerifiedCard(std::nullopt);
  SetUpPreflightCallForEnrolledDevice();
  SetCreditCardFIDOAuthEnabled(true);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  ASSERT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
  histogram_tester.ExpectTotalCount(kFidoUnmaskResultHistogram, 0);
}

// Ensures the way the setting required one card to be authenticated does not
// leak into the next card: after a mandated CVC authentication, a failed FIDO
// authentication of a virtual card still falls back to the other challenge
// options of the virtual card, of which there are none here.
TEST_F(CreditCardAccessManagerServerControlledReauthTest,
       MandatedFlowDoesNotApplyToNextCard) {
  const CreditCard* masked_server_card =
      SetUpVerifiedCard(MandatoryReauthStatus::kEnabled);
  unmask_details().unmask_auth_method =
      payments::PaymentsAutofillClient::UnmaskAuthMethod::kFido;
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(masked_server_card);
  ASSERT_TRUE(IsCvcAuthenticationInProgress());
  ASSERT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  const CreditCard* virtual_card =
      CreateServerCard(kTestGUID2, kTestNumber2, kTestServerId2, kTestCvc16,
                       CreditCard::RecordType::kVirtualCard);
  fido_authenticator().set_is_user_opted_in(true);
  FetchCreditCard(virtual_card);
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));
  ASSERT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kFido);

  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  EXPECT_EQ(payments_autofill_client().autofill_error_dialog_context().type,
            AutofillErrorDialogType::kVirtualCardPermanentError);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
