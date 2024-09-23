// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_params_manager.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace autofill {
namespace {

constexpr char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
constexpr char kTestNumber[] = "4234567890123456";  // Visa
constexpr char16_t kTestNumber16[] = u"4234567890123456";
constexpr char kTestRelyingPartyId[] = "google.com";
// Base64 encoding of "This is a test challenge".
constexpr char kTestChallenge[] = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
// Base64 encoding of "This is a test Credential ID".
constexpr char kTestCredentialId[] = "VGhpcyBpcyBhIHRlc3QgQ3JlZGVudGlhbCBJRC4=";
// Base64 encoding of "This is a test signature".
constexpr char kTestSignature[] = "VGhpcyBpcyBhIHRlc3Qgc2lnbmF0dXJl";
constexpr char kTestAuthToken[] = "dummy_card_authorization_token";
constexpr std::string_view kEnrollmentOfferedHistogramName =
    "Autofill.BetterAuth.EnrollmentPromptOffered";

std::vector<uint8_t> Base64ToBytes(std::string base64) {
  return base::Base64Decode(base64).value_or(std::vector<uint8_t>());
}

std::string BytesToBase64(const std::vector<uint8_t> bytes) {
  return base::Base64Encode(bytes);
}

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class CreditCardFidoAuthenticatorTest : public testing::Test {
 public:
  void SetUp() override {
    personal_data_manager().SetPrefService(autofill_client_.GetPrefs());

    autofill_driver_.SetAuthenticator(new TestInternalAuthenticator());

    autofill_client_.GetPaymentsAutofillClient()
        ->set_test_payments_network_interface(
            std::make_unique<payments::TestPaymentsNetworkInterface>(
                autofill_client_.GetURLLoaderFactory(),
                autofill_client_.GetIdentityManager(),
                &personal_data_manager()));
    autofill_client_.set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
    fido_authenticator_ = std::make_unique<CreditCardFidoAuthenticator>(
        &autofill_driver_, &autofill_client_);
  }

  CreditCard CreateServerCard(std::string guid, std::string number) {
    CreditCard masked_server_card = CreditCard();
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            number.c_str(), test::NextMonth().c_str(),
                            test::NextYear().c_str(), "1");
    masked_server_card.set_guid(guid);
    masked_server_card.set_record_type(
        CreditCard::RecordType::kMaskedServerCard);

    personal_data_manager().test_payments_data_manager().ClearCreditCards();
    personal_data_manager().test_payments_data_manager().AddServerCreditCard(
        masked_server_card);

    return masked_server_card;
  }

  base::Value::Dict GetTestRequestOptions(std::string challenge,
                                          std::string relying_party_id,
                                          std::string credential_id) {
    base::Value::Dict request_options;

    // Building the following JSON structure--
    // request_options = {
    //   "challenge": challenge,
    //   "timeout_millis": kTestTimeoutSeconds,
    //   "relying_party_id": relying_party_id,
    //   "key_info": [{
    //       "credential_id": credential_id,
    //       "authenticator_transport_support": ["INTERNAL"]
    // }]}
    request_options.Set("challenge", base::Value(challenge));
    request_options.Set("relying_party_id", base::Value(relying_party_id));

    base::Value::Dict key_info;
    key_info.Set("credential_id", base::Value(credential_id));
    key_info.Set("authenticator_transport_support",
                 base::Value(base::Value::Type::LIST));
    key_info.FindList("authenticator_transport_support")->Append("INTERNAL");

    request_options.Set("key_info", base::Value(base::Value::Type::LIST));
    request_options.FindList("key_info")->Append(std::move(key_info));
    return request_options;
  }

  base::Value::Dict GetTestCreationOptions(std::string challenge,
                                           std::string relying_party_id) {
    base::Value::Dict creation_options;
    if (!challenge.empty())
      creation_options.Set("challenge", base::Value(challenge));
    creation_options.Set("relying_party_id", base::Value(relying_party_id));
    return creation_options;
  }

