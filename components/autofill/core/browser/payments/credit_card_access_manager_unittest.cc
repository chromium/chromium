// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_base.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/mock_credit_card_access_manager_observer.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/payments/test/mock_payments_window_manager.h"
#include "components/autofill/core/browser/payments/test/mock_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/test/test_credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"
#endif


namespace autofill {
namespace {

using autofill_metrics::CreditCardFormEventLogger;
using ::base::ASCIIToUTF16;
using test::AsFullServerCard;
using test::AsVirtualCard;
using test::WithCvc;
using ::testing::_;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Ref;

using PaymentsRpcCardType =
    payments::PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
std::string BytesToBase64(const std::vector<uint8_t>& bytes) {
  return base::Base64Encode(bytes);
}
#endif

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

using CreditCardAccessManagerTest = CreditCardAccessManagerTestBase;

// Params of the
// CreditCardAccessManagerAuthFlowTest:
// -- bool masked_server_card_risk_based_auth_enabled;
class CreditCardAccessManagerAuthFlowTest
    : public testing::WithParamInterface<bool>,
      public CreditCardAccessManagerTestBase {
 public:
  CreditCardAccessManagerAuthFlowTest() = default;
  ~CreditCardAccessManagerAuthFlowTest() override = default;

  bool IsMaskedServerCardRiskBasedAuthEnabled() { return GetParam(); }

  void FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      const CreditCard* card,
      base::Value::Dict fido_request_options = base::Value::Dict()) {
    CreditCardAccessManagerTestBase::FetchCreditCard(card);
    if (!IsMaskedServerCardRiskBasedAuthEnabled()) {
      return;
    }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
        CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
            .with_result(CreditCardRiskBasedAuthenticator::
                             RiskBasedAuthenticationResponse::Result::
                                 kAuthenticationRequired)
            .with_fido_request_options(std::move(fido_request_options))
            .with_context_token("fake context token"));
#endif
  }

 protected:
  void SetUp() override {
    CreditCardAccessManagerTestBase::SetUp();
    if (IsMaskedServerCardRiskBasedAuthEnabled()) {
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
      GTEST_SKIP() << "Skipping test because masked server card risk-based "
                      "flow should only happen on WIN, MAC or ANDROID";
#endif
    }
    feature_list_.InitWithFeatureState(
        features::kAutofillEnableFpanRiskBasedAuthentication,
        IsMaskedServerCardRiskBasedAuthEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerAuthFlowTest,
                         testing::Bool());

// Tests retrieving local cards.
TEST_F(CreditCardAccessManagerTest, FetchLocalCardSuccess) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  CreateLocalCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  PrepareToFetchCreditCardAndWaitForCallbacks();

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalSuccess(*card, *card, observer);
  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCard(card);
  credit_card_access_manager().RemoveObserver(&observer);

  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  ASSERT_TRUE(type.has_value());
  ASSERT_EQ(type.value(), NonInteractivePaymentMethodType::kLocalCard);
}

// Tests that fetching a local card correctly updates the fetched payments data
// context in FormDataImporter. Exists to ensure crbug.com/448461590 stays
// fixed.
TEST_F(CreditCardAccessManagerTest, FetchLocalCard_UpdatesPaymentsContext) {
  int64_t instrument_id = 12345;
  CreditCard local_card = test::GetCreditCard();
  local_card.set_instrument_id(instrument_id);
  personal_data().payments_data_manager().AddCreditCard(local_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(
          local_card.guid());
  auto* form_data_importer = autofill_client().GetFormDataImporter();
  ASSERT_TRUE(form_data_importer);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(card);

  const auto& context = form_data_importer->fetched_payments_data_context();
  EXPECT_EQ(context.fetched_card_instrument_id, instrument_id);
  ASSERT_TRUE(context.card_was_fetched_from_cache.has_value());
  EXPECT_FALSE(*context.card_was_fetched_from_cache);
}

// Tests that `CCAM::Observer::OnCreditCardAccessManagerDestroyed` is called
// when CCAM is destroyed.
TEST_F(CreditCardAccessManagerTest, CallsObserverOnDestruction) {
  NiceMock<MockCreditCardAccessManagerObserver> observer;
  EXPECT_CALL(observer, OnCreditCardAccessManagerDestroyed).WillOnce([&]() {
    credit_card_access_manager().RemoveObserver(&observer);
  });
  credit_card_access_manager().AddObserver(&observer);
  autofill_manager().Reset();
}

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// response from payments.
TEST_P(CreditCardAccessManagerAuthFlowTest, FetchServerCardCVCSuccess) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  base::HistogramTester histogram_tester;
  std::string flow_events_histogram_name = "Autofill.BetterAuth.FlowEvents.Cvc";
  PrepareToFetchCreditCardAndWaitForCallbacks();

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalSuccess(*card, AsFullServerCard(*card), observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  credit_card_access_manager().RemoveObserver(&observer);
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Attempt", true, 1);

  // Expect that we did not signal that there was no interactive
  // authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_P(CreditCardAccessManagerAuthFlowTest, FetchServerCardCVCNetworkError) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(*card, observer);

  credit_card_access_manager().AddObserver(&observer);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  EXPECT_TRUE(
      GetRealPanForCVCAuth(PaymentsRpcResult::kNetworkError, std::string()));
  credit_card_access_manager().RemoveObserver(&observer);

  EXPECT_THAT(accessor().number(), IsEmpty());
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardCVCPermanentFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(*card, observer);

  credit_card_access_manager().AddObserver(&observer);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kPermanentFailure,
                                   std::string()));
  credit_card_access_manager().RemoveObserver(&observer);

  EXPECT_THAT(accessor().number(), IsEmpty());
}

// Tests that the observer receives `OnCreditCardFetchFailed` events for
// retrievals that not completed on `CreditCardAccessManager` destruction.
TEST_P(CreditCardAccessManagerAuthFlowTest, ResetsOnDestruction) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(*card, observer);
  EXPECT_CALL(observer, OnCreditCardAccessManagerDestroyed).WillOnce([&]() {
    credit_card_access_manager().RemoveObserver(&observer);
  });

  credit_card_access_manager().AddObserver(&observer);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  autofill_manager().Reset();
}

