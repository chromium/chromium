// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

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
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"
#include "components/autofill/core/browser/payments/test/test_credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"
#include "content/public/test/mock_navigation_handle.h"
#endif

using base::ASCIIToUTF16;
using testing::NiceMock;

namespace autofill {
namespace {

const char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
const char kTestGUID2[] = "00000000-0000-0000-0000-000000000002";
const char kTestNumber[] = "4234567890123456";  // Visa
const char kTestNumber2[] = "5454545454545454";
const char16_t kTestNumber16[] = u"4234567890123456";
const char16_t kTestCvc16[] = u"123";
const char kTestServerId[] = "server_id_1";
const char kTestServerId2[] = "server_id_2";

using autofill_metrics::CreditCardFormEventLogger;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
const char kTestCvc[] = "123";
// Base64 encoding of "This is a test challenge".
constexpr char kTestChallenge[] = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
// Base64 encoding of "This is a test Credential ID".
const char kCredentialId[] = "VGhpcyBpcyBhIHRlc3QgQ3JlZGVudGlhbCBJRC4=";
const char kGooglePaymentsRpid[] = "google.com";

std::string BytesToBase64(const std::vector<uint8_t> bytes) {
  std::string base64;
  base::Base64Encode(std::string(bytes.begin(), bytes.end()), &base64);
  return base64;
}
#endif

class TestAccessor : public CreditCardAccessManager::Accessor {
 public:
  TestAccessor() = default;

  base::WeakPtr<TestAccessor> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* card,
                           const std::u16string& cvc) override {
    result_ = result;
    if (result == CreditCardFetchResult::kSuccess) {
      DCHECK(card);
      number_ = card->number();
      cvc_ = cvc;
      expiry_month_ = card->Expiration2DigitMonthAsString();
      expiry_year_ = card->Expiration4DigitYearAsString();
    }
  }

  std::u16string number() { return number_; }
  std::u16string cvc() { return cvc_; }
  std::u16string expiry_month() { return expiry_month_; }
  std::u16string expiry_year() { return expiry_year_; }
  CreditCardFetchResult result() { return result_; }

 private:
  // The result of the credit card fetching.
  CreditCardFetchResult result_ = CreditCardFetchResult::kNone;
  // The card number returned from OnCreditCardFetched().
  std::u16string number_;
  // The returned CVC, if any.
  std::u16string cvc_;
  // The two-digit expiration month in string.
  std::u16string expiry_month_;
  // The four-digit expiration year in string.
  std::u16string expiry_year_;
  base::WeakPtrFactory<TestAccessor> weak_ptr_factory_{this};
};

}  // namespace

class CreditCardAccessManagerTest : public testing::Test {
 public:
  CreditCardAccessManagerTest()
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

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data().Init(/*profile_database=*/database_,
                         /*account_database=*/nullptr,
                         /*pref_service=*/autofill_client_.GetPrefs(),
                         /*local_state=*/autofill_client_.GetPrefs(),
                         /*identity_manager=*/nullptr,
                         /*history_service=*/nullptr,
                         /*sync_service=*/&sync_service_,
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr);
    personal_data().SetPrefService(autofill_client_.GetPrefs());

    accessor_ = std::make_unique<TestAccessor>();
    autofill_driver_ = std::make_unique<TestAutofillDriver>();

    payments_client_ = new payments::TestPaymentsClient(
        autofill_client_.GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data());
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client_));
    autofill_client_.set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);
    credit_card_access_manager_ =
        browser_autofill_manager_->GetCreditCardAccessManager();
    autofill_driver_->set_autofill_manager(
        std::move(browser_autofill_manager_));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    autofill_driver_->SetAuthenticator(new TestInternalAuthenticator());
    auto fido_authenticator = std::make_unique<TestCreditCardFidoAuthenticator>(
        autofill_driver_.get(), &autofill_client_);
    fido_authenticator_ = fido_authenticator.get();
    credit_card_access_manager_->set_fido_authenticator_for_testing(
        std::move(fido_authenticator));
#endif
    auto otp_authenticator =
        std::make_unique<TestCreditCardOtpAuthenticator>(&autofill_client_);
    otp_authenticator_ = otp_authenticator.get();
    autofill_client_.set_otp_authenticator(std::move(otp_authenticator));
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_driver_.reset();

    personal_data().SetPrefService(nullptr);
    personal_data().ClearCreditCards();
  }

  bool IsAuthenticationInProgress() {
    return credit_card_access_manager_->is_authentication_in_progress();
  }

  void ResetFetchCreditCard() {
    // Resets all variables related to credit card fetching.
    credit_card_access_manager_->is_authentication_in_progress_ = false;
    credit_card_access_manager_->can_fetch_unmask_details_ = true;
    credit_card_access_manager_->is_user_verifiable_ = absl::nullopt;
  }

  void ClearCards() { personal_data().ClearCreditCards(); }

  void CreateLocalCard(std::string guid, std::string number = std::string()) {
    CreditCard local_card = CreditCard();
    test::SetCreditCardInfo(&local_card, "Elvis Presley", number.c_str(),
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
    local_card.set_guid(guid);
    local_card.set_record_type(CreditCard::LOCAL_CARD);

    personal_data().AddCreditCard(local_card);
  }

  void CreateServerCard(std::string guid,
                        std::string number = std::string(),
                        bool masked = true,
                        std::string server_id = std::string()) {
    CreditCard server_card = CreditCard();
    test::SetCreditCardInfo(&server_card, "Elvis Presley", number.c_str(),
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
    server_card.set_guid(guid);
    server_card.set_record_type(masked ? CreditCard::MASKED_SERVER_CARD
                                       : CreditCard::FULL_SERVER_CARD);
    server_card.set_server_id(server_id);
    personal_data().AddServerCreditCard(server_card);
  }

  CreditCardCvcAuthenticator* GetCvcAuthenticator() {
    return autofill_client_.GetCvcAuthenticator();
  }

  void MockUserResponseForCvcAuth(std::u16string cvc, bool enable_fido) {
    payments::FullCardRequest* full_card_request =
        GetCvcAuthenticator()->full_card_request_.get();
    if (!full_card_request)
      return;

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = cvc;
#if BUILDFLAG(IS_ANDROID)
    details.enable_fido_auth = enable_fido;
#endif
    full_card_request->OnUnmaskPromptAccepted(details);
    full_card_request->OnDidGetUnmaskRiskData(/*risk_data=*/"");
  }

  // Returns true if full card request was sent from CVC auth.
  bool GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult result,
                            const std::string& real_pan,
                            bool fido_opt_in = false,
                            bool follow_with_fido_auth = false,
                            bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        GetCvcAuthenticator()->full_card_request_.get();

    if (!full_card_request)
      return false;

    MockUserResponseForCvcAuth(kTestCvc16, follow_with_fido_auth);

    payments::PaymentsClient::UnmaskResponseDetails response;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    response.card_authorization_token = "dummy_card_authorization_token";
    if (follow_with_fido_auth) {
      response.fido_request_options = GetTestRequestOptions();
    }
#endif
    response.card_type = is_virtual_card
                             ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                             : AutofillClient::PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
    return true;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  void AddMaxStrikes() {
    auto* strike_database =
        GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
    strike_database->AddStrikes(strike_database->GetMaxStrikesLimit());
  }

  void ClearStrikes() {
    return GetFIDOAuthenticator()
        ->GetOrCreateFidoAuthenticationStrikeDatabase()
        ->ClearAllStrikes();
  }

  int GetStrikes() {
    return GetFIDOAuthenticator()
        ->GetOrCreateFidoAuthenticationStrikeDatabase()
        ->GetStrikes();
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

  base::Value::Dict GetTestCreationOptions() {
    base::Value::Dict creation_options;
    creation_options.Set("challenge", base::Value(kTestChallenge));
    creation_options.Set("relying_party_id", base::Value(kGooglePaymentsRpid));
    return creation_options;
  }

  // Returns true if full card request was sent from FIDO auth.
  bool GetRealPanForFIDOAuth(AutofillClient::PaymentsRpcResult result,
                             const std::string& real_pan,
                             const std::string& dcvv = std::string(),
                             bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        GetFIDOAuthenticator()->full_card_request_.get();

    if (!full_card_request)
      return false;

    payments::PaymentsClient::UnmaskResponseDetails response;
    response.card_type = is_virtual_card
                             ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                             : AutofillClient::PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(
        result, response.with_real_pan(real_pan).with_dcvv(dcvv));
    return true;
  }

  // Mocks an OptChange response from Payments Client.
  void OptChange(AutofillClient::PaymentsRpcResult result,
                 bool user_is_opted_in,
                 bool include_creation_options = false,
                 bool include_request_options = false) {
    payments::PaymentsClient::OptChangeResponseDetails response;
    response.user_is_opted_in = user_is_opted_in;
    if (include_creation_options) {
      response.fido_creation_options = GetTestCreationOptions();
    }
    if (include_request_options) {
      response.fido_request_options = GetTestRequestOptions();
    }
    GetFIDOAuthenticator()->OnDidGetOptChangeResult(result, response);
  }

  TestCreditCardFidoAuthenticator* GetFIDOAuthenticator() {
    return static_cast<TestCreditCardFidoAuthenticator*>(
        credit_card_access_manager_->GetOrCreateFidoAuthenticator());
  }
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Mocks user response for the offer dialog.
  void AcceptWebauthnOfferDialog(bool did_accept) {
    GetFIDOAuthenticator()->OnWebauthnOfferDialogUserResponse(did_accept);
  }
#endif

  void InvokeDelayedGetUnmaskDetailsResponse() {
    credit_card_access_manager_->OnDidGetUnmaskDetails(
        AutofillClient::PaymentsRpcResult::kSuccess,
        *payments_client_->unmask_details());
  }

  void InvokeUnmaskDetailsTimeout() {
    credit_card_access_manager_->ready_to_start_authentication_.Signal();
    credit_card_access_manager_->can_fetch_unmask_details_ = true;
  }

  void WaitForCallbacks() { task_environment_.RunUntilIdle(); }

  void SetCreditCardFIDOAuthEnabled(bool enabled) {
    prefs::SetCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs(), enabled);
  }

  bool IsCreditCardFIDOAuthEnabled() {
    return prefs::IsCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs());
  }

  UnmaskAuthFlowType getUnmaskAuthFlowType() {
    return credit_card_access_manager_->unmask_auth_flow_type_;
  }

  void MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      bool fido_authenticator_is_user_opted_in,
      bool is_user_verifiable,
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      int selected_index) {
    CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
    CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
    virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    fido_authenticator_->set_is_user_opted_in(
        fido_authenticator_is_user_opted_in);
#endif

    // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
    // |is_user_verifiable_| related logic from CreditCardAccessManager to
    // CreditCardFidoAuthenticator.
    credit_card_access_manager_->is_user_verifiable_ = is_user_verifiable;
    credit_card_access_manager_->FetchCreditCard(virtual_card,
                                                 accessor_->GetWeakPtr());

    // Ensures the UnmaskRequestDetails is populated with correct contents.
    EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
    EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
    EXPECT_TRUE(payments_client_->unmask_request()
                    ->last_committed_primary_main_frame_origin.has_value());

    // Mock server response with information regarding VCN auth.
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.context_token = "fake_context_token";
    response.card_unmask_challenge_options = challenge_options;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    if (fido_authenticator_is_user_opted_in)
      response.fido_request_options = GetTestRequestOptions();
#endif
    credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
        AutofillClient::PaymentsRpcResult::kSuccess, response);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    // This if-statement ensures that fido-related flows run correctly.
    if (fido_authenticator_is_user_opted_in) {
      // Expect the CreditCardAccessManager invokes the FIDO authenticator
      // first.
      DCHECK(fido_authenticator_);
      ASSERT_TRUE(fido_authenticator_->authenticate_invoked());
      EXPECT_EQ(fido_authenticator_->card().number(),
                base::UTF8ToUTF16(std::string(kTestNumber)));
      EXPECT_EQ(fido_authenticator_->card().record_type(),
                CreditCard::VIRTUAL_CARD);
      ASSERT_TRUE(fido_authenticator_->context_token().has_value());
      EXPECT_EQ(fido_authenticator_->context_token().value(),
                "fake_context_token");

      CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response{
          .did_succeed = false};
      credit_card_access_manager_->OnFIDOAuthenticationComplete(fido_response);
    }