  // Invokes GetRealPan callback.
  void GetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult result,
                  const std::string& real_pan,
                  bool is_virtual_card = false) {
    DCHECK(fido_authenticator().full_card_request_);
    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.card_type = is_virtual_card ? payments::PaymentsAutofillClient::
                                               PaymentsRpcCardType::kVirtualCard
                                         : payments::PaymentsAutofillClient::
                                               PaymentsRpcCardType::kServerCard;
    fido_authenticator().full_card_request_->OnDidGetRealPan(
        result, response.with_real_pan(real_pan));
  }

  // Mocks an OptChange response from the PaymentsNetworkInterface.
  void OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult result,
                 bool user_is_opted_in,
                 bool include_creation_options = false,
                 bool include_request_options = false) {
    payments::PaymentsNetworkInterface::OptChangeResponseDetails response;
    response.user_is_opted_in = user_is_opted_in;
    if (include_creation_options) {
      response.fido_creation_options =
          GetTestCreationOptions(kTestChallenge, kTestRelyingPartyId);
    }
    if (include_request_options) {
      response.fido_request_options = GetTestRequestOptions(
          kTestChallenge, kTestRelyingPartyId, kTestCredentialId);
    }
    fido_authenticator().OnDidGetOptChangeResult(result, response);
  }

  void SetUserOptInPreference(bool user_is_opted_in) {
    ::autofill::prefs::SetCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs(),
                                                    user_is_opted_in);
    fido_authenticator().user_is_opted_in_ =
        fido_authenticator().IsUserOptedIn();
  }

 protected:
  CreditCardFidoAuthenticator& fido_authenticator() {
    return *fido_authenticator_;
  }
  TestPersonalDataManager& personal_data_manager() {
    return static_cast<TestPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }
  TestAuthenticationRequester& requester() { return requester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestAutofillClient autofill_client_;
  TestAutofillDriver autofill_driver_{&autofill_client_};
  TestAuthenticationRequester requester_;
  std::unique_ptr<CreditCardFidoAuthenticator> fido_authenticator_;
};

TEST_F(CreditCardFidoAuthenticatorTest, IsUserOptedIn_False) {
  SetUserOptInPreference(false);
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());
}

