// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager_test_base.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_payments_window_manager.h"
#include "components/autofill/core/browser/payments/test/test_credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync/test/test_sync_service.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"
#endif

namespace autofill {
namespace {
using PaymentsRpcCardType =
    payments::PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult =
    payments::TestPaymentsAutofillClient::PaymentsRpcResult;
}  // namespace

CreditCardAccessManagerTestBase::TestAccessor::TestAccessor() = default;
CreditCardAccessManagerTestBase::TestAccessor::~TestAccessor() = default;

void CreditCardAccessManagerTestBase::TestAccessor::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* card) {
  result_ = result;
  if (result == CreditCardFetchResult::kSuccess) {
    DCHECK(card);
    number_ = card->number();
    cvc_ = card->cvc();
    expiry_month_ = card->Expiration2DigitMonthAsString();
    expiry_year_ = card->Expiration4DigitYearAsString();
  }
}

CreditCardAccessManagerTestBase::CreditCardAccessManagerTestBase()
    : task_environment_(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {
  // Advance the mock clock to 2021-01-01, 00:00:00.000.
  base::Time year_2021;
  CHECK(base::Time::FromUTCExploded({.year = 2021,
                                     .month = 1,
                                     .day_of_week = 4,
                                     .day_of_month = 1,
                                     .hour = 0,
                                     .minute = 0,
                                     .second = 0,
                                     .millisecond = 0},
                                    &year_2021));
  task_environment_.AdvanceClock(year_2021 -
                                 task_environment_.GetMockClock()->Now());
}

CreditCardAccessManagerTestBase::~CreditCardAccessManagerTestBase() = default;

void CreditCardAccessManagerTestBase::SetUp() {
  autofill_client_.SetPrefs(test::PrefServiceForTesting());
  personal_data().SetPrefService(autofill_client_.GetPrefs());
  personal_data().SetSyncServiceForTest(&sync_service_);
#if BUILDFLAG(IS_IOS)
  // On iOS mandatory reauth is by default enabled. Disable it explicitly
  // to not interfere with tests that do not test reauth functionalities.
  autofill_client_.GetPrefs()->SetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth, false);
#endif
  accessor_ = std::make_unique<TestAccessor>();
  autofill_driver_ = std::make_unique<TestAutofillDriver>(&autofill_client_);

  autofill_client_.GetPaymentsAutofillClient()
      ->set_test_payments_network_interface(
          std::make_unique<payments::TestPaymentsNetworkInterface>(
              autofill_client_.GetURLLoaderFactory(),
              autofill_client_.GetIdentityManager(), &personal_data()));
  autofill_client_.set_test_strike_database(
      std::make_unique<TestStrikeDatabase>());
  autofill_driver_->set_autofill_manager(
      std::make_unique<TestBrowserAutofillManager>(autofill_driver_.get()));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  autofill_driver_->SetAuthenticator(new TestInternalAuthenticator());
  test_api(credit_card_access_manager())
      .set_fido_authenticator(std::make_unique<TestCreditCardFidoAuthenticator>(
          autofill_driver_.get(), &autofill_client_));
#endif
  auto otp_authenticator =
      std::make_unique<TestCreditCardOtpAuthenticator>(&autofill_client_);
  otp_authenticator_ = otp_authenticator.get();
  autofill_client_.GetPaymentsAutofillClient()->set_otp_authenticator(
      std::move(otp_authenticator));

  // Force creation of the CreditCardAccessManager.
  std::ignore = credit_card_access_manager();
}

bool CreditCardAccessManagerTestBase::IsAuthenticationInProgress() {
  return test_api(credit_card_access_manager()).is_authentication_in_progress();
}

void CreditCardAccessManagerTestBase::ResetFetchCreditCard() {
  test_api(credit_card_access_manager())
      .set_is_authentication_in_progress(false);
  test_api(credit_card_access_manager()).set_can_fetch_unmask_details(true);
  test_api(credit_card_access_manager())
      .set_unmask_details_request_in_progress(false);
  test_api(credit_card_access_manager()).set_is_user_verifiable(std::nullopt);
}

void CreditCardAccessManagerTestBase::ClearCards() {
  personal_data().test_payments_data_manager().ClearCreditCards();
}

void CreditCardAccessManagerTestBase::CreateLocalCard(std::string guid,
                                                      std::string number) {
  CreditCard local_card = CreditCard();
  test::SetCreditCardInfo(&local_card, "Elvis Presley", number.c_str(),
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1", kTestCvc16);
  local_card.set_guid(guid);
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);

  personal_data().payments_data_manager().AddCreditCard(local_card);
}