#endif

    const CardUnmaskChallengeOption& challenge_option =
        response.card_unmask_challenge_options[selected_index];
    credit_card_access_manager_->OnUserAcceptedAuthenticationSelectionDialog(
        challenge_option.id.value());

    switch (challenge_option.type) {
      case CardUnmaskChallengeOptionType::kCvc: {
        CreditCardCvcAuthenticator* cvc_authenticator =
            autofill_client_.GetCvcAuthenticator();
        DCHECK(cvc_authenticator);
        payments::PaymentsClient::UnmaskRequestDetails* request_details =
            cvc_authenticator->GetFullCardRequest()->request_.get();
        EXPECT_EQ(request_details->card.record_type(),
                  CreditCard::VIRTUAL_CARD);
        EXPECT_EQ(request_details->card.number(),
                  base::UTF8ToUTF16(std::string(kTestNumber)));
        EXPECT_EQ(request_details->context_token, "fake_context_token");
        EXPECT_EQ(request_details->selected_challenge_option->id.value(),
                  "234");
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
        EXPECT_EQ(
            otp_authenticator_->selected_challenge_option().challenge_info,
            u"xxx-xxx-3547");
        break;
      case CardUnmaskChallengeOptionType::kEmailOtp:
        VerifyOnSelectChallengeOptionInvoked();
        EXPECT_EQ(otp_authenticator_->selected_challenge_option().id.value(),
                  "345");
        EXPECT_EQ(otp_authenticator_->selected_challenge_option().type,
                  CardUnmaskChallengeOptionType::kEmailOtp);
        EXPECT_EQ(
            otp_authenticator_->selected_challenge_option().challenge_info,
            u"a******b@google.com");
        break;
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED();
        break;
    }
  }

  void VerifyOnSelectChallengeOptionInvoked() {
    DCHECK(otp_authenticator_);
    EXPECT_TRUE(otp_authenticator_->on_challenge_option_selected_invoked());
    EXPECT_EQ(otp_authenticator_->card().number(),
              base::UTF8ToUTF16(std::string(kTestNumber)));
    EXPECT_EQ(otp_authenticator_->card().record_type(),
              CreditCard::VIRTUAL_CARD);
    EXPECT_EQ(otp_authenticator_->context_token(), "fake_context_token");
  }

 protected:
  TestPersonalDataManager& personal_data() {
    return *autofill_client_.GetPersonalDataManager();
  }

  std::unique_ptr<TestAccessor> accessor_;
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  syncer::TestSyncService sync_service_;
  raw_ptr<payments::TestPaymentsClient, DanglingUntriaged> payments_client_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  scoped_refptr<AutofillWebDataService> database_;
  std::unique_ptr<BrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<CreditCardAccessManager, DanglingUntriaged>
      credit_card_access_manager_;
  raw_ptr<TestCreditCardOtpAuthenticator> otp_authenticator_;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  raw_ptr<TestCreditCardFidoAuthenticator, DanglingUntriaged>
      fido_authenticator_;
#endif
};

// Ensures DeleteCard() successfully removes local cards.
TEST_F(CreditCardAccessManagerTest, RemoveLocalCreditCard) {
  CreateLocalCard(kTestGUID);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  EXPECT_TRUE(personal_data().GetCreditCardByGUID(kTestGUID));
  EXPECT_TRUE(credit_card_access_manager_->DeleteCard(card));
  EXPECT_FALSE(personal_data().GetCreditCardByGUID(kTestGUID));
}

// Ensures DeleteCard() does nothing for server cards.
TEST_F(CreditCardAccessManagerTest, RemoveServerCreditCard) {
  CreateServerCard(kTestGUID);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  EXPECT_TRUE(personal_data().GetCreditCardByGUID(kTestGUID));
  EXPECT_FALSE(credit_card_access_manager_->DeleteCard(card));

  // Cannot delete server cards.
  EXPECT_TRUE(personal_data().GetCreditCardByGUID(kTestGUID));
}