TEST_F(CreditCardFidoAuthenticatorTest, IsUserOptedIn_True) {
  SetUserOptInPreference(true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(CreditCardFidoAuthenticatorTest,
       GetUserOptInIntention_IntentToOptIn_Android) {
  // If payments is offering to opt-in, then that means user is not opted in
  // from payments.
  payments::PaymentsNetworkInterface::UnmaskDetails unmask_details;
  unmask_details.offer_fido_opt_in = true;
  // Set the local preference to be enabled, which denotes user manually opted
  // in from settings page, and Payments did not update the status in time.
  SetUserOptInPreference(true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());

  EXPECT_EQ(fido_authenticator().GetUserOptInIntention(unmask_details),
            UserOptInIntention::kIntentToOptIn);
  // On Android, the local pref is not consistent with payments until opt-in
  // succeeds, so it is unnecessary to check that IsUserOptedIn() is true here,
  // since it will not have updated yet.
}
#else
TEST_F(CreditCardFidoAuthenticatorTest,
       GetUserOptInIntention_IntentToOptIn_Desktop) {
  // If payments is offering to opt-in, then that means user is not opted in
  // from payments.
  payments::PaymentsNetworkInterface::UnmaskDetails unmask_details;
  unmask_details.offer_fido_opt_in = true;
  // Set the local preference to be enabled, which denotes user manually opted
  // in from settings page and Payments did not update the status in time, or
  // something updated on the server side which caused Chrome to be out of sync.
  SetUserOptInPreference(true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());

  // We won't return user intent to opt in for Desktop.
  EXPECT_EQ(fido_authenticator().GetUserOptInIntention(unmask_details),
            UserOptInIntention::kUnspecified);
  // We update mismatched local pref for Desktop in order to be consistent with
  // payments.
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());
}
#endif

TEST_F(CreditCardFidoAuthenticatorTest, GetUserOptInIntention_IntentToOptOut) {
  // If payments is requesting a FIDO auth, then that means user is opted in
  // from payments.
  payments::PaymentsNetworkInterface::UnmaskDetails unmask_details;
  unmask_details.unmask_auth_method =
      payments::PaymentsAutofillClient::UnmaskAuthMethod::kFido;
  // Set the local preference to be disabled, which denotes user manually opted
  // out from settings page, and Payments did not update the status in time.
  SetUserOptInPreference(false);
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  EXPECT_EQ(fido_authenticator().GetUserOptInIntention(unmask_details),
            UserOptInIntention::kIntentToOptOut);
  // The local pref is not consistent with payments until opt-out succeeds, so
  // it is unnecessary to check that IsUserOptedIn() is false here, since it
  // will not have updated yet.
}

TEST_F(CreditCardFidoAuthenticatorTest, IsUserVerifiable_False) {
  fido_authenticator().IsUserVerifiable(
      base::BindOnce(&TestAuthenticationRequester::IsUserVerifiableCallback,
                     requester().GetWeakPtr()));
  EXPECT_FALSE(requester().is_user_verifiable().value());
}

TEST_F(CreditCardFidoAuthenticatorTest, ParseRequestOptions) {
  base::Value::Dict request_options_json = GetTestRequestOptions(
      kTestChallenge, kTestRelyingPartyId, kTestCredentialId);

  blink::mojom::PublicKeyCredentialRequestOptionsPtr request_options_ptr =
      fido_authenticator().ParseRequestOptions(std::move(request_options_json));
  EXPECT_EQ(kTestChallenge, BytesToBase64(request_options_ptr->challenge));
  EXPECT_EQ(kTestRelyingPartyId, request_options_ptr->relying_party_id);
  EXPECT_EQ(kTestCredentialId,
            BytesToBase64(request_options_ptr->allow_credentials.front().id));
  EXPECT_FALSE(request_options_ptr->extensions.is_null());
}

TEST_F(CreditCardFidoAuthenticatorTest, ParseAssertionResponse) {
  blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response_ptr =
      blink::mojom::GetAssertionAuthenticatorResponse::New();
  assertion_response_ptr->info = blink::mojom::CommonCredentialInfo::New();
  assertion_response_ptr->info->raw_id = Base64ToBytes(kTestCredentialId);
  assertion_response_ptr->signature = Base64ToBytes(kTestSignature);

  base::Value::Dict assertion_response_json =
      fido_authenticator().ParseAssertionResponse(
          std::move(assertion_response_ptr));
  EXPECT_EQ(kTestCredentialId,
            *assertion_response_json.FindString("credential_id"));
  EXPECT_EQ(kTestSignature, *assertion_response_json.FindString("signature"));
}

TEST_F(CreditCardFidoAuthenticatorTest, ParseCreationOptions) {
  base::Value::Dict creation_options_json =
      GetTestCreationOptions(kTestChallenge, kTestRelyingPartyId);

  blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options_ptr =
      fido_authenticator().ParseCreationOptions(
          std::move(creation_options_json));
  EXPECT_EQ(kTestChallenge, BytesToBase64(creation_options_ptr->challenge));
  EXPECT_EQ(kTestRelyingPartyId, creation_options_ptr->relying_party.id);

  // Ensure only platform authenticators are allowed.
  EXPECT_EQ(
      device::AuthenticatorAttachment::kPlatform,
      creation_options_ptr->authenticator_selection->authenticator_attachment);
  EXPECT_EQ(device::UserVerificationRequirement::kRequired,
            creation_options_ptr->authenticator_selection
                ->user_verification_requirement);
}

TEST_F(CreditCardFidoAuthenticatorTest, ParseAttestationResponse) {
  blink::mojom::MakeCredentialAuthenticatorResponsePtr
      attestation_response_ptr =
          blink::mojom::MakeCredentialAuthenticatorResponse::New();
  attestation_response_ptr->info = blink::mojom::CommonCredentialInfo::New();
  attestation_response_ptr->attestation_object = Base64ToBytes(kTestSignature);

  base::Value::Dict attestation_response_json =
      fido_authenticator().ParseAttestationResponse(
          std::move(attestation_response_ptr));
  EXPECT_EQ(kTestSignature, *attestation_response_json.FindStringByDottedPath(
                                "fido_attestation_info.attestation_object"));
}

TEST_F(CreditCardFidoAuthenticatorTest, AuthenticateCard_BadRequestOptions) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  fido_authenticator().Authenticate(card, requester().GetWeakPtr(),
                                    base::Value::Dict());
  EXPECT_FALSE((*requester().did_succeed()));
}

