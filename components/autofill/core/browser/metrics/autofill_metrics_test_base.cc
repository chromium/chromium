// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"

#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif

namespace autofill::autofill_metrics {

namespace {
void SetProfileTestData(AutofillProfile* profile) {
  test::SetProfileInfo(profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile->set_guid(kTestProfileId);
}
}  // namespace

MockAutofillClient::MockAutofillClient() = default;
MockAutofillClient::~MockAutofillClient() = default;

AutofillMetricsBaseTest::AutofillMetricsBaseTest(bool is_in_any_main_frame)
    : is_in_any_main_frame_(is_in_any_main_frame) {
  scoped_feature_list_async_parse_form_.InitAndEnableFeature(
      features::kAutofillParseAsync);
}

AutofillMetricsBaseTest::~AutofillMetricsBaseTest() = default;

void AutofillMetricsBaseTest::SetUpHelper() {
  autofill_client_ = std::make_unique<MockAutofillClient>();
  autofill_client_->SetPrefs(test::PrefServiceForTesting());
  test_ukm_recorder_ = autofill_client_->GetTestUkmRecorder();

  personal_data().set_auto_accept_address_imports_for_testing(true);
  personal_data().SetPrefService(autofill_client_->GetPrefs());
  personal_data().OnSyncServiceInitialized(&sync_service_);

  autofill_driver_ = std::make_unique<TestAutofillDriver>();
  autofill_driver_->SetIsInAnyMainFrame(is_in_any_main_frame_);

  payments::TestPaymentsClient* payments_client =
      new payments::TestPaymentsClient(autofill_client_->GetURLLoaderFactory(),
                                       autofill_client_->GetIdentityManager(),
                                       &personal_data());
  autofill_client_->set_test_payments_client(
      std::unique_ptr<payments::TestPaymentsClient>(payments_client));
  auto credit_card_save_manager = std::make_unique<TestCreditCardSaveManager>(
      autofill_driver_.get(), autofill_client_.get(), payments_client,
      &personal_data());
  autofill_client_->set_test_form_data_importer(
      std::make_unique<TestFormDataImporter>(
          autofill_client_.get(), payments_client,
          std::move(credit_card_save_manager),
          /*iban_save_manager=*/nullptr, &personal_data(), "en-US"));
  autofill_client_->set_autofill_offer_manager(
      std::make_unique<AutofillOfferManager>(
          &personal_data(), /*coupon_service_delegate=*/nullptr));

  auto browser_autofill_manager = std::make_unique<TestBrowserAutofillManager>(
      autofill_driver_.get(), autofill_client_.get());
  autofill_driver_->set_autofill_manager(std::move(browser_autofill_manager));

  auto external_delegate = std::make_unique<AutofillExternalDelegate>(
      &autofill_manager(), autofill_driver_.get());
  external_delegate_ = external_delegate.get();
  autofill_manager().SetExternalDelegateForTest(std::move(external_delegate));

#if !BUILDFLAG(IS_IOS)
  autofill_manager()
      .GetCreditCardAccessManager()
      ->set_fido_authenticator_for_testing(
          std::make_unique<TestCreditCardFidoAuthenticator>(
              autofill_driver_.get(), autofill_client_.get()));
#endif

  // Initialize the TestPersonalDataManager with some default data.
  CreateTestAutofillProfiles();
}

void AutofillMetricsBaseTest::TearDownHelper() {
  test_ukm_recorder_->Purge();
  autofill_driver_.reset();
  autofill_client_.reset();
}

void AutofillMetricsBaseTest::PurgeUKM() {
  autofill_manager().Reset();
  test_ukm_recorder_->Purge();
  autofill_client_->InitializeUKMSources();
}

void AutofillMetricsBaseTest::CreateAmbiguousProfiles() {
  personal_data().ClearProfiles();
  CreateTestAutofillProfiles();

  AutofillProfile profile;
  test::SetProfileInfo(&profile, "John", "Decca", "Public", "john@gmail.com",
                       "Company", "123 Main St.", "unit 7", "Springfield",
                       "Texas", "79401", "US", "2345678901");
  profile.set_guid("00000000-0000-0000-0000-000000000003");
  personal_data().AddProfile(profile);
  personal_data().Refresh();
}

void AutofillMetricsBaseTest::RecreateProfile(bool is_server) {
  personal_data().ClearProfiles();

  if (is_server) {
    AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "server_id");
    SetProfileTestData(&profile);
    personal_data().AddProfile(profile);
  } else {
    AutofillProfile profile;
    SetProfileTestData(&profile);
    personal_data().AddProfile(profile);
  }