// Ensures that a "try again" response from payments does not end the flow.
TEST_P(CreditCardAccessManagerAuthFlowTest, FetchServerCardCVCTryAgainFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalSuccess(*card, AsFullServerCard(*card), observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  EXPECT_TRUE(
      GetRealPanForCVCAuth(PaymentsRpcResult::kTryAgainFailure, std::string()));
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  credit_card_access_manager().RemoveObserver(&observer);

  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Ensures that FetchCreditCard() returns the full PAN upon a successful
// WebAuthn verification and response from payments.
TEST_P(CreditCardAccessManagerAuthFlowTest, FetchServerCardFIDOSuccess) {
  base::HistogramTester histogram_tester;
  std::string unmask_decision_histogram_name =
      "Autofill.BetterAuth.CardUnmaskTypeDecision";
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Fido";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // FIDO Success.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kAuthenticationFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  EXPECT_EQ(kCredentialId,
            BytesToBase64(GetFIDOAuthenticator()->GetCredentialId()));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  histogram_tester.ExpectUniqueSample(
      unmask_decision_histogram_name,
      autofill_metrics::CardUnmaskTypeDecisionMetric::kFidoOnly, 1);
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido.ServerCard.Success", 1);
  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that CVC filling gets logged after FIDO success if the card has CVC.
TEST_P(CreditCardAccessManagerAuthFlowTest, LogCvcFillingFIDOSuccess) {
  base::HistogramTester histogram_tester;

  CreditCard server_card = WithCvc(test::GetMaskedServerCard());
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          server_card.instrument_id());
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kFido, 1);
}

// Ensures that CVC filling doesn't get logged after FIDO success if the card
// doesn't have CVC.
TEST_P(CreditCardAccessManagerAuthFlowTest, DoNotLogCvcFillingFIDOSuccess) {
  base::HistogramTester histogram_tester;

  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_cvc(u"");
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          server_card.instrument_id());
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kFido, 0);
}

// Ensures that CVC filling gets logged if a card with CVC is retrieved with
// non-interactive authentication.
TEST_F(CreditCardAccessManagerTest,
       LogCvcFillingWithoutInteractiveAuthentication) {
  base::HistogramTester histogram_tester;
  CreditCard local_card = WithCvc(test::GetCreditCard());
  personal_data().payments_data_manager().AddCreditCard(local_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(
          local_card.guid());

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 1);
}

// Ensures that CVC filling doesn't get logged if a card without CVC is
// retrieved with non-interactive authentication
TEST_F(CreditCardAccessManagerTest,
       DoNotLogCvcFillingWithoutInteractiveAuthentication) {
  base::HistogramTester histogram_tester;
  CreditCard local_card = test::GetCreditCard();
  personal_data().payments_data_manager().AddCreditCard(local_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(
          local_card.guid());

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 0);
}

// Ensures that accessor retrieve empty CVC upon a successful
// WebAuthn verification and response from payments using masked server card
// without CVC.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardWithoutCvcFIDOSuccess) {
  CreditCard server_card = CreditCard();
  test::SetCreditCardInfo(&server_card, "Elvis Presley", kTestNumber,
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1", u"");
  server_card.set_guid(kTestGUID);
  server_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  // FIDO Success.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kAuthenticationFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(u"", accessor().cvc());
}

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// WebAuthn verification and response from payments.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardFIDOSuccessWithDcvv) {
  // Opt user in for FIDO auth.
  prefs::SetCreditCardFIDOAuthEnabled(autofill_client().GetPrefs(), true);

  // General setup.
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  // Mock Payments response that includes DCVV along with Full PAN.
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                    kTestCvc));

  // Expect accessor to successfully retrieve the DCVV.
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Ensures that CVC prompt is invoked after WebAuthn fails.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardFIDOVerificationFailureCVCFallback) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";
  std::string flow_events_fido_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Fido";
  std::string flow_events_cvc_fallback_histogram_name =
      "Autofill.BetterAuth.FlowEvents.CvcFallbackFromFido";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  histogram_tester.ExpectUniqueSample(
      flow_events_fido_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // FIDO Failure.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kAuthenticationFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  histogram_tester.ExpectBucketCount(
      flow_events_cvc_fallback_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_FALSE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  // Followed by a fallback to CVC.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kNoneFlow,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kNotAllowedError, 1);
  histogram_tester.ExpectBucketCount(
      flow_events_fido_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 0);
  histogram_tester.ExpectBucketCount(
      flow_events_cvc_fallback_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that CVC prompt is invoked after payments returns an error from
// GetRealPan via FIDO.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardFIDOServerFailureCVCFallback) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions());
  WaitForCallbacks();

  // FIDO Failure.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kAuthenticationFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(
      GetRealPanForFIDOAuth(PaymentsRpcResult::kPermanentFailure, kTestNumber));

  // Followed by a fallback to CVC.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kNoneFlow,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido.ServerCard.Failure", 1);
}

// Ensures WebAuthn call is not made if Request Options is missing a Credential
// ID, and falls back to CVC.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardBadRequestOptionsCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  // Don't set Credential ID.
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), /*credential_id=*/"", kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  WaitForCallbacks();

  // FIDO Failure.
  EXPECT_FALSE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  // Followed by a fallback to CVC.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Ensures that CVC prompt is invoked when the pre-flight call to Google
// Payments times out.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FetchServerCardFIDOTimeoutCVCFallback) {
  if (IsMaskedServerCardRiskBasedAuthEnabled()) {
    GTEST_SKIP() << "For risk based authentication, whether fallback to FIDO "
                    "or CVC flow doesn't rely on the platform call";
  }
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  FetchCreditCard(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

// Ensures whether user choose a masked server card before or after the
// preflight call is returned is correctly logged.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       Metrics_LoggingPreflightCallReturnedBeforeUserSelection) {
  // Setting up a FIDO-enabled user with a local card and a server card.
  std::string server_guid = "00000000-0000-0000-0000-000000000001";
  std::string local_guid = "00000000-0000-0000-0000-000000000003";
  CreateServerCard(server_guid, "4594299181086168");
  CreateLocalCard(local_guid, "4409763681177079");
  const CreditCard* server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(server_guid);
  const CreditCard* local_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(local_guid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);

  for (bool user_is_opted_in : {true, false}) {
    std::string histogram_name;
    if (IsMaskedServerCardRiskBasedAuthEnabled()) {
      histogram_name =
          "Autofill.BetterAuth.PreflightCallResponseReceivedOnCardSelection.";
      histogram_name +=
          user_is_opted_in ? "OptedIn.ServerCard" : "OptedOut.ServerCard";
    } else {
      histogram_name =
          "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.";
      histogram_name += user_is_opted_in ? "OptedIn" : "OptedOut";
    }
    SetCreditCardFIDOAuthEnabled(user_is_opted_in);

    {
      // Preflight call ignored because local card was chosen.
      base::HistogramTester histogram_tester;

      ResetFetchCreditCard();
      credit_card_access_manager().PrepareToFetchCreditCard();
      FastForwardBy(base::Seconds(4));
      WaitForCallbacks();

      FetchCreditCard(local_card);
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          std::string(
              "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.") +
              (user_is_opted_in ? "OptedIn" : "OptedOut"),
          autofill_metrics::PreflightCallEvent::kDidNotChooseMaskedCard, 1);
    }

    {
      // Preflight call returned after card was chosen.
      base::HistogramTester histogram_tester;
      payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);

      ResetFetchCreditCard();
      credit_card_access_manager().PrepareToFetchCreditCard();
      FetchCreditCard(server_card);
      FastForwardBy(base::Seconds(4));
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::
              kCardChosenBeforePreflightCallReturned,
          1);
      if (!IsMaskedServerCardRiskBasedAuthEnabled()) {
        histogram_tester.ExpectTotalCount(
            "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
            "Duration",
            static_cast<int>(user_is_opted_in));
        histogram_tester.ExpectTotalCount(
            "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
            "TimedOutCvcFallback",
            static_cast<int>(user_is_opted_in));
      }
    }

    {
      // Preflight call returned before card was chosen.
      base::HistogramTester histogram_tester;
      // This is important because CreditCardFidoAuthenticator will update the
      // opted-in pref according to GetDetailsForGetRealPan response.
      payments_network_interface().AllowFidoRegistration(!user_is_opted_in);

      ResetFetchCreditCard();
      PrepareToFetchCreditCardAndWaitForCallbacks();
      FetchCreditCard(server_card);
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::
              kPreflightCallReturnedBeforeCardChosen,
          1);
    }
  }
}