TEST_F(CreditCardFidoAuthenticatorTest,
       AuthenticateCard_UserVerificationFailed) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  fido_authenticator().Authenticate(
      card, requester().GetWeakPtr(),
      GetTestRequestOptions(kTestChallenge, kTestRelyingPartyId,
                            kTestCredentialId));

  TestCreditCardFidoAuthenticator::GetAssertion(&fido_authenticator(),
                                                /*did_succeed=*/false);
  EXPECT_FALSE((*requester().did_succeed()));
}

TEST_F(CreditCardFidoAuthenticatorTest,
       AuthenticateCard_PaymentsResponseError) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  fido_authenticator().Authenticate(
      card, requester().GetWeakPtr(),
      GetTestRequestOptions(kTestChallenge, kTestRelyingPartyId,
                            kTestCredentialId));
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            fido_authenticator().current_flow());

  // Mock user verification.
  TestCreditCardFidoAuthenticator::GetAssertion(&fido_authenticator(),
                                                /*did_succeed=*/true);
  GetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kNetworkError,
             "");

  EXPECT_FALSE((*requester().did_succeed()));
}

TEST_F(CreditCardFidoAuthenticatorTest,
       AuthenticateCard_PaymentsResponseVcnRetrievalError) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  fido_authenticator().Authenticate(
      card, requester().GetWeakPtr(),
      GetTestRequestOptions(kTestChallenge, kTestRelyingPartyId,
                            kTestCredentialId));
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            fido_authenticator().current_flow());

  // Mock user verification.
  TestCreditCardFidoAuthenticator::GetAssertion(&fido_authenticator(),
                                                /*did_succeed=*/true);
  GetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::
                 kVcnRetrievalPermanentFailure,
             "", /*is_virtual_card=*/true);

  EXPECT_FALSE((*requester().did_succeed()));
  EXPECT_EQ(
      requester().failure_type(),
      payments::FullCardRequest::VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE);
}

TEST_F(CreditCardFidoAuthenticatorTest, AuthenticateCard_Success) {
  CreditCard card = CreateServerCard(kTestGUID, kTestNumber);

  fido_authenticator().Authenticate(
      card, requester().GetWeakPtr(),
      GetTestRequestOptions(kTestChallenge, kTestRelyingPartyId,
                            kTestCredentialId));
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            fido_authenticator().current_flow());

  // Mock user verification and payments response.
  TestCreditCardFidoAuthenticator::GetAssertion(&fido_authenticator(),
                                                /*did_succeed=*/true);
  GetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
             kTestNumber);

  EXPECT_TRUE((*requester().did_succeed()));
  EXPECT_EQ(kTestNumber16, requester().number());
}

TEST_F(CreditCardFidoAuthenticatorTest, OptIn_PaymentsResponseError) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(kTestAuthToken);
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_FETCH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());

  // Mock payments response.
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kNetworkError,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());
  histogram_tester.ExpectUniqueSample(
      histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
}

TEST_F(CreditCardFidoAuthenticatorTest, OptIn_Success) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(kTestAuthToken);
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_FETCH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());

  // Mock payments response.
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());
  histogram_tester.ExpectUniqueSample(
      histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
}

TEST_F(CreditCardFidoAuthenticatorTest, Register_BadCreationOptions) {
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(
      kTestAuthToken,
      GetTestCreationOptions(/*challenge=*/"", kTestRelyingPartyId));

  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());
}