// Ensures GetDeletionConfirmationText(~) returns correct values for local
// cards.
TEST_F(CreditCardAccessManagerTest, LocalCardGetDeletionConfirmationText) {
  CreateLocalCard(kTestGUID);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  std::u16string title = std::u16string();
  std::u16string body = std::u16string();
  EXPECT_TRUE(credit_card_access_manager_->GetDeletionConfirmationText(
      card, &title, &body));

  // |title| and |body| should be updated appropriately.
  EXPECT_EQ(title, card->CardNameAndLastFourDigits());
  EXPECT_EQ(body,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
}

// Ensures GetDeletionConfirmationText(~) returns false for server cards.
TEST_F(CreditCardAccessManagerTest, ServerCardGetDeletionConfirmationText) {
  CreateServerCard(kTestGUID);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  std::u16string title = std::u16string();
  std::u16string body = std::u16string();
  EXPECT_FALSE(credit_card_access_manager_->GetDeletionConfirmationText(
      card, &title, &body));

  // |title| and |body| should remain unchanged.
  EXPECT_EQ(title, std::u16string());
  EXPECT_EQ(body, std::u16string());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Parameters of the CreditCardAccessManagerMandatoryReauthTest:
// - bool feature_flag_is_on: Whether the mandatory re-auth feature flag is
// turned on or off.
// - bool pref_is_enabled: Whether the mandatory re-auth pref is turned on or
// off.
// - bool mandatory_reauth_response_is_success: Whether the response from the
// mandatory re-auth is a success or failure.
// - bool authentication_method_is_biometric: Whether the authentication method
// is biometric. If false, it's using screen lock.
class CreditCardAccessManagerMandatoryReauthTest
    : public CreditCardAccessManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  CreditCardAccessManagerMandatoryReauthTest() = default;
  ~CreditCardAccessManagerMandatoryReauthTest() override = default;

  void SetUp() override {
    CreditCardAccessManagerTest::SetUp();
    feature_list_.InitWithFeatureState(
        features::kAutofillEnablePaymentsMandatoryReauth, FeatureFlagIsOn());
    autofill_client_.GetPrefs()->SetBoolean(
        prefs::kAutofillPaymentMethodsMandatoryReauth,
        /*value=*/PrefIsEnabled());
  }

  bool FeatureFlagIsOn() const { return std::get<0>(GetParam()); }

  bool PrefIsEnabled() const { return std::get<1>(GetParam()); }

  bool MandatoryReauthResponseIsSuccess() const {
    return std::get<2>(GetParam());
  }

  bool isBiometric() const { return std::get<3>(GetParam()); }

  void SetUpDeviceAuthenticatorResponseMock() {
    if (isBiometric()) {
      ON_CALL(mandatory_reauth_manager(), GetAuthenticationMethod)
          .WillByDefault(testing::Return(
              payments::MandatoryReauthAuthenticationMethod::kBiometric));
    } else {
      ON_CALL(mandatory_reauth_manager(), GetAuthenticationMethod)
          .WillByDefault(testing::Return(
              payments::MandatoryReauthAuthenticationMethod::kScreenLock));
    }

    // We should only expect an AuthenticateWithMessage() call if the feature
    // flag is on and the pref is enabled.
    if (FeatureFlagIsOn() && PrefIsEnabled()) {
      ON_CALL(mandatory_reauth_manager(),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
              AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
              Authenticate)
#endif
          .WillByDefault(testing::WithArg<1>(
              testing::Invoke([mandatory_reauth_response_is_success =
                                   MandatoryReauthResponseIsSuccess()](
                                  base::OnceCallback<void(bool)> callback) {
                std::move(callback).Run(mandatory_reauth_response_is_success);
              })));
    } else {
      EXPECT_CALL(mandatory_reauth_manager(),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
                  AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
                  Authenticate)
#endif
          .Times(0);
    }
  }

 private:
  payments::MockMandatoryReauthManager& mandatory_reauth_manager() {
    return *static_cast<payments::MockMandatoryReauthManager*>(
        autofill_client_.GetOrCreatePaymentsMandatoryReauthManager());
  }

  base::test::ScopedFeatureList feature_list_;
};

// Tests that retrieving local cards works correctly in the context of the
// Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthTest,
       MandatoryReauth_FetchLocalCard) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  // The only time we should expect an error is if the feature flag is on, the
  // pref is enabled, but the mandatory re-auth authentication was not
  // successful.
  if (FeatureFlagIsOn() && PrefIsEnabled() &&
      !MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
    EXPECT_TRUE(accessor_->number().empty());
  } else {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
    EXPECT_EQ(kTestNumber16, accessor_->number());
  }
  std::string histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.LocalCard";
  histogram_name += isBiometric() ? ".Biometric" : ".ScreenLock";
  if (FeatureFlagIsOn() && PrefIsEnabled()) {
    histogram_tester.ExpectBucketCount(
        histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        1);
    histogram_tester.ExpectBucketCount(
        histogram_name,
        MandatoryReauthResponseIsSuccess()
            ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                  kFlowSucceeded
            : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                  kFlowFailed,
        1);
  }
}