// Ensures that falling back to CVC because of preflight timeout is correctly
// logged.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       Metrics_LoggingTimedOutCvcFallback) {
  if (IsMaskedServerCardRiskBasedAuthEnabled()) {
    GTEST_SKIP() << "For risk based authentication, whether fallback to FIDO "
                    "or CVC flow doesn't rely on the platform call";
  }
  // Setting up a FIDO-enabled user with a local card and a server card.
  std::string server_guid = "00000000-0000-0000-0000-000000000001";
  CreateServerCard(server_guid, "4594299181086168");
  const CreditCard* server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(server_guid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);

  std::string existence_perceived_latency_histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn";
  std::string perceived_latency_duration_histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "Duration";
  std::string timeout_cvc_fallback_histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "TimedOutCvcFallback";

  // Preflight call arrived before timeout, after card was chosen.
  {
    base::HistogramTester histogram_tester;

    ResetFetchCreditCard();
    credit_card_access_manager().PrepareToFetchCreditCard();
    FetchCreditCard(server_card);

    // Mock a delayed response.
    InvokeDelayedGetUnmaskDetailsResponse();

    FastForwardBy(base::Seconds(4));
    WaitForCallbacks();

    histogram_tester.ExpectUniqueSample(
        existence_perceived_latency_histogram_name,
        autofill_metrics::PreflightCallEvent::
            kCardChosenBeforePreflightCallReturned,
        1);
    histogram_tester.ExpectTotalCount(perceived_latency_duration_histogram_name,
                                      1);
    histogram_tester.ExpectBucketCount(timeout_cvc_fallback_histogram_name,
                                       false, 1);
  }

  // Preflight call timed out and CVC fallback was invoked.
  {
    base::HistogramTester histogram_tester;

    ResetFetchCreditCard();
    credit_card_access_manager().PrepareToFetchCreditCard();
    FetchCreditCard(server_card);
    FastForwardBy(base::Seconds(4));
    WaitForCallbacks();

    histogram_tester.ExpectUniqueSample(
        existence_perceived_latency_histogram_name,
        autofill_metrics::PreflightCallEvent::
            kCardChosenBeforePreflightCallReturned,
        1);
    histogram_tester.ExpectTotalCount(perceived_latency_duration_histogram_name,
                                      1);
    histogram_tester.ExpectBucketCount(timeout_cvc_fallback_histogram_name,
                                       true, 1);
  }
}

// Ensures that use of a server card that is not enrolled into FIDO invokes
// authorization flow when user is opted-in.
TEST_P(CreditCardAccessManagerAuthFlowTest, FIDONewCardAuthorization) {
  base::HistogramTester histogram_tester;
  std::string unmask_decision_histogram_name =
      "Autofill.BetterAuth.CardUnmaskTypeDecision";
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.AuthenticationAfterCVC";
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.CvcThenFido";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  OptUserInToFido();

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Do not return any RequestOptions or CreationOptions in GetRealPan.
  // RequestOptions should have been returned in unmask details response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
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
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  // Mock OptChange payments call.
  OptChange(PaymentsRpcResult::kSuccess, true);

  histogram_tester.ExpectUniqueSample(
      unmask_decision_histogram_name,
      autofill_metrics::CardUnmaskTypeDecisionMetric::kCvcThenFido, 1);
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that the use of a server card that is not enrolled into FIDO fills
// the form if the user is opted-in to FIDO but no request options are present.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDONewCardAuthorization_NoRequestOptions_FormFilled) {
  const CreditCard* card = CreateServerCard(kTestGUID, kTestNumber);
  OptUserInToFido();

  // Clear the FIDO request options that were set.
  payments_network_interface().unmask_details()->fido_request_options.clear();

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Do not return any RequestOptions or CreationOptions in GetRealPan.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is filled as there are no FIDO request options
  // present.
  EXPECT_EQ(accessor().number(), kTestNumber16);
  EXPECT_EQ(accessor().cvc(), kTestCvc16);
}

// Ensures that use of a server card that is not enrolled into FIDO fills the
// form if the user is opted-in to FIDO but the request options are invalid.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDONewCardAuthorization_InvalidRequestOptions_FormFilled) {
  const CreditCard* card = CreateServerCard(kTestGUID, kTestNumber);
  OptUserInToFido();

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);

  // Set invalid FIDO request options.
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      /*credential_id=*/"", /*relying_party_id=*/"");

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(
      card, /*fido_request_options=*/GetTestRequestOptions(
          /*return_invalid_request_options=*/true));

  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Do not return any RequestOptions in GetRealPan. RequestOptions should have
  // been returned in unmask details response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is filled as the only FIDO request options present are
  // invalid.
  EXPECT_EQ(accessor().number(), kTestNumber16);
  EXPECT_EQ(accessor().cvc(), kTestCvc16);
}

// Ensures expired cards always invoke a CVC prompt instead of WebAuthn.
TEST_F(CreditCardAccessManagerTest, FetchExpiredServerCardInvokesCvcPrompt) {
  // Creating an expired server card and opting the user in with authorized
  // card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard card =
      *personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  card.SetExpirationYearFromString(u"2010");
  personal_data().payments_data_manager().UpdateCreditCard(card);

  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card.server_id(), kCredentialId, kGooglePaymentsRpid);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCard(&card);
  WaitForCallbacks();

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
}

#if BUILDFLAG(IS_ANDROID)
// Ensures that the WebAuthn verification prompt is invoked after user opts in
// on unmask card checkbox.
TEST_P(CreditCardAccessManagerAuthFlowTest, FIDOOptInSuccess_Android) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to valid to mock user
  // checking the opt-in checkbox and ensuring GetRealPan returns
  // RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kValid));
  WaitForCallbacks();

  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kOptInWithChallengeFlow,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor().number(), std::u16string());
  EXPECT_EQ(accessor().cvc(), std::u16string());

  // Mock user response.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  // Mock OptChange payments call.
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kSuccess, 1);
}