TEST_F(CreditCardFidoAuthenticatorTest, Register_UserResponseFailure) {
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(
      kTestAuthToken,
      GetTestCreationOptions(kTestChallenge, kTestRelyingPartyId));
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());

  // Mock user response and payments response.
  TestCreditCardFidoAuthenticator::MakeCredential(&fido_authenticator(),
                                                  /*did_succeed=*/false);
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());
}

TEST_F(CreditCardFidoAuthenticatorTest, Register_Success) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(
      kTestAuthToken,
      GetTestCreationOptions(kTestChallenge, kTestRelyingPartyId));
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());

  // Mock user response and payments response.
  TestCreditCardFidoAuthenticator::MakeCredential(&fido_authenticator(),
                                                  /*did_succeed=*/true);
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithCreationChallenge, 1);
}

TEST_F(CreditCardFidoAuthenticatorTest,
       Register_EnrollAttemptReturnsCreationOptions) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(kTestAuthToken);
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_FETCH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());

  // Mock payments response with challenge to invoke enrollment flow.
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false, /*include_creation_options=*/true);
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  // Mock user response and second payments response.
  TestCreditCardFidoAuthenticator::MakeCredential(&fido_authenticator(),
                                                  /*did_succeed=*/true);
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());

  histogram_tester.ExpectTotalCount(histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithCreationChallenge, 1);
}

#if !BUILDFLAG(IS_ANDROID)
// This test is not applicable for Android (we won't opt-in with Register).
TEST_F(CreditCardFidoAuthenticatorTest,
       Register_OptInAttemptReturnsRequestOptions) {
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Register(kTestAuthToken);
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_FETCH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());

  // Mock payments response with challenge to invoke opt-in flow.
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false, /*include_creation_options=*/false,
            /*include_request_options=*/true);
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            fido_authenticator().current_flow());
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());

  // Mock user response and second payments response.
  TestCreditCardFidoAuthenticator::GetAssertion(&fido_authenticator(),
                                                /*did_succeed=*/true);
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());
}
#endif

TEST_F(CreditCardFidoAuthenticatorTest, Register_NewCardAuthorization) {
  SetUserOptInPreference(true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().Authorize(
      requester().GetWeakPtr(), kTestAuthToken,
      GetTestRequestOptions(kTestChallenge, kTestRelyingPartyId,
                            kTestCredentialId));
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::FOLLOWUP_AFTER_CVC_AUTH_FLOW,
            fido_authenticator().current_flow());

  // Mock user response and second payments response.
  TestCreditCardFidoAuthenticator::GetAssertion(&fido_authenticator(),
                                                /*did_succeed=*/true);
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);
  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());
}

// Test that if FIDO enrollment is offered, the enrollment histogram logs to the
// enrollment offered bucket.
TEST_F(CreditCardFidoAuthenticatorTest,
       Register_EnrollmentOfferedHistogramBucketLogs) {
  base::HistogramTester histogram_tester;

  SetUserOptInPreference(true);

  fido_authenticator().Authorize(
      requester().GetWeakPtr(), kTestAuthToken,
      GetTestRequestOptions(kTestChallenge, kTestRelyingPartyId,
                            kTestCredentialId));

  histogram_tester.ExpectUniqueSample(kEnrollmentOfferedHistogramName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

// Test that if FIDO enrollment is not offered, the enrollment histogram logs
// to the enrollment not offered bucket.
TEST_F(CreditCardFidoAuthenticatorTest,
       Register_EnrollmentNotOfferedHistogramBucketLogs) {
  base::HistogramTester histogram_tester;

  SetUserOptInPreference(true);

  fido_authenticator().Authorize(requester().GetWeakPtr(), kTestAuthToken,
                                 base::Value::Dict());

  histogram_tester.ExpectUniqueSample(kEnrollmentOfferedHistogramName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

TEST_F(CreditCardFidoAuthenticatorTest, OptOut_Success) {
  SetUserOptInPreference(true);

  EXPECT_TRUE(fido_authenticator().IsUserOptedIn());

  fido_authenticator().OptOut();
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_OUT_FLOW,
            fido_authenticator().current_flow());

  // Mock payments response.
  OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(fido_authenticator().IsUserOptedIn());
}

}  // namespace autofill