// Tests that retrieving virtual cards works correctly in the context of the
// Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthTest,
       MandatoryReauth_FetchVirtualCard) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with valid card information.
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.real_pan = "4111111111111111";
  response.dcvv = "321";
  response.expiration_month = test::NextMonth();
  response.expiration_year = test::NextYear();
  response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  // Expect the accessor receives the correct response.
  if (FeatureFlagIsOn() && PrefIsEnabled() &&
      !MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  } else {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
    EXPECT_EQ(accessor_->number(), u"4111111111111111");
    EXPECT_EQ(accessor_->cvc(), u"321");
    EXPECT_EQ(accessor_->expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
    EXPECT_EQ(accessor_->expiry_year(), base::UTF8ToUTF16(test::NextYear()));
  }
  std::string histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.VirtualCard";
  histogram_name += isBiometric() ? ".Biometric" : ".ScreenLock";
  if (FeatureFlagIsOn() && PrefIsEnabled()) {
    histogram_tester.ExpectBucketCount(
        histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        1);
    histogram_tester.ExpectBucketCount(
        histogram_name,
        MandatoryReauthResponseIsSuccess()
            ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                  kFlowSucceeded
            : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                  kFlowFailed,
        1);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerMandatoryReauthTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// Tests retrieving local cards.
TEST_F(CreditCardAccessManagerTest, FetchLocalCardSuccess) {
  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  absl::optional<absl::variant<FormDataImporter::CardGuid,
                               FormDataImporter::CardLastFourDigits>>
      card_identifier =
          autofill_client_.GetFormDataImporter()
              ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted();
  ASSERT_TRUE(card_identifier.has_value());
  ASSERT_TRUE(absl::holds_alternative<FormDataImporter::CardGuid>(
      card_identifier.value()));
  ASSERT_EQ(
      absl::get<FormDataImporter::CardGuid>(card_identifier.value()).value(),
      kTestGUID);
}

// Ensures that FetchCreditCard() reports a failure when a card does not exist.
TEST_F(CreditCardAccessManagerTest, FetchNullptrFailure) {
  personal_data().ClearCreditCards();

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(nullptr,
                                               accessor_->GetWeakPtr());
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);
}

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCSuccess) {
  for (bool enable_downstream_histogram_remake : {true, false}) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (enable_downstream_histogram_remake) {
      scoped_feature_list.InitAndEnableFeature(
          features::kAutofillEnableRemadeDownstreamMetrics);
    } else {
      scoped_feature_list.InitAndDisableFeature(
          features::kAutofillEnableRemadeDownstreamMetrics);
    }
    CreateServerCard(kTestGUID, kTestNumber);
    CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
    base::HistogramTester histogram_tester;
    std::string flow_events_histogram_name =
        "Autofill.BetterAuth.FlowEvents.Cvc";

    credit_card_access_manager_->PrepareToFetchCreditCard();
    WaitForCallbacks();

    credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
    histogram_tester.ExpectUniqueSample(
        flow_events_histogram_name,
        CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

    EXPECT_TRUE(GetRealPanForCVCAuth(
        AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber));
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
    EXPECT_EQ(kTestNumber16, accessor_->number());
    EXPECT_EQ(kTestCvc16, accessor_->cvc());

    histogram_tester.ExpectBucketCount(
        flow_events_histogram_name,
        CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
    if (enable_downstream_histogram_remake) {
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.ServerCard.Attempt", true, 1);
    } else {
      histogram_tester.ExpectBucketCount(
          "Autofill.ServerCardUnmask.ServerCard.Attempt", true, 0);
    }
    // Expect that we did not signal that there was no interactive
    // authentication.
    EXPECT_FALSE(
        autofill_client_.GetFormDataImporter()
            ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()
            .has_value());
  }
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCNetworkError) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(GetRealPanForCVCAuth(
      AutofillClient::PaymentsRpcResult::kNetworkError, std::string()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCPermanentFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(GetRealPanForCVCAuth(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, std::string()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);
}

// Ensures that a "try again" response from payments does not end the flow.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCTryAgainFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  EXPECT_TRUE(GetRealPanForCVCAuth(
      AutofillClient::PaymentsRpcResult::kTryAgainFailure, std::string()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Params of the CreditCardAccessManagerBetterAuthLogTest:
// -- bool has_server_card;
// -- bool is_user_opted_in;
class CreditCardAccessManagerBetterAuthLogTest
    : public CreditCardAccessManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CreditCardAccessManagerBetterAuthLogTest() = default;
  ~CreditCardAccessManagerBetterAuthLogTest() override = default;

  bool HasServerCard() { return std::get<0>(GetParam()); }
  bool IsUserOptedIn() { return std::get<1>(GetParam()); }

  void SetUp() override {
    ClearCards();
    if (HasServerCard()) {
      CreateServerCard(kTestGUID, kTestNumber);
    } else {
      CreateLocalCard(kTestGUID, kTestNumber);
    }
    CreditCardAccessManagerTest::SetUp();
  }

  const std::string kVerifiabilityCheckDurationMetrics =
      "Autofill.BetterAuth.UserVerifiabilityCheckDuration";
  const std::string kPreflightCallMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus";
  const std::string kPreflightLatencyMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightDuration";
  const std::string kPreflightFlowInitiatedMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightInitiated";
};

TEST_P(CreditCardAccessManagerBetterAuthLogTest,
       CardUnmaskPreflightCalledMetric_FidoEligible) {
  base::HistogramTester histogram_tester;
  auto* fido_authenticator = GetFIDOAuthenticator();
  fido_authenticator->SetUserVerifiable(/*is_user_verifiable=*/true);
  fido_authenticator->set_is_user_opted_in(IsUserOptedIn());
  ResetFetchCreditCard();
  credit_card_access_manager_->PrepareToFetchCreditCard();
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  histogram_tester.ExpectTotalCount(/*name=*/kVerifiabilityCheckDurationMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  if (HasServerCard()) {
    histogram_tester.ExpectUniqueSample(kPreflightCallMetrics,
                                        /*sample=*/IsUserOptedIn(),
                                        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                      /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
}

TEST_P(CreditCardAccessManagerBetterAuthLogTest,
       CardUnmaskPreflightCalledMetric_NotFidoEligible) {
  base::HistogramTester histogram_tester;
  GetFIDOAuthenticator()->SetUserVerifiable(/*is_user_verifiable=*/false);
  ResetFetchCreditCard();
  credit_card_access_manager_->PrepareToFetchCreditCard();
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  if (HasServerCard()) {
    histogram_tester.ExpectUniqueSample(
        /*name=*/kPreflightFlowInitiatedMetrics, /*sample=*/true,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectTotalCount(
        /*name=*/kVerifiabilityCheckDurationMetrics,
        /*expected_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                      /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/kVerifiabilityCheckDurationMetrics,
        /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(
      /*name=*/kPreflightCallMetrics,
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerBetterAuthLogTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// WebAuthn verification and response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOSuccess) {
  base::HistogramTester histogram_tester;
  std::string unmask_decision_histogram_name =
      "Autofill.BetterAuth.CardUnmaskTypeDecision";
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Fido";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // FIDO Success.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                    kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);

  EXPECT_EQ(kCredentialId,
            BytesToBase64(GetFIDOAuthenticator()->GetCredentialId()));
  EXPECT_EQ(kTestNumber16, accessor_->number());

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

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// WebAuthn verification and response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOSuccessWithDcvv) {
  // Opt user in for FIDO auth.
  prefs::SetCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs(), true);

  // General setup.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  // Mock Payments response that includes DCVV along with Full PAN.
  EXPECT_TRUE(GetRealPanForFIDOAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                    kTestNumber, kTestCvc));

  // Expect accessor to successfully retrieve the DCVV.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

// Ensures that CVC prompt is invoked after WebAuthn fails.
TEST_F(CreditCardAccessManagerTest,
       FetchServerCardFIDOVerificationFailureCVCFallback) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";
  std::string flow_events_fido_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Fido";
  std::string flow_events_cvc_fallback_histogram_name =
      "Autofill.BetterAuth.FlowEvents.CvcFallbackFromFido";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();
  histogram_tester.ExpectUniqueSample(
      flow_events_fido_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // FIDO Failure.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  histogram_tester.ExpectBucketCount(
      flow_events_cvc_fallback_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_FALSE(GetRealPanForFIDOAuth(
      AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  // Followed by a fallback to CVC.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::NONE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

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
TEST_F(CreditCardAccessManagerTest,
       FetchServerCardFIDOServerFailureCVCFallback) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // FIDO Failure.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, kTestNumber));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  // Followed by a fallback to CVC.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::NONE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido.ServerCard.Failure", 1);
}

// Ensures WebAuthn call is not made if Request Options is missing a Credential
// ID, and falls back to CVC.
TEST_F(CreditCardAccessManagerTest,
       FetchServerCardBadRequestOptionsCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  // Don't set Credential ID.
  payments_client_->AddFidoEligibleCard(card->server_id(), /*credential_id=*/"",
                                        kGooglePaymentsRpid);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // FIDO Failure.
  EXPECT_FALSE(GetRealPanForFIDOAuth(
      AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  // Followed by a fallback to CVC.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

// Ensures that CVC prompt is invoked when the pre-flight call to Google
// Payments times out.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOTimeoutCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

// Ensures the existence of user-perceived latency during the preflight call is
// correctly logged.
TEST_F(CreditCardAccessManagerTest,
       Metrics_LoggingExistenceOfUserPerceivedLatency) {
  // Setting up a FIDO-enabled user with a local card and a server card.
  std::string server_guid = "00000000-0000-0000-0000-000000000001";
  std::string local_guid = "00000000-0000-0000-0000-000000000003";
  CreateServerCard(server_guid, "4594299181086168");
  CreateLocalCard(local_guid, "4409763681177079");
  CreditCard* server_card = personal_data().GetCreditCardByGUID(server_guid);
  CreditCard* local_card = personal_data().GetCreditCardByGUID(local_guid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);

  for (bool user_is_opted_in : {true, false}) {
    std::string histogram_name =
        "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.";
    histogram_name += user_is_opted_in ? "OptedIn" : "OptedOut";
    SetCreditCardFIDOAuthEnabled(user_is_opted_in);

    {
      // Preflight call ignored because local card was chosen.
      base::HistogramTester histogram_tester;

      ResetFetchCreditCard();
      credit_card_access_manager_->PrepareToFetchCreditCard();
      task_environment_.FastForwardBy(base::Seconds(4));
      WaitForCallbacks();

      credit_card_access_manager_->FetchCreditCard(local_card,
                                                   accessor_->GetWeakPtr());
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::kDidNotChooseMaskedCard, 1);
    }

    {
      // Preflight call returned after card was chosen.
      base::HistogramTester histogram_tester;
      payments_client_->ShouldReturnUnmaskDetailsImmediately(false);

      ResetFetchCreditCard();
      credit_card_access_manager_->PrepareToFetchCreditCard();
      credit_card_access_manager_->FetchCreditCard(server_card,
                                                   accessor_->GetWeakPtr());
      task_environment_.FastForwardBy(base::Seconds(4));
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::
              kCardChosenBeforePreflightCallReturned,
          1);
      histogram_tester.ExpectTotalCount(
          "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
          "Duration",
          static_cast<int>(user_is_opted_in));
      histogram_tester.ExpectTotalCount(
          "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
          "TimedOutCvcFallback",
          static_cast<int>(user_is_opted_in));
    }

    {
      // Preflight call returned before card was chosen.
      base::HistogramTester histogram_tester;
      // This is important because CreditCardFidoAuthenticator will update the
      // opted-in pref according to GetDetailsForGetRealPan response.
      payments_client_->AllowFidoRegistration(!user_is_opted_in);

      ResetFetchCreditCard();
      credit_card_access_manager_->PrepareToFetchCreditCard();
      WaitForCallbacks();

      credit_card_access_manager_->FetchCreditCard(server_card,
                                                   accessor_->GetWeakPtr());
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
TEST_F(CreditCardAccessManagerTest, Metrics_LoggingTimedOutCvcFallback) {
  // Setting up a FIDO-enabled user with a local card and a server card.
  std::string server_guid = "00000000-0000-0000-0000-000000000001";
  CreateServerCard(server_guid, "4594299181086168");
  CreditCard* server_card = personal_data().GetCreditCardByGUID(server_guid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->ShouldReturnUnmaskDetailsImmediately(false);

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
    credit_card_access_manager_->PrepareToFetchCreditCard();
    credit_card_access_manager_->FetchCreditCard(server_card,
                                                 accessor_->GetWeakPtr());

    // Mock a delayed response.
    InvokeDelayedGetUnmaskDetailsResponse();

    task_environment_.FastForwardBy(base::Seconds(4));
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
    credit_card_access_manager_->PrepareToFetchCreditCard();
    credit_card_access_manager_->FetchCreditCard(server_card,
                                                 accessor_->GetWeakPtr());
    task_environment_.FastForwardBy(base::Seconds(4));
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

// Ensures that use of new card invokes authorization flow when user is
// opted-in.
TEST_F(CreditCardAccessManagerTest, FIDONewCardAuthorization) {
  base::HistogramTester histogram_tester;
  std::string unmask_decision_histogram_name =
      "Autofill.BetterAuth.CardUnmaskTypeDecision";
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.AuthenticationAfterCVC";
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.CvcThenFido";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  // Opt the user in, but don't include the card above.
  std::string other_server_id = "00000000-0000-0000-0000-000000000034";
  // Add other FIDO eligible card, it will return RequestOptions in unmask
  // details.
  payments_client_->AddFidoEligibleCard(other_server_id, kCredentialId,
                                        kGooglePaymentsRpid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Do not return any RequestOptions or CreationOptions in GetRealPan.
  // RequestOptions should have been returned in unmask details response.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false,
                                   /*follow_with_fido_auth=*/false));
  // Ensure that form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::FOLLOWUP_AFTER_CVC_AUTH_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that form is filled after user verification (OnCreditCardFetched is
  // called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Mock OptChange payments call.
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess, true);

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

// Ensures expired cards always invoke a CVC prompt instead of WebAuthn.
TEST_F(CreditCardAccessManagerTest, FetchExpiredServerCardInvokesCvcPrompt) {
  // Creating an expired server card and opting the user in with authorized
  // card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  card->SetExpirationYearFromString(u"2010");
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

#if BUILDFLAG(IS_ANDROID)
// Ensures that the WebAuthn verification prompt is invoked after user opts in
// on unmask card checkbox.
TEST_F(CreditCardAccessManagerTest, FIDOOptInSuccess_Android) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set |follow_with_fido_auth| to true to mock user checking the
  // opt-in checkbox and ensuring GetRealPan returns RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false,
                                   /*follow_with_fido_auth=*/true));
  WaitForCallbacks();

  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that form is filled after user verification (OnCreditCardFetched is
  // called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Mock OptChange payments call.
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kSuccess, 1);
}

// Ensures that the failed user verification disallows enrollment.
TEST_F(CreditCardAccessManagerTest, FIDOOptInUserVerificationFailure) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set |follow_with_fido_auth| to true to mock user checking the
  // opt-in checkbox and ensuring GetRealPan returns RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false,
                                   /*follow_with_fido_auth=*/true));
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock GetAssertion failure.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);
  // Ensure that form is still filled even if user verification fails
  // (OnCreditCardFetched is called). Note that this is different behavior than
  // registering a new card.
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kNotAllowedError,
      1);
}

// Ensures that enrollment does not happen if the server returns a failure.
TEST_F(CreditCardAccessManagerTest, FIDOOptInServerFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set |follow_with_fido_auth| to true to mock user checking the
  // opt-in checkbox and ensuring GetRealPan returns RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false,
                                   /*follow_with_fido_auth=*/true));
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response and OptChange payments call.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that form is filled after user verification (OnCreditCardFetched is
  // called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
  OptChange(AutofillClient::PaymentsRpcResult::kPermanentFailure, false);

  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());
}

// Ensures that enrollment does not happen if user unchecking the opt-in
// checkbox.
TEST_F(CreditCardAccessManagerTest, FIDOOptIn_CheckboxDeclined) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set |follow_with_fido_auth| to false to mock user unchecking
  // the opt-in checkbox and ensuring GetRealPan won't return RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false,
                                   /*follow_with_fido_auth=*/false));
  // Ensure that form is filled (OnCreditCardFetched is called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // never called.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::NONE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());
}

// Ensures that opting-in through settings page on Android successfully sends an
// opt-in request the next time the user downstreams a card.
TEST_F(CreditCardAccessManagerTest, FIDOSettingsPageOptInSuccess_Android) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);

  // Setting the local opt-in state as true and implying that Payments servers
  // has the opt-in state to false - this shows the user opted-in through the
  // settings page.
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AllowFidoRegistration(true);
  payments_client_->ShouldReturnUnmaskDetailsImmediately(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  MockUserResponseForCvcAuth(kTestCvc16, /*enable_fido=*/false);

  // Although the checkbox was hidden and |enable_fido_auth| was set to false in
  // the user request, because of the previous opt-in intention, the client must
  // request to opt-in.
  EXPECT_TRUE(
      payments_client_->unmask_request()->user_response.enable_fido_auth);
}

#else   // BUILDFLAG(IS_ANDROID)
// Ensures that the WebAuthn enrollment prompt is invoked after user opts in. In
// this case, the user is not yet enrolled server-side, and thus receives
// |creation_options|.
TEST_F(CreditCardAccessManagerTest,
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
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_client_->AllowFidoRegistration(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/true);

  // Mock user response and OptChange payments call.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/true);
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
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
TEST_F(CreditCardAccessManagerTest, FIDOEnrollment_OfferDeclined_Desktop) {
  base::HistogramTester histogram_tester;
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_client_->AllowFidoRegistration(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
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
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollment_OfferDeclinedAfterAccepting_Desktop) {
  base::HistogramTester histogram_tester;
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_client_->AllowFidoRegistration(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
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
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollment_UserVerificationFailed_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_client_->AllowFidoRegistration(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
  WaitForCallbacks();
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
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
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollmentSuccess_RequestOptions_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_client_->AllowFidoRegistration(true);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
  WaitForCallbacks();
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/false,
            /*include_request_options=*/true);

  // Mock user response and OptChange payments call.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
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

// Ensures WebAuthn result is logged correctly for a settings page opt-in.
TEST_F(CreditCardAccessManagerTest, SettingsPage_FIDOEnrollment) {
  base::HistogramTester histogram_tester;
  std::string webauthn_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.SettingsPageOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromSettingsPage";
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromSettingsPage";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromSettingsPage";

  GetFIDOAuthenticator()->SetUserVerifiable(true);

  for (bool did_succeed : {false, true}) {
    SetCreditCardFIDOAuthEnabled(false);
    credit_card_access_manager_->OnSettingsPageFIDOAuthToggled(true);

    // Mock user and payments response.
    AcceptWebauthnOfferDialog(/*did_accept=*/true);
    OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
              /*user_is_opted_in=*/false,
              /*include_creation_options=*/true);
    // Mock user response and payments response.
    TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                    did_succeed);

    histogram_tester.ExpectBucketCount(
        webauthn_histogram_name,
        did_succeed ? autofill_metrics::WebauthnResultMetric::kSuccess
                    : autofill_metrics::WebauthnResultMetric::kNotAllowedError,
        1);
  }

  histogram_tester.ExpectTotalCount(webauthn_histogram_name, 2);
  histogram_tester.ExpectTotalCount(opt_in_histogram_name, 3);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 2);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithCreationChallenge, 1);
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 2);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::kAccepted, 2);
}