// Ensures that the card is filled into the form if the request options returned
// are invalid when the user opts in through the checkbox.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOOptInFailure_InvalidResponseRequestOptions) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Set the test request options returned to invalid to mock the user checking
  // the checkbox, but invalid request options are returned from the server.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kInvalid));
  WaitForCallbacks();

  // Ensure that the form is filled because the request options returned from
  // the response were invalid.
  EXPECT_EQ(accessor().number(), kTestNumber16);
  EXPECT_EQ(accessor().cvc(), kTestCvc16);
}

// Ensures that the failed user verification disallows enrollment.
TEST_P(CreditCardAccessManagerAuthFlowTest, FIDOOptInUserVerificationFailure) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to valid to mock user
  // checking the opt-in checkbox and ensuring GetRealPan returns
  // RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kValid));
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kOptInWithChallengeFlow,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor().number(), std::u16string());
  EXPECT_EQ(accessor().cvc(), std::u16string());

  // Mock GetAssertion failure.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);
  // Ensure that form is still filled even if user verification fails
  // (OnCreditCardFetched is called). Note that this is different behavior than
  // registering a new card.
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kNotAllowedError,
      1);
}

// Ensures that enrollment does not happen if the server returns a failure.
TEST_P(CreditCardAccessManagerAuthFlowTest, FIDOOptInServerFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to valid to mock user
  // checking the opt-in checkbox and ensuring GetRealPan returns
  // RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kValid));
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kOptInWithChallengeFlow,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor().number(), std::u16string());
  EXPECT_EQ(accessor().cvc(), std::u16string());

  // Mock user response and OptChange payments call.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
  OptChange(PaymentsRpcResult::kPermanentFailure, false);

  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());
}

// Ensures that enrollment does not happen if user unchecking the opt-in
// checkbox.
TEST_P(CreditCardAccessManagerAuthFlowTest, FIDOOptIn_CheckboxDeclined) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to not present to mock
  // user unchecking the opt-in checkbox resulting in GetRealPan not returning
  // request options.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that form is filled (OnCreditCardFetched is called).
  EXPECT_EQ(kTestNumber16, accessor().number());
  EXPECT_EQ(kTestCvc16, accessor().cvc());
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // never called.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kNoneFlow,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());
}

// Ensures that opting-in through settings page on Android successfully sends an
// opt-in request the next time the user downstreams a card.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOSettingsPageOptInSuccess_Android) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);

  // Setting the local opt-in state as true and implying that Payments servers
  // has the opt-in state to false - this shows the user opted-in through the
  // settings page.
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AllowFidoRegistration(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  MockUserResponseForCvcAuth(kTestCvc16, /*enable_fido=*/false);

  // Although the checkbox was hidden and |enable_fido_auth| was set to false in
  // the user request, because of the previous opt-in intention, the client must
  // request to opt-in.
  EXPECT_TRUE(payments_network_interface()
                  .unmask_request()
                  ->user_response.enable_fido_auth);
}

#else   // BUILDFLAG(IS_ANDROID)
// Ensures that the WebAuthn enrollment prompt is invoked after user opts in. In
// this case, the user is not yet enrolled server-side, and thus receives
// |creation_options|.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOEnrollmentSuccess_CreationOptions_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/true);

  // Mock user response and OptChange payments call.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kOptInWithChallengeFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/true);
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());
  EXPECT_EQ(0, GetStrikes());
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(opt_in_histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithCreationChallenge, 1);
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::kAccepted, 1);
}

// Ensures that the correct number of strikes are added when the user declines
// the WebAuthn offer.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOEnrollment_OfferDeclined_Desktop) {
  base::HistogramTester histogram_tester;
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  AcceptWebauthnOfferDialog(/*did_accept=*/false);
  EXPECT_EQ(
      FidoAuthenticationStrikeDatabase::kStrikesToAddWhenOptInOfferDeclined,
      GetStrikes());
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::
          kDeclinedImmediately,
      1);
}

// Ensures that the correct number of strikes are added when the user declines
// the WebAuthn offer.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOEnrollment_OfferDeclinedAfterAccepting_Desktop) {
  base::HistogramTester histogram_tester;
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  AcceptWebauthnOfferDialog(/*did_accept=*/true);
  AcceptWebauthnOfferDialog(/*did_accept=*/false);
  EXPECT_EQ(
      FidoAuthenticationStrikeDatabase::kStrikesToAddWhenOptInOfferDeclined,
      GetStrikes());
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::
          kDeclinedAfterAccepting,
      1);
}

// Ensures that the correct number of strikes are added when the user fails to
// complete user-verification for an opt-in attempt.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOEnrollment_UserVerificationFailed_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/true);

  // Mock user response.
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/false);
  EXPECT_EQ(FidoAuthenticationStrikeDatabase::
                kStrikesToAddWhenUserVerificationFailsOnOptInAttempt,
            GetStrikes());
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kNotAllowedError, 1);
  histogram_tester.ExpectUniqueSample(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
}

// Ensures that the WebAuthn enrollment prompt is invoked after user opts in. In
// this case, the user is already enrolled server-side, and thus receives
// |request_options|.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       FIDOEnrollmentSuccess_RequestOptions_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/false,
            /*include_request_options=*/true);

  // Mock user response and OptChange payments call.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::kOptInWithChallengeFlow,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(opt_in_histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithRequestChallenge, 1);
}

TEST_F(CreditCardAccessManagerTest, SettingsPage_OptOut) {
  base::HistogramTester histogram_tester;
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  EXPECT_TRUE(IsCreditCardFIDOAuthEnabled());
  credit_card_access_manager().OnSettingsPageFIDOAuthToggled(false);
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);

  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Ensure that when unmask detail response is delayed, we will automatically