  personal_data().Refresh();
}

void AutofillMetricsBaseTest::SetFidoEligibility(bool is_verifiable) {
  CreditCardAccessManager* access_manager =
      autofill_manager().GetCreditCardAccessManager();
#if !BUILDFLAG(IS_IOS)
  static_cast<TestCreditCardFidoAuthenticator*>(
      access_manager->GetOrCreateFidoAuthenticator())
      ->SetUserVerifiable(is_verifiable);
#endif
  static_cast<payments::TestPaymentsClient*>(
      autofill_client_->GetPaymentsClient())
      ->AllowFidoRegistration(true);
  access_manager->is_authentication_in_progress_ = false;
  access_manager->can_fetch_unmask_details_ = true;
  access_manager->is_user_verifiable_ = absl::nullopt;
}

void AutofillMetricsBaseTest::OnDidGetRealPan(
    AutofillClient::PaymentsRpcResult result,
    const std::string& real_pan,
    bool is_virtual_card) {
  payments::FullCardRequest* full_card_request = autofill_manager()
                                                     .client()
                                                     ->GetCvcAuthenticator()
                                                     ->full_card_request_.get();
  DCHECK(full_card_request);

  // Fake user response.
  payments::FullCardRequest::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_request->OnUnmaskPromptAccepted(details);

  payments::PaymentsClient::UnmaskResponseDetails response;
  response.card_type = is_virtual_card
                           ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                           : AutofillClient::PaymentsRpcCardType::kServerCard;
  full_card_request->OnDidGetRealPan(result, response.with_real_pan(real_pan));
}

void AutofillMetricsBaseTest::OnDidGetRealPanWithNonHttpOkResponse() {
  payments::FullCardRequest* full_card_request = autofill_manager()
                                                     .client()
                                                     ->GetCvcAuthenticator()
                                                     ->full_card_request_.get();
  DCHECK(full_card_request);

  // Fake user response.
  payments::FullCardRequest::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_request->OnUnmaskPromptAccepted(details);

  payments::PaymentsClient::UnmaskResponseDetails response;
  // Don't set |response.card_type|, so that it stays as kUnknown.
  full_card_request->OnDidGetRealPan(
      AutofillClient::PaymentsRpcResult::kPermanentFailure, response);
}

void AutofillMetricsBaseTest::OnCreditCardFetchingSuccessful(
    const std::u16string& real_pan,
    bool is_virtual_card) {
  credit_card_.set_record_type(
      is_virtual_card ? CreditCard::RecordType::VIRTUAL_CARD
                      : CreditCard::RecordType::MASKED_SERVER_CARD);
  credit_card_.SetNumber(real_pan);

  autofill_manager().OnCreditCardFetchedForTest(CreditCardFetchResult::kSuccess,
                                                &credit_card_, u"123");
}

void AutofillMetricsBaseTest::OnCreditCardFetchingFailed() {
  autofill_manager().OnCreditCardFetchedForTest(
      CreditCardFetchResult::kPermanentError, nullptr, u"");
}

void AutofillMetricsBaseTest::RecreateCreditCards(
    bool include_local_credit_card,
    bool include_masked_server_credit_card,
    bool include_full_server_credit_card,
    bool masked_card_is_enrolled_for_virtual_card) {
  personal_data().ClearCreditCards();
  if (include_local_credit_card) {
    CreditCard local_credit_card = test::GetCreditCard();
    local_credit_card.set_guid("10000000-0000-0000-0000-000000000001");
    personal_data().AddCreditCard(local_credit_card);
  }
  if (include_masked_server_credit_card) {
    CreditCard masked_server_credit_card(CreditCard::MASKED_SERVER_CARD,
                                         "server_id_1");
    masked_server_credit_card.set_guid("10000000-0000-0000-0000-000000000002");
    masked_server_credit_card.set_instrument_id(1);
    masked_server_credit_card.SetNetworkForMaskedCard(kDiscoverCard);
    masked_server_credit_card.SetNumber(u"9424");
    if (masked_card_is_enrolled_for_virtual_card) {
      masked_server_credit_card.set_virtual_card_enrollment_state(
          CreditCard::ENROLLED);
    }
    personal_data().AddServerCreditCard(masked_server_credit_card);
  }
  if (include_full_server_credit_card) {
    CreditCard full_server_credit_card(CreditCard::FULL_SERVER_CARD,
                                       "server_id_2");
    full_server_credit_card.set_guid("10000000-0000-0000-0000-000000000003");
    full_server_credit_card.set_instrument_id(2);
    personal_data().AddFullServerCreditCard(full_server_credit_card);
  }
  personal_data().Refresh();
}

std::string AutofillMetricsBaseTest::CreateLocalMasterCard(
    bool clear_existing_cards) {
  if (clear_existing_cards) {
    personal_data().ClearCreditCards();
  }
  std::string guid("10000000-0000-0000-0000-000000000003");
  CreditCard local_credit_card = test::GetCreditCard();
  local_credit_card.SetNumber(u"5454545454545454" /* Mastercard */);
  local_credit_card.set_guid(guid);
  personal_data().AddCreditCard(local_credit_card);
  return guid;
}

std::vector<std::string>
AutofillMetricsBaseTest::CreateLocalAndDuplicateServerCreditCard() {
  personal_data().ClearCreditCards();

  // Local credit card creation.
  CreditCard local_credit_card = test::GetCreditCard();
  std::string local_card_guid("10000000-0000-0000-0000-000000000001");
  local_credit_card.set_guid(local_card_guid);
  personal_data().AddCreditCard(local_credit_card);

  // Duplicate masked server card with same card information as local card.
  CreditCard masked_server_credit_card = test::GetCreditCard();
  masked_server_credit_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
  masked_server_credit_card.set_server_id("server_id_2");
  std::string server_card_guid("10000000-0000-0000-0000-000000000002");
  masked_server_credit_card.set_guid(server_card_guid);
  masked_server_credit_card.set_instrument_id(1);
  masked_server_credit_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_credit_card.SetNumber(u"1111");
  personal_data().AddServerCreditCard(masked_server_credit_card);

  personal_data().Refresh();
  return {local_card_guid, server_card_guid};
}

void AutofillMetricsBaseTest::AddMaskedServerCreditCardWithOffer(
    std::string guid,
    std::string offer_reward_amount,
    GURL url,
    int64_t id,
    bool offer_expired) {
  CreditCard masked_server_credit_card(CreditCard::MASKED_SERVER_CARD,
                                       "server_id_offer");
  masked_server_credit_card.set_guid(guid);
  masked_server_credit_card.set_instrument_id(id);
  masked_server_credit_card.SetNetworkForMaskedCard(kDiscoverCard);
  masked_server_credit_card.SetNumber(u"9424");
  personal_data().AddServerCreditCard(masked_server_credit_card);

  int64_t offer_id = id;
  base::Time expiry = offer_expired ? AutofillClock::Now() - base::Days(2)
                                    : AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins = {GURL{url}};
  GURL offer_details_url = GURL(url);
  DisplayStrings display_strings;
  display_strings.value_prop_text = "Get 5% off your purchase";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Check out with this card to activate";
  std::vector<int64_t> eligible_instrument_id = {
      masked_server_credit_card.instrument_id()};

  AutofillOfferData offer_data = AutofillOfferData::GPayCardLinkedOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      eligible_instrument_id, offer_reward_amount);
  personal_data().AddAutofillOfferData(offer_data);
  personal_data().Refresh();
}

void AutofillMetricsBaseTest::CreateTestAutofillProfiles() {
  AutofillProfile profile1;
  test::SetProfileInfo(&profile1, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile1.set_guid(kTestProfileId);
  personal_data().AddProfile(profile1);

  AutofillProfile profile2;
  test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                       "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                       "Lubbock", "Texas", "79401", "US", "2345678901");
  profile2.set_guid("00000000-0000-0000-0000-000000000002");
  personal_data().AddProfile(profile2);
}

}  // namespace autofill::autofill_metrics