// Ensure proper metrics are logged when user opts-out from settings page.
TEST_F(CreditCardAccessManagerTest, SettingsPage_OptOut) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.OptOutCalled.FromSettingsPage";
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  EXPECT_TRUE(IsCreditCardFIDOAuthEnabled());
  credit_card_access_manager_->OnSettingsPageFIDOAuthToggled(false);
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);

  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Params of the CreditCardAccessManagerBetterAuthOptInLogTest:
// -- bool is_virtual_card;
// -- bool unmask_details_offer_fido_opt_in;
// -- bool card_authorization_token_present;
// -- bool max_strikes_limit_reached;
// -- bool has_opted_in_from_android_settings;
// -- bool is_opted_in_for_fido;
class CreditCardAccessManagerBetterAuthOptInLogTest
    : public CreditCardAccessManagerTest,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, bool, bool, bool>> {
 public:
  CreditCardAccessManagerBetterAuthOptInLogTest() = default;
  ~CreditCardAccessManagerBetterAuthOptInLogTest() override = default;

  void SetUp() override {
    CreditCardAccessManagerTest::SetUp();

    if (MaxStrikesLimitReached()) {
      AddMaxStrikes();
    } else {
      ClearStrikes();
    }

    CreateServerCard(kTestGUID, kTestNumber);
    GetFIDOAuthenticator()->SetUserVerifiable(true);
#if BUILDFLAG(IS_ANDROID)
    SetCreditCardFIDOAuthEnabled(HasOptedInFromAndroidSettings());
#else
    SetCreditCardFIDOAuthEnabled(false);
#endif  // BUILDFLAG(OS_ANDROID)
    payments_client_->AllowFidoRegistration(
        /*offer_fido_opt_in=*/UnmaskDetailsOfferFidoOptIn());
    card_ = personal_data().GetCreditCardByGUID(kTestGUID);
    if (IsVirtualCard()) {
      card_->set_record_type(CreditCard::VIRTUAL_CARD);
    }
    if (IsOptedIntoFido()) {
      // If user and device are already opted into FIDO, then add an eligible
      // card to ensure that the `unmask_details_` contains fido request
      // options.
      payments_client_->AddFidoEligibleCard("random_id", kCredentialId,
                                            kGooglePaymentsRpid);
    }

    credit_card_access_manager_->PrepareToFetchCreditCard();
    credit_card_access_manager_->FetchCreditCard(card_,
                                                 accessor_->GetWeakPtr());
  }

  bool IsVirtualCard() { return std::get<0>(GetParam()); }
  bool UnmaskDetailsOfferFidoOptIn() { return std::get<1>(GetParam()); }
  bool CardAuthorizationTokenPresent() { return std::get<2>(GetParam()); }
  bool MaxStrikesLimitReached() { return std::get<3>(GetParam()); }
  bool HasOptedInFromAndroidSettings() { return std::get<4>(GetParam()); }
  bool IsOptedIntoFido() { return std::get<5>(GetParam()); }

  bool ShouldOfferFidoOptIn() {
    return !IsOptedIntoFido() && !IsVirtualCard() &&
           UnmaskDetailsOfferFidoOptIn() && CardAuthorizationTokenPresent() &&
           !MaxStrikesLimitReached();
  }

  bool ShouldOfferFidoOptInAndroid() {
    return !IsOptedIntoFido() && !IsVirtualCard() &&
           UnmaskDetailsOfferFidoOptIn() && !HasOptedInFromAndroidSettings();
  }

  const std::string GetFidoOptInNotOfferedHistogram() {
    return fido_opt_in_not_offered_histogram;
  }

  CreditCard* GetCreditCard() { return card_; }

 private:
  const std::string fido_opt_in_not_offered_histogram =
      "Autofill.BetterAuth.OptInPromoNotOfferedReason";
  raw_ptr<CreditCard, DanglingUntriaged> card_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensures that the correct metrics are logged when the FIDO opt-in dialog is
// not shown on Desktop.
TEST_P(CreditCardAccessManagerBetterAuthOptInLogTest,
       FidoOptInNotShown_Desktop) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      credit_card_access_manager_->ShouldOfferFidoOptInDialogForTesting(
          CreditCardCvcAuthenticator::CvcAuthenticationResponse()
              .with_did_succeed(true)
              .with_card(GetCreditCard())
              .with_card_authorization_token(CardAuthorizationTokenPresent()
                                                 ? "card_authorization_token"
                                                 : "")
              .with_cvc(u"123")),
      ShouldOfferFidoOptIn());

  if (IsOptedIntoFido()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn,
        /*expected_bucket_count=*/1);
  } else if (!UnmaskDetailsOfferFidoOptIn()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse,
        /*expected_bucket_count=*/1);
  } else if (!CardAuthorizationTokenPresent()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kCardAuthorizationTokenEmpty,
        /*expected_bucket_count=*/1);
  } else if (MaxStrikesLimitReached()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kBlockedByStrikeDatabase,
        /*expected_bucket_count=*/1);
  } else if (IsVirtualCard()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(GetFidoOptInNotOfferedHistogram(),
                                      /*expected_count=*/0);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
// Ensures that the correct metrics are logged when the FIDO opt-in checkbox is
// not shown on Android.
TEST_P(CreditCardAccessManagerBetterAuthOptInLogTest,
       FidoOptInNotShown_Android) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(credit_card_access_manager_->ShouldOfferFidoAuthForTesting(),
            ShouldOfferFidoOptInAndroid());

  if (IsOptedIntoFido()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn,
        /*expected_bucket_count=*/1);
  } else if (!UnmaskDetailsOfferFidoOptIn()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse,
        /*expected_bucket_count=*/1);
  } else if (HasOptedInFromAndroidSettings()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kOptedInFromSettings,
        /*expected_bucket_count=*/1);
  } else if (IsVirtualCard()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(GetFidoOptInNotOfferedHistogram(),
                                      /*expected_count=*/0);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerBetterAuthOptInLogTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Ensure that when unmask detail response is delayed, we will automatically
// fall back to CVC even if local pref and Payments mismatch.
TEST_F(CreditCardAccessManagerTest,
       IntentToOptOut_DelayedUnmaskDetailsResponse) {
  base::HistogramTester histogram_tester;
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);
  // Delay the UnmaskDetailsResponse so that we can't discover the mismatch,
  // which will use local pref and fall back to CVC.
  payments_client_->ShouldReturnUnmaskDetailsImmediately(false);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  // Ensure the auth flow type is CVC because no unmask detail response is
  // returned and local pref denotes that user is opted out.
  EXPECT_EQ(getUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
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

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
  // Since no unmask detail returned, we can't discover the pref mismatch, we
  // won't call opt out and local pref is unchanged.
  EXPECT_FALSE(GetFIDOAuthenticator()->IsOptOutCalled());
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_F(CreditCardAccessManagerTest, IntentToOptOut_OptOutAfterUnmaskSucceeds) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Also ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(getUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth success.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
  WaitForCallbacks();

  // Ensure calling opt out after a successful cvc auth.
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  // Mock opt out success response. Local pref is consistent with payments.
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_F(CreditCardAccessManagerTest, IntentToOptOut_OptOutAfterUnmaskFails) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(getUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth failure.
  EXPECT_TRUE(GetRealPanForCVCAuth(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, std::string()));
  WaitForCallbacks();

  // Ensure calling opt out after cvc auth failure.
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  // Mock opt out success. Local pref is consistent with payments.
  OptChange(AutofillClient::PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_F(CreditCardAccessManagerTest, IntentToOptOut_OptOutFailure) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_client_->AddFidoEligibleCard(card->server_id(), kCredentialId,
                                        kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager_->PrepareToFetchCreditCard();
  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Also ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(getUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth success.
  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber,
                                   /*fido_opt_in=*/false));
  WaitForCallbacks();

  // Mock payments opt out failure. Local pref should be unchanged.
  OptChange(AutofillClient::PaymentsRpcResult::kPermanentFailure, false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

// TODO(crbug.com/1409172): Extend the FIDOAuthOptChange tests to more
// use-cases.
TEST_F(CreditCardAccessManagerTest, FIDOAuthOptChange_OptOut) {
  credit_card_access_manager_->FIDOAuthOptChange(/*opt_in=*/false);
  ASSERT_TRUE(fido_authenticator_->IsOptOutCalled());
}

TEST_F(CreditCardAccessManagerTest, FIDOAuthOptChange_OptOut_OffTheRecord) {
  autofill_client_.set_is_off_the_record(true);
  credit_card_access_manager_->FIDOAuthOptChange(/*opt_in=*/false);
  ASSERT_FALSE(fido_authenticator_->IsOptOutCalled());
}

// TODO(crbug.com/1109296) Debug issues and re-enable this test on MacOS.
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
  credit_card_access_manager_->PrepareToFetchCreditCard();
  histogram_tester.ExpectTotalCount(preflight_call_metric, 1);

  // Calling PrepareToFetchCreditCard() without a prior preflight call should
  // have set |can_fetch_unmask_details_| to false to prevent further ones.
  EXPECT_FALSE(credit_card_access_manager_->can_fetch_unmask_details_);

  // Any subsequent calls should not make a RPC.
  credit_card_access_manager_->PrepareToFetchCreditCard();
  histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
}
#endif  // !BUILDFLAG(IS_APPLE)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

// Ensures that UnmaskAuthFlowEvents also log to a ".ServerCard" subhistogram
// when a masked server card is selected.
TEST_F(CreditCardAccessManagerTest,
       UnmaskAuthFlowEvent_AlsoLogsServerCardSubhistogram) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);
  base::HistogramTester histogram_tester;
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Cvc.ServerCard";

  credit_card_access_manager_->PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);

  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that |is_authentication_in_progress_| is set correctly.
TEST_F(CreditCardAccessManagerTest, AuthenticationInProgress) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card = personal_data().GetCreditCardByGUID(kTestGUID);

  EXPECT_FALSE(IsAuthenticationInProgress());

  credit_card_access_manager_->FetchCreditCard(card, accessor_->GetWeakPtr());
  EXPECT_TRUE(IsAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(AutofillClient::PaymentsRpcResult::kSuccess,
                                   kTestNumber));
  EXPECT_FALSE(IsAuthenticationInProgress());
}

// Ensures that the use of |unmasked_card_cache_| is set and logged correctly.
TEST_F(CreditCardAccessManagerTest, FetchCreditCardUsesUnmaskedCardCache) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false);
  CreditCard* unmasked_card = personal_data().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager_->CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true);
  CreditCard* masked_card = personal_data().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager_->FetchCreditCard(masked_card,
                                               accessor_->GetWeakPtr());
  histogram_tester.ExpectBucketCount("Autofill.UsedCachedServerCard", 1, 1);
  credit_card_access_manager_->FetchCreditCard(masked_card,
                                               accessor_->GetWeakPtr());
  histogram_tester.ExpectBucketCount("Autofill.UsedCachedServerCard", 2, 1);

  // Create a virtual card.
  CreditCard virtual_card = CreditCard();
  test::SetCreditCardInfo(&virtual_card, "Elvis Presley", kTestNumber,
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  virtual_card.set_record_type(CreditCard::VIRTUAL_CARD);
  credit_card_access_manager_->CacheUnmaskedCardInfo(virtual_card, kTestCvc16);

  // Mocks that user selects the virtual card option of the masked card.
  masked_card->set_record_type(CreditCard::VIRTUAL_CARD);
  credit_card_access_manager_->FetchCreditCard(masked_card,
                                               accessor_->GetWeakPtr());

  histogram_tester.ExpectBucketCount("Autofill.UsedCachedVirtualCard", 1, 1);
}