// fall back to CVC even if local pref and Payments mismatch.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       IntentToOptOut_DelayedUnmaskDetailsResponse) {
  if (IsMaskedServerCardRiskBasedAuthEnabled()) {
    GTEST_SKIP() << "For risk based authentication, whether fallback to FIDO "
                    "or CVC flow doesn't rely on the platform call";
  }
  base::HistogramTester histogram_tester;
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);
  // Delay the UnmaskDetailsResponse so that we can't discover the mismatch,
  // which will use local pref and fall back to CVC.
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCard(card);

  // Ensure the auth flow type is CVC because no unmask detail response is
  // returned and local pref denotes that user is opted out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
  // Also ensure that since local pref is disabled, we will directly fall back
  // to CVC instead of falling back after time out. Ensure that
  // CardChosenBeforePreflightCallReturned is logged to opted-out histogram.
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedOut",
      autofill_metrics::PreflightCallEvent::
          kCardChosenBeforePreflightCallReturned,
      1);
  // No bucket count for OptIn TimedOutCvcFallback.
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "TimedOutCvcFallback",
      0);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  // Since no unmask detail returned, we can't discover the pref mismatch, we
  // won't call opt out and local pref is unchanged.
  EXPECT_FALSE(GetFIDOAuthenticator()->IsOptOutCalled());
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_P(CreditCardAccessManagerAuthFlowTest,
       IntentToOptOut_OptOutAfterUnmaskSucceeds) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Also ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth success.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();

  // Ensure calling opt out after a successful cvc auth.
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  // Mock opt out success response. Local pref is consistent with payments.
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_P(CreditCardAccessManagerAuthFlowTest,
       IntentToOptOut_OptOutAfterUnmaskFails) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth failure.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kPermanentFailure,
                                   std::string()));
  WaitForCallbacks();

  // Ensure calling opt out after cvc auth failure.
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  // Mock opt out success. Local pref is consistent with payments.
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_P(CreditCardAccessManagerAuthFlowTest, IntentToOptOut_OptOutFailure) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Also ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth success.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();

  // Mock payments opt out failure. Local pref should be unchanged.
  OptChange(PaymentsRpcResult::kPermanentFailure, false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

// TODO(crbug.com/40253866): Extend the FIDOAuthOptChange tests to more
// use-cases.
TEST_F(CreditCardAccessManagerTest, FIDOAuthOptChange_OptOut) {
  credit_card_access_manager().FIDOAuthOptChange(/*opt_in=*/false);
  ASSERT_TRUE(fido_authenticator().IsOptOutCalled());
}

TEST_F(CreditCardAccessManagerTest, FIDOAuthOptChange_OptOut_OffTheRecord) {
  autofill_client().set_is_off_the_record(true);
  credit_card_access_manager().FIDOAuthOptChange(/*opt_in=*/false);
  ASSERT_FALSE(fido_authenticator().IsOptOutCalled());
}

// TODO(crbug.com/40707930) Debug issues and re-enable this test on MacOS.
#if !BUILDFLAG(IS_APPLE)
// Ensures that PrepareToFetchCreditCard() is properly rate limited.
TEST_F(CreditCardAccessManagerTest, PreflightCallRateLimited) {
  // Create server card and set user as eligible for FIDO auth.
  base::HistogramTester histogram_tester;
  std::string preflight_call_metric =
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus";

  ClearCards();
  CreateServerCard(kTestGUID, kTestNumber);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  ResetFetchCreditCard();

  // First call to PrepareToFetchCreditCard() should make RPC.
  credit_card_access_manager().PrepareToFetchCreditCard();
  histogram_tester.ExpectTotalCount(preflight_call_metric, 1);

  // Calling PrepareToFetchCreditCard() without a prior preflight call should
  // have set |can_fetch_unmask_details_| to false to prevent further ones.
  EXPECT_FALSE(
      test_api(credit_card_access_manager()).can_fetch_unmask_details());

  // Any subsequent calls should not make a RPC.
  credit_card_access_manager().PrepareToFetchCreditCard();
  histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
}
#endif  // !BUILDFLAG(IS_APPLE)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

// Ensures that UnmaskAuthFlowEvents also log to a ".ServerCard" subhistogram
// when a masked server card is selected.
TEST_P(CreditCardAccessManagerAuthFlowTest,
       UnmaskAuthFlowEvent_AlsoLogsServerCardSubhistogram) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  base::HistogramTester histogram_tester;
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Cvc.ServerCard";

  PrepareToFetchCreditCardAndWaitForCallbacks();
  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that |is_authentication_in_progress_| is set correctly.
TEST_P(CreditCardAccessManagerAuthFlowTest, AuthenticationInProgress) {
  CreateServerCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  EXPECT_FALSE(IsAuthenticationInProgress());

  FetchCreditCardAndCompleteRiskBasedAuthIfAvailable(card);

  EXPECT_TRUE(IsAuthenticationInProgress());
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_FALSE(IsAuthenticationInProgress());
}

// Ensures that the use of |unmasked_card_cache_| is set and logged correctly.
TEST_F(CreditCardAccessManagerTest, FetchCreditCardUsesUnmaskedCardCache) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard masked_card =
      *personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  // Mock out that the card has become unmasked and cached.
  CreditCard unmasked_card = AsFullServerCard(masked_card);
  credit_card_access_manager().CacheUnmaskedCardInfo(unmasked_card, kTestCvc16);

  // Now fetch the masked credit card - this should use the cached unmasked
  // version.
  FetchCreditCard(&masked_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit, 1);

  FetchCreditCard(&masked_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit, 2);

  // Create a virtual card.
  CreditCard virtual_card;
  test::SetCreditCardInfo(&virtual_card, "Elvis Presley", kTestNumber,
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  virtual_card = AsVirtualCard(virtual_card);
  credit_card_access_manager().CacheUnmaskedCardInfo(virtual_card, kTestCvc16);

  // Mocks that user selects the virtual card option of the masked card.
  masked_card = AsVirtualCard(masked_card);
  FetchCreditCard(&masked_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit, 1);
}

TEST_F(CreditCardAccessManagerTest, GetCachedUnmaskedCards) {
  // Assert that there are no cards cached initially.
  EXPECT_EQ(0U, credit_card_access_manager().GetCachedUnmaskedCards().size());

  CreditCard unmasked_card = AsFullServerCard(
      *CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  CreateServerCard(kTestGUID2, kTestNumber2, kTestServerId2);
  // Add a card to the cache.
  credit_card_access_manager().CacheUnmaskedCardInfo(unmasked_card, kTestCvc16);

  // Verify that only the card added to the cache is returned.
  ASSERT_EQ(1U, credit_card_access_manager().GetCachedUnmaskedCards().size());
  EXPECT_EQ(unmasked_card,
            credit_card_access_manager().GetCachedUnmaskedCards()[0]->card);
}

TEST_F(CreditCardAccessManagerTest, IsCardPresentInUnmaskedCache) {
  CreditCard unmasked_card = AsFullServerCard(
      *CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  CreateServerCard(kTestGUID2, kTestNumber2, kTestServerId2);
  // Add a card to the cache.
  credit_card_access_manager().CacheUnmaskedCardInfo(unmasked_card, kTestCvc16);

  // Verify that only one card is present in the cache.
  EXPECT_TRUE(
      credit_card_access_manager().IsCardPresentInUnmaskedCache(unmasked_card));
  EXPECT_FALSE(credit_card_access_manager().IsCardPresentInUnmaskedCache(
      *personal_data().payments_data_manager().GetCreditCardByGUID(
          kTestGUID2)));
}

TEST_F(CreditCardAccessManagerTest, IsVirtualCardPresentInUnmaskedCache) {
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));

  // Add the virtual card to the cache.
  credit_card_access_manager().CacheUnmaskedCardInfo(virtual_card, kTestCvc16);

  // Verify that the virtual card is present in the cache.
  EXPECT_TRUE(
      credit_card_access_manager().IsCardPresentInUnmaskedCache(virtual_card));
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Success_VirtualCards) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  CreditCard server_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  CreditCard virtual_card = AsVirtualCard(
      *personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID));
  virtual_card.set_cvc(u"321");

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalSuccess(server_card, virtual_card, observer);
  credit_card_access_manager().AddObserver(&observer);

  FetchCreditCard(&server_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(virtual_card));
  credit_card_access_manager().RemoveObserver(&observer);

  // Ensure the accessor received the correct response.
  EXPECT_EQ(accessor().number(), u"4234567890123456");
  EXPECT_EQ(accessor().cvc(), u"321");
  EXPECT_EQ(accessor().expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
  EXPECT_EQ(accessor().expiry_year(), base::UTF8ToUTF16(test::NextYear()));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  EXPECT_THAT(type,
              testing::Optional(NonInteractivePaymentMethodType::kVirtualCard));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the OTP authenticator, when only the OTP
// challenge option is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_OtpOnly) {
  base::HistogramTester histogram_tester;

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kEmailOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options,
      /*selected_index=*/0, CreditCard::RecordType::kVirtualCard);

  CreditCard card = test::GetVirtualCard();
  credit_card_access_manager().OnOtpAuthenticationComplete(
      CreditCardOtpAuthenticator::OtpAuthenticationResponse()
          .with_result(CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                           Result::kSuccess)
          .with_card(&card)
          .with_cvc(kTestCvc16));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect accessor to successfully retrieve the CVC.
  EXPECT_EQ(kTestCvc16, accessor().cvc());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the CVC authenticator, when only the CVC