CreditCard* CreditCardAccessManagerTestBase::CreateServerCard(
    std::string guid,
    std::string number,
    std::string server_id) {
  CreditCard server_card = CreditCard();
  test::SetCreditCardInfo(&server_card, "Elvis Presley", number.c_str(),
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1", kTestCvc16);
  server_card.set_guid(guid);
  server_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  server_card.set_server_id(server_id);
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  return personal_data().payments_data_manager().GetCreditCardByGUID(guid);
}

CreditCardCvcAuthenticator&
CreditCardAccessManagerTestBase::GetCvcAuthenticator() {
  return autofill_client_.GetPaymentsAutofillClient()->GetCvcAuthenticator();
}

void CreditCardAccessManagerTestBase::MockUserResponseForCvcAuth(
    std::u16string cvc,
    bool enable_fido) {
  payments::FullCardRequest* full_card_request =
      GetCvcAuthenticator().full_card_request_.get();
  if (!full_card_request) {
    return;
  }

  // Mock user response.
  payments::FullCardRequest::UserProvidedUnmaskDetails details;
  details.cvc = cvc;
#if BUILDFLAG(IS_ANDROID)
  details.enable_fido_auth = enable_fido;
#endif
  full_card_request->OnUnmaskPromptAccepted(details);
  full_card_request->OnDidGetUnmaskRiskData(/*risk_data=*/"");
}

bool CreditCardAccessManagerTestBase::GetRealPanForCVCAuth(
    PaymentsRpcResult result,
    const std::string& real_pan,
    TestFidoRequestOptionsType test_fido_request_options_type) {
  payments::FullCardRequest* full_card_request =
      GetCvcAuthenticator().full_card_request_.get();

  if (!full_card_request) {
    return false;
  }

  MockUserResponseForCvcAuth(kTestCvc16,
                             /*enable_fido=*/test_fido_request_options_type !=
                                 TestFidoRequestOptionsType::kNotPresent);

  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  response.card_authorization_token = "dummy_card_authorization_token";
  if (test_fido_request_options_type == TestFidoRequestOptionsType::kValid) {
    response.fido_request_options = GetTestRequestOptions();
  } else if (test_fido_request_options_type ==
             TestFidoRequestOptionsType::kInvalid) {
    response.fido_request_options =
        GetTestRequestOptions(/*return_invalid_request_options=*/true);
  }
#endif
  response.card_type = PaymentsRpcCardType::kServerCard;
  full_card_request->OnDidGetRealPan(result, response.with_real_pan(real_pan));
  return true;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
void CreditCardAccessManagerTestBase::AddMaxStrikes() {
  auto* strike_database =
      GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
  CHECK(strike_database);
  strike_database->AddStrikes(strike_database->GetMaxStrikesLimit());
}

void CreditCardAccessManagerTestBase::ClearStrikes() {
  auto* strike_database =
      GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
  CHECK(strike_database);
  strike_database->ClearAllStrikes();
}

int CreditCardAccessManagerTestBase::GetStrikes() {
  auto* strike_database =
      GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
  CHECK(strike_database);
  return strike_database->GetStrikes();
}

base::Value::Dict CreditCardAccessManagerTestBase::GetTestRequestOptions(
    bool return_invalid_request_options) {
  base::Value::Dict request_options;
  request_options.Set("challenge", base::Value(kTestChallenge));
  request_options.Set("relying_party_id", base::Value(kGooglePaymentsRpid));

  // If invalid request options are to be returned, don't set key info or
  // credential ID.
  if (return_invalid_request_options) {
    return request_options;
  }

  base::Value::Dict key_info;
  key_info.Set("credential_id", base::Value(kCredentialId));
  request_options.Set("key_info", base::Value(base::Value::Type::LIST));
  request_options.FindList("key_info")->Append(std::move(key_info));
  return request_options;
}

base::Value::Dict CreditCardAccessManagerTestBase::GetTestCreationOptions() {
  base::Value::Dict creation_options;
  creation_options.Set("challenge", base::Value(kTestChallenge));
  creation_options.Set("relying_party_id", base::Value(kGooglePaymentsRpid));
  return creation_options;
}

bool CreditCardAccessManagerTestBase::GetRealPanForFIDOAuth(
    PaymentsRpcResult result,
    const std::string& real_pan,
    const std::string& dcvv,
    bool is_virtual_card) {
  payments::FullCardRequest* full_card_request =
      GetFIDOAuthenticator()->full_card_request_.get();

  if (!full_card_request) {
    return false;
  }

  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.card_type = is_virtual_card ? PaymentsRpcCardType::kVirtualCard
                                       : PaymentsRpcCardType::kServerCard;
  full_card_request->OnDidGetRealPan(
      result, response.with_real_pan(real_pan).with_dcvv(dcvv));
  return true;
}

void CreditCardAccessManagerTestBase::OptChange(PaymentsRpcResult result,
                                                bool user_is_opted_in,
                                                bool include_creation_options,
                                                bool include_request_options) {
  payments::PaymentsNetworkInterface::OptChangeResponseDetails response;
  response.user_is_opted_in = user_is_opted_in;
  if (include_creation_options) {
    response.fido_creation_options = GetTestCreationOptions();
  }
  if (include_request_options) {
    response.fido_request_options = GetTestRequestOptions();
  }
  GetFIDOAuthenticator()->OnDidGetOptChangeResult(result, response);
}

TestCreditCardFidoAuthenticator*
CreditCardAccessManagerTestBase::GetFIDOAuthenticator() {
  return static_cast<TestCreditCardFidoAuthenticator*>(
      credit_card_access_manager().GetOrCreateFidoAuthenticator());
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void CreditCardAccessManagerTestBase::AcceptWebauthnOfferDialog(
    bool did_accept) {
  GetFIDOAuthenticator()->OnWebauthnOfferDialogUserResponse(did_accept);
}
#endif

void CreditCardAccessManagerTestBase::InvokeDelayedGetUnmaskDetailsResponse() {
  test_api(credit_card_access_manager())
      .OnDidGetUnmaskDetails(PaymentsRpcResult::kSuccess,
                             *payments_network_interface().unmask_details());
}

void CreditCardAccessManagerTestBase::InvokeUnmaskDetailsTimeout() {
  test_api(credit_card_access_manager())
      .ready_to_start_authentication()
      .Signal();
  test_api(credit_card_access_manager()).set_can_fetch_unmask_details(true);
}

void CreditCardAccessManagerTestBase::WaitForCallbacks() {
  task_environment_.RunUntilIdle();
}

void CreditCardAccessManagerTestBase::SetCreditCardFIDOAuthEnabled(
    bool enabled) {
  prefs::SetCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs(), enabled);
}

bool CreditCardAccessManagerTestBase::IsCreditCardFIDOAuthEnabled() {
  return prefs::IsCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs());
}

UnmaskAuthFlowType CreditCardAccessManagerTestBase::GetUnmaskAuthFlowType() {
  return test_api(credit_card_access_manager()).unmask_auth_flow_type();
}

void CreditCardAccessManagerTestBase::
    MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
        bool fido_authenticator_is_user_opted_in,
        bool is_user_verifiable,
        const std::vector<CardUnmaskChallengeOption>& challenge_options,
        int selected_index) {
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(
      fido_authenticator_is_user_opted_in);
#endif

  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // |is_user_verifiable_| related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager())
      .set_is_user_verifiable(is_user_verifiable);
  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with information regarding VCN auth.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  response.card_unmask_challenge_options = challenge_options;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  if (fido_authenticator_is_user_opted_in) {
    response.fido_request_options = GetTestRequestOptions();
  }
#endif
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  // This if-statement ensures that fido-related flows run correctly.
  if (fido_authenticator_is_user_opted_in) {
    // Expect the CreditCardAccessManager invokes the FIDO authenticator
    // first.
    ASSERT_TRUE(fido_authenticator().authenticate_invoked());
    EXPECT_EQ(fido_authenticator().card().number(),
              base::UTF8ToUTF16(std::string(kTestNumber)));
    EXPECT_EQ(fido_authenticator().card().record_type(),
              CreditCard::RecordType::kVirtualCard);
    ASSERT_TRUE(fido_authenticator().context_token().has_value());
    EXPECT_EQ(fido_authenticator().context_token().value(),
              "fake_context_token");

    CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response{
        .did_succeed = false};
    test_api(credit_card_access_manager())
        .OnFIDOAuthenticationComplete(fido_response);
  }
#endif

  const CardUnmaskChallengeOption& challenge_option =
      response.card_unmask_challenge_options[selected_index];

  payments::PaymentsWindowManager::Vcn3dsContext vcn_3ds_context;
  if (challenge_option.type ==
      CardUnmaskChallengeOptionType::kThreeDomainSecure) {
    EXPECT_CALL(*static_cast<payments::MockPaymentsWindowManager*>(
                    autofill_client_.GetPaymentsAutofillClient()
                        ->GetPaymentsWindowManager()),
                InitVcn3dsAuthentication)
        .Times(1)
        .WillOnce([&vcn_3ds_context](
                      payments::PaymentsWindowManager::Vcn3dsContext context) {
          vcn_3ds_context = std::move(context);
        });
  }

  test_api(credit_card_access_manager())
      .OnUserAcceptedAuthenticationSelectionDialog(challenge_option.id.value());

  // TODO(crbug.com/329523854): Check that the challenge selection acceptance
  // was handled correctly using mocks instead of test classes.
  switch (challenge_option.type) {
    case CardUnmaskChallengeOptionType::kCvc: {
      CreditCardCvcAuthenticator& cvc_authenticator =
          autofill_client_.GetPaymentsAutofillClient()->GetCvcAuthenticator();
      payments::PaymentsNetworkInterface::UnmaskRequestDetails*
          request_details =
              cvc_authenticator.GetFullCardRequest()->request_.get();
      EXPECT_EQ(request_details->card.record_type(),
                CreditCard::RecordType::kVirtualCard);
      EXPECT_EQ(request_details->card.number(),
                base::UTF8ToUTF16(std::string(kTestNumber)));
      EXPECT_EQ(request_details->context_token, "fake_context_token");
      EXPECT_EQ(request_details->selected_challenge_option->id.value(), "234");
      EXPECT_EQ(request_details->selected_challenge_option->type,
                CardUnmaskChallengeOptionType::kCvc);
      break;
    }
    case CardUnmaskChallengeOptionType::kSmsOtp:
      VerifyOnSelectChallengeOptionInvoked();
      EXPECT_EQ(otp_authenticator_->selected_challenge_option().id.value(),
                "123");
      EXPECT_EQ(otp_authenticator_->selected_challenge_option().type,
                CardUnmaskChallengeOptionType::kSmsOtp);
      EXPECT_EQ(otp_authenticator_->selected_challenge_option().challenge_info,
                u"xxx-xxx-3547");
      break;
    case CardUnmaskChallengeOptionType::kEmailOtp:
      VerifyOnSelectChallengeOptionInvoked();
      EXPECT_EQ(otp_authenticator_->selected_challenge_option().id.value(),
                "345");
      EXPECT_EQ(otp_authenticator_->selected_challenge_option().type,
                CardUnmaskChallengeOptionType::kEmailOtp);
      EXPECT_EQ(otp_authenticator_->selected_challenge_option().challenge_info,
                u"a******b@google.com");
      break;
    case CardUnmaskChallengeOptionType::kThreeDomainSecure:
      EXPECT_EQ(vcn_3ds_context.context_token, response.context_token);
      EXPECT_EQ(vcn_3ds_context.card, *virtual_card);
      EXPECT_EQ(vcn_3ds_context.challenge_option.type,
                CardUnmaskChallengeOptionType::kThreeDomainSecure);
      EXPECT_TRUE(vcn_3ds_context.user_consent_already_given);
      break;
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void CreditCardAccessManagerTestBase::VerifyOnSelectChallengeOptionInvoked() {
  DCHECK(otp_authenticator_);
  EXPECT_TRUE(otp_authenticator_->on_challenge_option_selected_invoked());
  EXPECT_EQ(otp_authenticator_->card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(otp_authenticator_->card().record_type(),
            CreditCard::RecordType::kVirtualCard);
  EXPECT_EQ(otp_authenticator_->context_token(), "fake_context_token");
}

CreditCardAccessManager&
CreditCardAccessManagerTestBase::credit_card_access_manager() {
  return static_cast<BrowserAutofillManager&>(
             autofill_driver_->GetAutofillManager())
      .GetCreditCardAccessManager();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
TestCreditCardFidoAuthenticator&
CreditCardAccessManagerTestBase::fido_authenticator() {
  return static_cast<TestCreditCardFidoAuthenticator&>(
      *credit_card_access_manager().GetOrCreateFidoAuthenticator());
}
#endif

payments::TestPaymentsNetworkInterface&
CreditCardAccessManagerTestBase::payments_network_interface() {
  return *autofill_client_.GetPaymentsAutofillClient()
              ->GetPaymentsNetworkInterface();
}

TestPersonalDataManager& CreditCardAccessManagerTestBase::personal_data() {
  return *autofill_client_.GetPersonalDataManager();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
void CreditCardAccessManagerTestBase::OptUserInToFido() {
  std::string other_server_id = "00000000-0000-0000-0000-000000000034";
  // Add a random FIDO eligible card, it will return RequestOptions in unmask
  // details.
  payments_network_interface().AddFidoEligibleCard(
      other_server_id, kCredentialId, kGooglePaymentsRpid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
}
#endif

}  // namespace autofill