TEST_F(CreditCardAccessManagerTest, GetCachedUnmaskedCards) {
  // Assert that there are no cards cached initially.
  EXPECT_EQ(0U, credit_card_access_manager_->GetCachedUnmaskedCards().size());

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreateServerCard(kTestGUID2, kTestNumber2, /*masked=*/true, kTestServerId2);
  // Add a card to the cache.
  CreditCard* unmasked_card = personal_data().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager_->CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  // Verify that only the card added to the cache is returned.
  ASSERT_EQ(1U, credit_card_access_manager_->GetCachedUnmaskedCards().size());
  EXPECT_EQ(*unmasked_card,
            credit_card_access_manager_->GetCachedUnmaskedCards()[0]->card);
}

TEST_F(CreditCardAccessManagerTest, IsCardPresentInUnmaskedCache) {
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreateServerCard(kTestGUID2, kTestNumber2, /*masked=*/true, kTestServerId2);
  // Add a card to the cache.
  CreditCard* unmasked_card = personal_data().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager_->CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  // Verify that only one card is present in the cache.
  EXPECT_TRUE(credit_card_access_manager_->IsCardPresentInUnmaskedCache(
      *unmasked_card));
  EXPECT_FALSE(credit_card_access_manager_->IsCardPresentInUnmaskedCache(
      *personal_data().GetCreditCardByGUID(kTestGUID2)));
}