// challenge option is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_CvcOnly) {
  base::HistogramTester histogram_tester;

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options,
      /*selected_index=*/0, CreditCard::RecordType::kVirtualCard);

  CreditCard card = test::GetCreditCard();
  credit_card_access_manager().OnCvcAuthenticationComplete(
      CreditCardCvcAuthenticator::CvcAuthenticationResponse()
          .with_did_succeed(true)
          .with_card(&card)
          .with_cvc(u"123"));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  // TODO(crbug.com/40240970): Add metrics checks for Virtual Card CVC auth
  // result.
}

// Ensures the virtual card risk-based unmasking flow type is set to
// kThreeDomainSecure when only the 3DS challenge option is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Only3dsChallengeReturned) {
  base::HistogramTester histogram_tester;
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  FetchCreditCard(&virtual_card);

  EXPECT_CALL(*static_cast<payments::MockPaymentsWindowManager*>(
                  autofill_client()
                      .GetPaymentsAutofillClient()
                      ->GetPaymentsWindowManager()),
              InitVcn3dsAuthentication)
      .WillOnce([&](payments::PaymentsWindowManager::Vcn3dsContext context) {
        EXPECT_EQ(context.context_token, "fake_context_token");
        EXPECT_EQ(context.card, virtual_card);
        EXPECT_EQ(context.challenge_option.type,
                  CardUnmaskChallengeOptionType::kThreeDomainSecure);
        EXPECT_FALSE(context.user_consent_already_given);
      });

  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse response;
  response.context_token = "fake_context_token";
  response.result = CreditCardRiskBasedAuthenticator::
      RiskBasedAuthenticationResponse::Result::kAuthenticationRequired;
  response.card_unmask_challenge_options = test::GetCardUnmaskChallengeOptions(
      {CardUnmaskChallengeOptionType::kThreeDomainSecure});

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      response);

  // If VCN 3DS is the only challenge option returned, verify that flow type is
  // kThreeDomainSecure.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kThreeDomainSecure);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.ThreeDomainSecure",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);
}

// Ensures the `kCardInfoRetrievalEnrolledUnmaskProgressDialog` is set if
// `card_info_retrieval_enrollment_state` is enrolled.
TEST_F(CreditCardAccessManagerTest, CardInfoRetrievalEnrolledCardUnmasking) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableFpanRiskBasedAuthentication};
  base::HistogramTester histogram_tester;
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_guid(kTestGUID);
  server_card.set_card_info_retrieval_enrollment_state(
      CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled);
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  FetchCreditCard(card);

  EXPECT_EQ(autofill_client()
                .GetPaymentsAutofillClient()
                ->autofill_progress_dialog_type(),
            AutofillProgressDialogType::
                kCardInfoRetrievalEnrolledUnmaskProgressDialog);
}

// Ensures the `kCardInfoRetrievalEnrolledUnmaskProgressDialog` is not set, even
// if `kAutofillEnableCardInfoRuntimeRetrieval` is enabled, but
// `card_info_retrieval_enrollment_state` is not enrolled.
TEST_F(CreditCardAccessManagerTest,
       CardInfoRetrievalEnrolledCardUnmaskingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardInfoRuntimeRetrieval},
      /*disabled_features=*/{
          features::kAutofillEnableFpanRiskBasedAuthentication});

  base::HistogramTester histogram_tester;
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_guid(kTestGUID);
  server_card.set_card_info_retrieval_enrollment_state(
      CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalUnspecified);
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  FetchCreditCard(card);

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client()
                   .GetPaymentsAutofillClient()
                   ->risk_based_authentication_invoked());
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the correct authenticator when multiple
// challenge options are returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_OtpAndCvcAnd3ds) {
  base::HistogramTester histogram_tester;

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kCvc,
           CardUnmaskChallengeOptionType::kThreeDomainSecure});

  for (size_t selected_index = 0; selected_index < challenge_options.size();
       selected_index++) {
    MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
        /*fido_authenticator_is_user_opted_in=*/false,
        /*is_user_verifiable=*/false, challenge_options, selected_index,
        CreditCard::RecordType::kVirtualCard);

    switch (challenge_options[selected_index].type) {
      case CardUnmaskChallengeOptionType::kSmsOtp: {
        CreditCard card = test::GetVirtualCard();
        credit_card_access_manager().OnOtpAuthenticationComplete(
            CreditCardOtpAuthenticator::OtpAuthenticationResponse()
                .with_result(CreditCardOtpAuthenticator::
                                 OtpAuthenticationResponse::Result::kSuccess)
                .with_card(&card)
                .with_cvc(u"123"));
        break;
      }
      case CardUnmaskChallengeOptionType::kCvc: {
        CreditCard card = test::GetVirtualCard();
        credit_card_access_manager().OnCvcAuthenticationComplete(
            CreditCardCvcAuthenticator::CvcAuthenticationResponse()
                .with_did_succeed(true)
                .with_card(&card)
                .with_cvc(u"123"));
        break;
      }
      case CardUnmaskChallengeOptionType::kThreeDomainSecure:
        // VCN 3DS is one of the challenge options returned in the challenge
        // selection dialog, and user selected the 3DS challenge option. Verify
        // that flow type is kThreeDomainSecureConsentAlreadyGiven.
        EXPECT_EQ(GetUnmaskAuthFlowType(),
                  UnmaskAuthFlowType::kThreeDomainSecureConsentAlreadyGiven);
        break;
      case CardUnmaskChallengeOptionType::kEmailOtp:
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED();
    }
  }

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client().GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 3);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
  // TODO(crbug.com/40240970): Add metrics checks for Virtual Card CVC auth
  // result.
}

#if !BUILDFLAG(IS_IOS)
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_CreditCardAccessManagerReset_TriggersOtpAuthenticatorResetOnFlowCancelled) {
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options,
      /*selected_index=*/0, CreditCard::RecordType::kVirtualCard);

  // This check already happens in
  // MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(), but double
  // checking here helps show this test works correctly.
  EXPECT_TRUE(otp_authenticator().on_challenge_option_selected_invoked());

  test_api(credit_card_access_manager()).OnVirtualCardUnmaskCancelled();
  EXPECT_FALSE(otp_authenticator().on_challenge_option_selected_invoked());
}

// Test that a success response for a VCN 3DS authentication is handled
// correctly and notifies the caller with the proper fields set, and both the
// authentication unmasked server card result and prompt completed better auth
// flow event metric buckets are logged to.
TEST_F(CreditCardAccessManagerTest,
       VirtualCardUnmasking_3dsResponseReceived_Success) {
  // Set up the test.
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetVirtualCard();
  FetchCreditCard(&card);
  test_api(credit_card_access_manager())
      .set_unmask_auth_flow_type(UnmaskAuthFlowType::kThreeDomainSecure);
  payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response;
  response.card = card;
  response.result =
      payments::PaymentsWindowManager::Vcn3dsAuthenticationResult::kSuccess;

  // Mock the VCN 3DS authentication response.
  test_api(credit_card_access_manager())
      .OnVcn3dsAuthenticationComplete(response);

  // Check that `accessor_` was triggered with the expected values.
  EXPECT_EQ(accessor().number(), response.card->number());
  EXPECT_EQ(accessor().cvc(), response.card->cvc());
  EXPECT_FALSE(
      test_api(credit_card_access_manager()).is_authentication_in_progress());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.ThreeDomainSecure",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.ThreeDomainSecure",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Test that a failure response for a VCN 3DS authentication is handled
// correctly and notifies the caller with the proper fields set, and the virtual
// card retrieval error server card unmask result metric bucket is logged to.
TEST_F(CreditCardAccessManagerTest,
       VirtualCardUnmasking_3dsResponseReceived_AuthenticationError) {
  // Set up the test.
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetVirtualCard();
  FetchCreditCard(&card);
  payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response;
  response.result = payments::PaymentsWindowManager::
      Vcn3dsAuthenticationResult::kAuthenticationFailed;

  // Mock the VCN 3DS authentication response.
  test_api(credit_card_access_manager())
      .OnVcn3dsAuthenticationComplete(response);

  EXPECT_FALSE(
      test_api(credit_card_access_manager()).is_authentication_in_progress());
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.ThreeDomainSecure",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

// Test that the user cancelling a VCN 3DS authentication is handled correctly
// and notifies the caller with the proper fields set, and the flow cancelled
// result metric bucket is logged to.
TEST_F(
    CreditCardAccessManagerTest,
    VirtualCardUnmasking_3dsResponseReceived_AuthenticationNotCompletedError) {
  // Set up the test.
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetVirtualCard();

  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(card, observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCard(&card);
  payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response;
  response.result = payments::PaymentsWindowManager::
      Vcn3dsAuthenticationResult::kAuthenticationNotCompleted;

  // Mock the VCN 3DS authentication response.
  test_api(credit_card_access_manager())
      .OnVcn3dsAuthenticationComplete(response);
  credit_card_access_manager().RemoveObserver(&observer);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.ThreeDomainSecure",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
}

TEST_F(CreditCardAccessManagerTest, Prefetching_RiskData) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnablePrefetchingRiskDataForRetrieval};
  // Setting up a server card.
  CreateServerCard(kTestGUID, kTestNumber);

  credit_card_access_manager().PrepareToFetchCreditCard();

  EXPECT_TRUE(
      autofill_client().GetPaymentsAutofillClient()->risk_data_loaded());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// only the FIDO challenge options is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly) {
  base::HistogramTester histogram_tester;
  const CreditCard* virtual_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kVirtualCard);

  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(true);

  FetchCreditCard(virtual_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(fido_authenticator().card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(fido_authenticator().card().record_type(),
            CreditCard::RecordType::kVirtualCard);
  ASSERT_TRUE(fido_authenticator().context_token().has_value());
  EXPECT_EQ(fido_authenticator().context_token().value(), "fake_context_token");

  // Mock FIDO authentication completed.
  CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response;
  fido_response.did_succeed = true;
  CreditCard card = WithCvc(test::GetVirtualCard(), u"234");
  fido_response.card = &card;
  test_api(credit_card_access_manager())
      .OnFIDOAuthenticationComplete(fido_response);

  // Expect accessor to successfully retrieve the virtual card CVC.
  EXPECT_EQ(u"234", accessor().cvc());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Fido",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// both FIDO and OTP challenge options are returned.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_PrefersFido) {
  base::HistogramTester histogram_tester;

  const CreditCard* virtual_card =
      CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                       CreditCard::RecordType::kVirtualCard);

  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(true);

  FetchCreditCard(virtual_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with information regarding both FIDO and OTP auth.
  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse response;
  response.context_token = "fake_context_token";
  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp})[0];
  response.card_unmask_challenge_options.emplace_back(challenge_option);
  response.fido_request_options = GetTestRequestOptions();
  response.result = CreditCardRiskBasedAuthenticator::
      RiskBasedAuthenticationResponse::Result::kAuthenticationRequired;
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      response);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(fido_authenticator().card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(fido_authenticator().card().record_type(),
            CreditCard::RecordType::kVirtualCard);
  ASSERT_TRUE(fido_authenticator().context_token().has_value());
  EXPECT_EQ(fido_authenticator().context_token().value(), "fake_context_token");

  // Mock FIDO authentication completed.
  CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response;
  fido_response.did_succeed = true;
  CreditCard card = test::GetVirtualCard();
  fido_response.card = &card;
  fido_response.cvc = u"123";
  test_api(credit_card_access_manager())
      .OnFIDOAuthenticationComplete(fido_response);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Fido",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the OTP authenticator, when both
// FIDO and OTP challenge options are returned but FIDO local preference is not
// opted in.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_FidoNotOptedIn) {
  base::HistogramTester histogram_tester;
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/true, challenge_options,
      /*selected_index=*/0, CreditCard::RecordType::kVirtualCard);

  CreditCardOtpAuthenticator::OtpAuthenticationResponse otp_response;
  otp_response.result =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  CreditCard card = test::GetVirtualCard();
  otp_response.card = &card;
  otp_response.cvc = u"123";
  credit_card_access_manager().OnOtpAuthenticationComplete(otp_response);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated first to the FIDO authenticator,
// when both FIDO and OTP challenge options are returned, but fall back to OTP
// authentication if FIDO failed.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_FidoFailedFallBackToOtp) {
  base::HistogramTester histogram_tester;
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/true,
      /*is_user_verifiable=*/true, challenge_options,
      /*selected_index=*/0, CreditCard::RecordType::kVirtualCard);

  CreditCardOtpAuthenticator::OtpAuthenticationResponse otp_response;
  otp_response.result =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  CreditCard card = test::GetVirtualCard();
  otp_response.card = &card;
  otp_response.cvc = u"123";
  credit_card_access_manager().OnOtpAuthenticationComplete(otp_response);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.OtpFallbackFromFido",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is only FIDO option returned by the server but the user