TEST_F(CreditCardAccessManagerTest, IsVirtualCardPresentInUnmaskedCache) {
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* unmasked_card = personal_data().GetCreditCardByGUID(kTestGUID);
  unmasked_card->set_record_type(CreditCard::VIRTUAL_CARD);

  // Add the virtual card to the cache.
  credit_card_access_manager_->CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  // Verify that the virtual card is present in the cache.
  EXPECT_TRUE(credit_card_access_manager_->IsCardPresentInUnmaskedCache(
      *unmasked_card));
}

TEST_F(CreditCardAccessManagerTest, RiskBasedVirtualCardUnmasking_Success) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with valid card information.
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.real_pan = "4111111111111111";
  response.dcvv = "321";
  response.expiration_month = test::NextMonth();
  response.expiration_year = test::NextYear();
  response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  // Expect the accessor receives the correct response.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(accessor_->number(), u"4111111111111111");
  EXPECT_EQ(accessor_->cvc(), u"321");
  EXPECT_EQ(accessor_->expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
  EXPECT_EQ(accessor_->expiry_year(), base::UTF8ToUTF16(test::NextYear()));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly. The card identifier in the virtual card case should
  // be the last four digits of the card number.
  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  absl::optional<absl::variant<FormDataImporter::CardGuid,
                               FormDataImporter::CardLastFourDigits>>
      card_identifier =
          autofill_client_.GetFormDataImporter()
              ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted();
  ASSERT_TRUE(card_identifier.has_value());
  ASSERT_TRUE(absl::holds_alternative<FormDataImporter::CardLastFourDigits>(
      card_identifier.value()));
  ASSERT_EQ(
      absl::get<FormDataImporter::CardLastFourDigits>(card_identifier.value())
          .value(),
      response.real_pan.substr(response.real_pan.size() - 4));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
}

#if !BUILDFLAG(IS_IOS)
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
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0);

  CreditCard card = test::GetCreditCard();
  credit_card_access_manager_->OnOtpAuthenticationComplete(
      CreditCardOtpAuthenticator::OtpAuthenticationResponse()
          .with_result(CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                           Result::kSuccess)
          .with_card(&card)
          .with_cvc(u"123"));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      autofill_client_.GetFormDataImporter()
          ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()
          .has_value());

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
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0);

  CreditCard card = test::GetCreditCard();
  credit_card_access_manager_->OnCvcAuthenticationComplete(
      CreditCardCvcAuthenticator::CvcAuthenticationResponse()
          .with_did_succeed(true)
          .with_card(&card)
          .with_cvc(u"123"));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      autofill_client_.GetFormDataImporter()
          ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()
          .has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  // TODO(crbug/1370329): Add metrics checks for Virtual Card CVC auth result.
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the correct authenticator when multiple
// challenge options are returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_OtpAndCvc) {
  base::HistogramTester histogram_tester;
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kCvc});

  for (size_t selected_index = 0; selected_index < challenge_options.size();
       selected_index++) {
    MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
        /*fido_authenticator_is_user_opted_in=*/false,
        /*is_user_verifiable=*/false, challenge_options, selected_index);

    switch (challenge_options[selected_index].type) {
      case CardUnmaskChallengeOptionType::kSmsOtp: {
        CreditCard card = test::GetCreditCard();
        credit_card_access_manager_->OnOtpAuthenticationComplete(
            CreditCardOtpAuthenticator::OtpAuthenticationResponse()
                .with_result(CreditCardOtpAuthenticator::
                                 OtpAuthenticationResponse::Result::kSuccess)
                .with_card(&card)
                .with_cvc(u"123"));
        break;
      }
      case CardUnmaskChallengeOptionType::kCvc: {
        CreditCard card = test::GetCreditCard();
        credit_card_access_manager_->OnCvcAuthenticationComplete(
            CreditCardCvcAuthenticator::CvcAuthenticationResponse()
                .with_did_succeed(true)
                .with_card(&card)
                .with_cvc(u"123"));
        break;
      }
      case CardUnmaskChallengeOptionType::kEmailOtp:
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED();
        break;
    }
  }

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      autofill_client_.GetFormDataImporter()
          ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()
          .has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 2);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
  // TODO(crbug/1370329): Add metrics checks for Virtual Card CVC auth result.
}

TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_CreditCardAccessManagerReset_TriggersOtpAuthenticatorResetOnFlowCancelled) {
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0);

  // This check already happens in
  // MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(), but double
  // checking here helps show this test works correctly.
  EXPECT_TRUE(otp_authenticator_->on_challenge_option_selected_invoked());

  credit_card_access_manager_->OnVirtualCardUnmaskCancelled();
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// only the FIDO challenge options is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  credit_card_access_manager_->is_user_verifiable_ = true;
  fido_authenticator_->set_is_user_opted_in(true);

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with information regarding FIDO auth.
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  response.fido_request_options = GetTestRequestOptions();
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator_->authenticate_invoked());
  EXPECT_EQ(fido_authenticator_->card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(fido_authenticator_->card().record_type(),
            CreditCard::VIRTUAL_CARD);
  ASSERT_TRUE(fido_authenticator_->context_token().has_value());
  EXPECT_EQ(fido_authenticator_->context_token().value(), "fake_context_token");

  // Mock FIDO authentication completed.
  CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response;
  fido_response.did_succeed = true;
  CreditCard card = test::GetCreditCard();
  fido_response.card = &card;
  fido_response.cvc = u"123";
  credit_card_access_manager_->OnFIDOAuthenticationComplete(fido_response);

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
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  credit_card_access_manager_->is_user_verifiable_ = true;
  fido_authenticator_->set_is_user_opted_in(true);

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with information regarding both FIDO and OTP auth.
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp})[0];
  response.card_unmask_challenge_options.emplace_back(challenge_option);
  response.fido_request_options = GetTestRequestOptions();
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  DCHECK(fido_authenticator_);
  ASSERT_TRUE(fido_authenticator_->authenticate_invoked());
  EXPECT_EQ(fido_authenticator_->card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(fido_authenticator_->card().record_type(),
            CreditCard::VIRTUAL_CARD);
  ASSERT_TRUE(fido_authenticator_->context_token().has_value());
  EXPECT_EQ(fido_authenticator_->context_token().value(), "fake_context_token");

  // Mock FIDO authentication completed.
  CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response;
  fido_response.did_succeed = true;
  CreditCard card = test::GetCreditCard();
  fido_response.card = &card;
  fido_response.cvc = u"123";
  credit_card_access_manager_->OnFIDOAuthenticationComplete(fido_response);

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
      /*is_user_verifiable=*/true, challenge_options, /*selected_index=*/0);

  CreditCardOtpAuthenticator::OtpAuthenticationResponse otp_response;
  otp_response.result =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  CreditCard card = test::GetCreditCard();
  otp_response.card = &card;
  otp_response.cvc = u"123";
  credit_card_access_manager_->OnOtpAuthenticationComplete(otp_response);

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
      /*is_user_verifiable=*/true, challenge_options, /*selected_index=*/0);

  CreditCardOtpAuthenticator::OtpAuthenticationResponse otp_response;
  otp_response.result =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  CreditCard card = test::GetCreditCard();
  otp_response.card = &card;
  otp_response.cvc = u"123";
  credit_card_access_manager_->OnOtpAuthenticationComplete(otp_response);

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
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  credit_card_access_manager_->is_user_verifiable_ = true;
  fido_authenticator_->set_is_user_opted_in(false);

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with information regarding FIDO auth.
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  response.fido_request_options = GetTestRequestOptions();
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kSuccess, response);

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
  EXPECT_FALSE(fido_authenticator_->authenticate_invoked());

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
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
  // |is_user_verifiable_| related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  credit_card_access_manager_->is_user_verifiable_ = true;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator_->set_is_user_opted_in(true);
#endif

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with no challenge options.
  payments::PaymentsClient::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, response);

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator_->authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationError, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is virtual card retrieval error.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_VirtualCardRetrievalError) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  credit_card_access_manager_->is_user_verifiable_ = true;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator_->set_is_user_opted_in(true);
#endif

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock server response with no challenge options.
  payments::PaymentsClient::UnmaskResponseDetails response;
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
      response);

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
  EXPECT_TRUE(autofill_client_.virtual_card_error_dialog_shown());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator_->authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_MerchantOptedOut) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  AutofillErrorDialogContext autofill_error_dialog_context;
  autofill_error_dialog_context.server_returned_title =
      "test_server_returned_title";
  autofill_error_dialog_context.server_returned_description =
      "test_server_returned_description";

  payments::PaymentsClient::UnmaskResponseDetails response;
  response.autofill_error_dialog_context = autofill_error_dialog_context;
  credit_card_access_manager_->OnVirtualCardUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
      response);

  EXPECT_TRUE(autofill_client_.virtual_card_error_dialog_shown());
  const AutofillErrorDialogContext& displayed_error_dialog_context =
      autofill_client_.autofill_error_dialog_context();
  EXPECT_EQ(*displayed_error_dialog_context.server_returned_title,
            *autofill_error_dialog_context.server_returned_title);
  EXPECT_EQ(*displayed_error_dialog_context.server_returned_description,
            *autofill_error_dialog_context.server_returned_description);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_FlowCancelled) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
  // TODO(crbug.com/1249665): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  credit_card_access_manager_->is_user_verifiable_ = true;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator_->set_is_user_opted_in(true);
#endif

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());

  // Mock that the flow was cancelled by the user.
  credit_card_access_manager_->OnVirtualCardUnmaskCancelled();

  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator_->authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
}

// Test that the CreditCardAccessManager's destructor resets the identifier of
// the card that had no interactive authentication flows completed in the
// associated FormDataImporter.
TEST_F(CreditCardAccessManagerTest, DestructorResetsCardIdentifier) {
  auto* form_data_importer = autofill_client_.GetFormDataImporter();
  form_data_importer
      ->SetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted(
          FormDataImporter::CardGuid("TestGuid"));
  EXPECT_TRUE(
      form_data_importer
          ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()
          .has_value());
  autofill_driver_.reset();
  EXPECT_FALSE(
      form_data_importer
          ->GetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted()
          .has_value());
}

// Params of the CreditCardAccessManagerCardMetadataTest:
// -- bool card_name_available;
// -- bool card_art_available;
// -- bool metadata_enabled;
class CreditCardAccessManagerCardMetadataTest
    : public CreditCardAccessManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  CreditCardAccessManagerCardMetadataTest() = default;
  ~CreditCardAccessManagerCardMetadataTest() override = default;

  bool CardNameAvailable() { return std::get<0>(GetParam()); }
  bool CardArtAvailable() { return std::get<1>(GetParam()); }
  bool MetadataEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerCardMetadataTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(CreditCardAccessManagerCardMetadataTest, MetadataSignal) {
  base::test::ScopedFeatureList metadata_feature_list;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card = personal_data().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::VIRTUAL_CARD);
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
    virtual_card->set_product_description(u"Fake card product name");
  }
  if (CardArtAvailable()) {
    virtual_card->set_card_art_url(GURL("https://www.example.com"));
  }

  credit_card_access_manager_->FetchCreditCard(virtual_card,
                                               accessor_->GetWeakPtr());

  // Ensures the UnmaskRequestDetails is populated with correct contents.
  EXPECT_TRUE(payments_client_->unmask_request()->context_token.empty());
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  EXPECT_TRUE(payments_client_->unmask_request()
                  ->last_committed_primary_main_frame_origin.has_value());
  std::vector<ClientBehaviorConstants> signals =
      payments_client_->unmask_request()->client_behavior_signals;
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