// is not opted in.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly_FidoNotOptedIn) {
  base::HistogramTester histogram_tester;
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(false);

  FetchCreditCard(&virtual_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_FALSE(otp_authenticator().on_challenge_option_selected_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Fido",
      autofill_metrics::ServerCardUnmaskResult::kOnlyFidoAvailableButNotOptedIn,
      1);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_IOS)

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is no challenge option returned by the server.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_NoOptionReturned) {
  base::HistogramTester histogram_tester;
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // |is_user_verifiable_| related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(true);
#endif
  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(virtual_card, observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCard(&virtual_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::kError)
          .with_context_token("fake_context_token"));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_FALSE(otp_authenticator().on_challenge_option_selected_invoked());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
#endif
  credit_card_access_manager().RemoveObserver(&observer);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationError, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is a virtual card retrieval error.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_VirtualCardRetrievalError) {
  base::HistogramTester histogram_tester;
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(true);
#endif
  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(virtual_card, observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCard(&virtual_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kVirtualCardRetrievalError));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_FALSE(otp_authenticator().on_challenge_option_selected_invoked());
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
#endif
  credit_card_access_manager().RemoveObserver(&observer);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_MerchantOptedOut) {
  base::HistogramTester histogram_tester;
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(virtual_card, observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCard(&virtual_card);

  AutofillErrorDialogContext autofill_error_dialog_context;
  autofill_error_dialog_context.server_returned_title =
      "test_server_returned_title";
  autofill_error_dialog_context.server_returned_description =
      "test_server_returned_description";

  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse response;
  response.error_dialog_context = autofill_error_dialog_context;
  response.result = CreditCardRiskBasedAuthenticator::
      RiskBasedAuthenticationResponse::Result::kVirtualCardRetrievalError;
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      response);
  credit_card_access_manager().RemoveObserver(&observer);

  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  const AutofillErrorDialogContext& displayed_error_dialog_context =
      autofill_client()
          .GetPaymentsAutofillClient()
          ->autofill_error_dialog_context();
  EXPECT_EQ(*displayed_error_dialog_context.server_returned_title,
            *autofill_error_dialog_context.server_returned_title);
  EXPECT_EQ(*displayed_error_dialog_context.server_returned_description,
            *autofill_error_dialog_context.server_returned_description);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_FlowCancelled) {
  base::HistogramTester histogram_tester;
  CreditCard virtual_card =
      AsVirtualCard(*CreateServerCard(kTestGUID, kTestNumber, kTestServerId));
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(true);
#endif
  NiceMock<MockCreditCardAccessManagerObserver> observer;
  ExpectCardRetrievalFailure(virtual_card, observer);

  credit_card_access_manager().AddObserver(&observer);
  FetchCreditCard(&virtual_card);

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock that the flow was cancelled by the user.
  test_api(credit_card_access_manager()).OnVirtualCardUnmaskCancelled();
  credit_card_access_manager().RemoveObserver(&observer);

  EXPECT_FALSE(otp_authenticator().on_challenge_option_selected_invoked());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
}

// Test that the CreditCardAccessManager's destructor resets the record type of
// the card that had no interactive authentication flows completed in the
// associated FormDataImporter.
TEST_F(CreditCardAccessManagerTest, DestructorResetsCardIdentifier) {
  auto* form_data_importer = autofill_client().GetFormDataImporter();
  form_data_importer
      ->SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);
  EXPECT_TRUE(
      test_api(*form_data_importer)
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
  DeleteAutofillDriver(autofill_driver());
  EXPECT_FALSE(
      test_api(*form_data_importer)
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

TEST_F(CreditCardAccessManagerTest, InvokeVirtualCardEnrollmentPreflightCall) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::
          kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment);
  auto virtual_card_enrollment_manager =
      std::make_unique<MockVirtualCardEnrollmentManager>(
          &personal_data().payments_data_manager(),
          /*payments_network_interface=*/
          static_cast<payments::MultipleRequestPaymentsNetworkInterface*>(
              nullptr),
          &autofill_client());
  autofill_client()
      .GetPaymentsAutofillClient()
      ->set_virtual_card_enrollment_manager(
          std::move(virtual_card_enrollment_manager));
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  personal_data().test_payments_data_manager().AddServerCreditCard(card);
  EXPECT_CALL(*static_cast<MockVirtualCardEnrollmentManager*>(
                  autofill_client()
                      .GetPaymentsAutofillClient()
                      ->GetVirtualCardEnrollmentManager()),
              InitVirtualCardEnroll);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  CreditCardAccessManagerTestBase::FetchCreditCard(&card);
}

TEST_F(CreditCardAccessManagerTest,
       DoNotInvokeVirtualCardEnrollmentPreflightCall_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::
          kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment);
  auto virtual_card_enrollment_manager =
      std::make_unique<MockVirtualCardEnrollmentManager>(
          &personal_data().payments_data_manager(),
          /*payments_network_interface=*/
          static_cast<payments::MultipleRequestPaymentsNetworkInterface*>(
              nullptr),
          &autofill_client());
  autofill_client()
      .GetPaymentsAutofillClient()
      ->set_virtual_card_enrollment_manager(
          std::move(virtual_card_enrollment_manager));
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  personal_data().test_payments_data_manager().AddServerCreditCard(card);
  EXPECT_CALL(*static_cast<MockVirtualCardEnrollmentManager*>(
                  autofill_client()
                      .GetPaymentsAutofillClient()
                      ->GetVirtualCardEnrollmentManager()),
              InitVirtualCardEnroll)
      .Times(0);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  CreditCardAccessManagerTestBase::FetchCreditCard(&card);
}

TEST_F(CreditCardAccessManagerTest,
       DoNotInvokeVirtualCardEnrollmentPreflightCall_CardNotEligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::
          kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment);
  auto virtual_card_enrollment_manager =
      std::make_unique<MockVirtualCardEnrollmentManager>(
          &personal_data().payments_data_manager(),
          /*payments_network_interface=*/
          static_cast<payments::MultipleRequestPaymentsNetworkInterface*>(
              nullptr),
          &autofill_client());
  autofill_client()
      .GetPaymentsAutofillClient()
      ->set_virtual_card_enrollment_manager(
          std::move(virtual_card_enrollment_manager));
  CreditCard card = test::GetMaskedServerCard();
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible);
  personal_data().test_payments_data_manager().AddServerCreditCard(card);
  EXPECT_CALL(*static_cast<MockVirtualCardEnrollmentManager*>(
                  autofill_client()
                      .GetPaymentsAutofillClient()
                      ->GetVirtualCardEnrollmentManager()),
              InitVirtualCardEnroll)
      .Times(0);

  PrepareToFetchCreditCardAndWaitForCallbacks();
  CreditCardAccessManagerTestBase::FetchCreditCard(&card);
}

}  // namespace autofill
