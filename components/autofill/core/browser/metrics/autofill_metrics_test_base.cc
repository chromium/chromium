// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"

#include <memory>

#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif


namespace autofill::autofill_metrics {

namespace {

using ::testing::Invoke;
using ::testing::NiceMock;

void SetProfileTestData(AutofillProfile* profile) {
  test::SetProfileInfo(profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile->set_guid(kTestProfileId);
}
}  // namespace

MockPaymentsAutofillClient::MockPaymentsAutofillClient(AutofillClient* client)
    : payments::TestPaymentsAutofillClient(client) {}

MockPaymentsAutofillClient::~MockPaymentsAutofillClient() = default;

MockCreditCardAccessManager::MockCreditCardAccessManager(
    BrowserAutofillManager* bam)
    : CreditCardAccessManager(bam) {
  ON_CALL(*this, FetchCreditCard)
      .WillByDefault(
          [this](const CreditCard* card, OnCreditCardFetchedCallback cb) {
            CreditCardAccessManager::FetchCreditCard(card, std::move(cb));
          });
}

MockCreditCardAccessManager::~MockCreditCardAccessManager() = default;

MockAutofillDriver::MockAutofillDriver(TestAutofillClient* client)
    : TestAutofillDriver(client) {
  ON_CALL(*this, ApplyFormAction)
      .WillByDefault(
          [this](mojom::FormActionType action_type,
                 mojom::ActionPersistence action_persistence,
                 base::span<const FormFieldData> data,
                 const url::Origin& triggered_origin,
                 const base::flat_map<FieldGlobalId, FieldType>& field_type_map,
                 const Section& section_for_clear_form_on_ios)
              -> base::flat_set<FieldGlobalId> {
            return TestAutofillDriver::ApplyFormAction(
                action_type, action_persistence, data, triggered_origin,
                field_type_map, section_for_clear_form_on_ios);
          });
}

MockAutofillDriver::~MockAutofillDriver() = default;

TestBrowserAutofillManager::TestBrowserAutofillManager(AutofillDriver* driver)
    : autofill::TestBrowserAutofillManager(driver) {
  test_api(*this).SetExternalDelegate(
      std::make_unique<TestAutofillExternalDelegate>(this));
  test_api(*this).set_credit_card_access_manager(
      std::make_unique<NiceMock<MockCreditCardAccessManager>>(this));
}

void TestBrowserAutofillManager::Reset() {
  autofill::TestBrowserAutofillManager::Reset();
  test_api(*this).set_credit_card_access_manager(
      std::make_unique<NiceMock<MockCreditCardAccessManager>>(this));
}

AutofillMetricsBaseTest::AutofillMetricsBaseTest() {
  scoped_features_.InitWithFeatures(
      {features::kAutofillEnableLoyaltyCardsFilling,
       features::kAutofillEnableEmailOrLoyaltyCardsFilling},
      {});
}

AutofillMetricsBaseTest::~AutofillMetricsBaseTest() = default;

void AutofillMetricsBaseTest::InitAutofillClient() {
  WithTestAutofillClientDriverManager::InitAutofillClient();
  autofill_client().set_payments_autofill_client(
      std::make_unique<NiceMock<MockPaymentsAutofillClient>>(
          &autofill_client()));
}

void AutofillMetricsBaseTest::SetUpHelper() {
  // Advance the mock clock to a fixed, arbitrary, somewhat recent date.
  base::Time year2020;
  ASSERT_TRUE(base::Time::FromString("01/01/20", &year2020));
  task_environment_.FastForwardBy(year2020 - base::Time::Now());

  InitAutofillClient();

  test_api(personal_data().address_data_manager())
      .set_auto_accept_address_imports(true);
  personal_data().SetSyncServiceForTest(&sync_service_);

  auto payments_network_interface =
      std::make_unique<payments::TestPaymentsNetworkInterface>(
          autofill_client().GetURLLoaderFactory(),
          autofill_client().GetIdentityManager(), &personal_data());
  payments_autofill_client().set_payments_network_interface(
      std::move(payments_network_interface));
  test_api(*autofill_client().GetFormDataImporter())
      .set_credit_card_save_manager(
          std::make_unique<TestCreditCardSaveManager>(&autofill_client()));
  payments_autofill_client().set_autofill_offer_manager(
      std::make_unique<AutofillOfferManager>(&paydm()));

  CreateAutofillDriver();
  autofill_driver().SetLocalFrameToken(test::MakeLocalFrameToken());

#if !BUILDFLAG(IS_IOS)
  test_api(*autofill_manager().GetCreditCardAccessManager())
      .set_fido_authenticator(std::make_unique<TestCreditCardFidoAuthenticator>(
          &autofill_driver(), &autofill_client()));
#endif

  // Initialize the TestPersonalDataManager with some default data.
  CreateTestAutofillProfiles();

  // Mandatory re-auth is required for credit card autofill on automotive, so
  // the authenticator response needs to be properly mocked.
#if BUILDFLAG(IS_ANDROID)
  payments_autofill_client()
      .SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif
}

void AutofillMetricsBaseTest::TearDownHelper() {
  test_ukm_recorder().Purge();
  DestroyAutofillClient();
}

void AutofillMetricsBaseTest::PurgeUKM() {
  autofill_client().GetAutofillDriverFactory().Reset(autofill_driver());
  test_ukm_recorder().Purge();
  autofill_driver().InitializeUKMSources();
}

void AutofillMetricsBaseTest::CreateAmbiguousProfiles() {
  personal_data().test_address_data_manager().ClearProfiles();
  CreateTestAutofillProfiles();

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "John", "Decca", "Public", "john@gmail.com",
                       "Company", "123 Main St.", "unit 7", "Springfield",
                       "Texas", "79401", "US", "2345678901");
  profile.set_guid("00000000-0000-0000-0000-000000000003");
  personal_data().address_data_manager().AddProfile(profile);
}

void AutofillMetricsBaseTest::RecreateProfile() {
  personal_data().test_address_data_manager().ClearProfiles();
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  SetProfileTestData(&profile);
  personal_data().address_data_manager().AddProfile(profile);
}

void AutofillMetricsBaseTest::SetFidoEligibility(bool is_verifiable) {
  CreditCardAccessManager& access_manager =
      *autofill_manager().GetCreditCardAccessManager();
#if !BUILDFLAG(IS_IOS)
  static_cast<TestCreditCardFidoAuthenticator*>(
      access_manager.GetOrCreateFidoAuthenticator())
      ->SetUserVerifiable(is_verifiable);
#endif
  static_cast<payments::TestPaymentsNetworkInterface*>(
      payments_autofill_client().GetPaymentsNetworkInterface())
      ->AllowFidoRegistration(true);
  test_api(access_manager).set_is_authentication_in_progress(false);
  test_api(access_manager).set_can_fetch_unmask_details(true);
  test_api(access_manager).set_is_user_verifiable(std::nullopt);
}

void AutofillMetricsBaseTest::OnDidGetRealPan(
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    const std::string& real_pan,
    bool is_virtual_card) {
  // FPAN risk-based authentication is implemented in some platforms. If
  // risk-based authentication is available, simulate a CVC authentication
  // challenge is required.
  if (autofill_manager()
          .GetCreditCardAccessManager()
          ->IsMaskedServerCardRiskBasedAuthAvailable()) {
    autofill_manager()
        .GetCreditCardAccessManager()
        ->OnRiskBasedAuthenticationResponseReceived(
            CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
                .with_result(CreditCardRiskBasedAuthenticator::
                                 RiskBasedAuthenticationResponse::Result::
                                     kAuthenticationRequired)
                .with_context_token("fake context token"));
  }

  payments::FullCardRequest* full_card_request =
      autofill_manager()
          .client()
          .GetPaymentsAutofillClient()
          ->GetCvcAuthenticator()
          .full_card_request_.get();
  DCHECK(full_card_request);

  // Fake user response.
  payments::FullCardRequest::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_request->OnUnmaskPromptAccepted(details);

  payments::UnmaskResponseDetails response;
  response.card_type =
      is_virtual_card
          ? payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard
          : payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard;
  full_card_request->OnDidGetRealPan(result, response.with_real_pan(real_pan));
}

void AutofillMetricsBaseTest::OnDidGetRealPanWithNonHttpOkResponse() {
  payments::FullCardRequest* full_card_request =
      autofill_manager()
          .client()
          .GetPaymentsAutofillClient()
          ->GetCvcAuthenticator()
          .full_card_request_.get();
  DCHECK(full_card_request);

  // Fake user response.
  payments::FullCardRequest::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_request->OnUnmaskPromptAccepted(details);

  payments::UnmaskResponseDetails response;
  // Don't set |response.card_type|, so that it stays as kUnknown.
  full_card_request->OnDidGetRealPan(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      response);
}

CreditCard AutofillMetricsBaseTest::BuildCard(const std::u16string& real_pan,
                                              bool is_virtual_card) {
  CreditCard card = test::WithCvc(test::GetMaskedServerCard());
  card.set_record_type(is_virtual_card
                           ? CreditCard::RecordType::kVirtualCard
                           : CreditCard::RecordType::kMaskedServerCard);
  card.SetNumber(real_pan);
  return card;
}

void AutofillMetricsBaseTest::RecreateCreditCards(
    bool include_local_credit_card,
    bool include_masked_server_credit_card,
    bool masked_card_is_enrolled_for_virtual_card,
    bool include_cvc_in_cards) {
  personal_data().test_payments_data_manager().ClearCreditCards();
  CreateCreditCards(
      include_local_credit_card, include_masked_server_credit_card,
      masked_card_is_enrolled_for_virtual_card, include_cvc_in_cards);
}

void AutofillMetricsBaseTest::CreateCreditCards(
    bool include_local_credit_card,
    bool include_masked_server_credit_card,
    bool masked_card_is_enrolled_for_virtual_card,
    bool include_cvc_in_cards) {
  if (include_local_credit_card) {
    CreditCard local_credit_card = test::GetCreditCard();
    local_credit_card.set_guid("10000000-0000-0000-0000-000000000001");
    if (include_cvc_in_cards) {
#if !BUILDFLAG(IS_IOS)
      local_credit_card.set_cvc(u"123");
#endif
    }
    paydm().AddCreditCard(local_credit_card);
  }
  if (include_masked_server_credit_card) {
    CreditCard masked_server_credit_card(
        CreditCard::RecordType::kMaskedServerCard, "server_id_1");
    masked_server_credit_card.set_guid("10000000-0000-0000-0000-000000000002");
    masked_server_credit_card.set_instrument_id(1);
    masked_server_credit_card.SetNetworkForMaskedCard(kDiscoverCard);
    masked_server_credit_card.SetNumber(u"9424");
    if (masked_card_is_enrolled_for_virtual_card) {
      masked_server_credit_card.set_virtual_card_enrollment_state(
          CreditCard::VirtualCardEnrollmentState::kEnrolled);
    }
    if (include_cvc_in_cards) {
#if !BUILDFLAG(IS_IOS)
      masked_server_credit_card.set_cvc(u"123");
#endif
    }
    personal_data().test_payments_data_manager().AddServerCreditCard(
        masked_server_credit_card);
  }
}

void AutofillMetricsBaseTest::CreateLocalAndDuplicateServerCreditCard() {
  // Local credit card creation.
  CreditCard local_credit_card = test::GetCreditCard();
  local_credit_card.SetNumber(u"5454545454545454" /* Mastercard */);
  std::string local_card_guid(kTestDuplicateLocalCardId);
  local_credit_card.set_guid(local_card_guid);
  paydm().AddCreditCard(local_credit_card);

  // Duplicate masked server card with same card information as local card.
  CreditCard masked_server_credit_card = test::GetCreditCard();
  masked_server_credit_card.set_record_type(
      CreditCard::RecordType::kMaskedServerCard);
  masked_server_credit_card.set_server_id("server_id_2");
  std::string server_card_guid(kTestDuplicateMaskedCardId);
  masked_server_credit_card.set_guid(server_card_guid);
  masked_server_credit_card.set_instrument_id(1);
  masked_server_credit_card.SetNetworkForMaskedCard(kMasterCard);
  masked_server_credit_card.SetNumber(u"5454");
  personal_data().test_payments_data_manager().AddServerCreditCard(
      masked_server_credit_card);
}

void AutofillMetricsBaseTest::AddMaskedServerCreditCardWithOffer(
    std::string guid,
    std::string offer_reward_amount,
    GURL url,
    int64_t id,
    bool offer_expired) {
  CreditCard masked_server_credit_card(
      CreditCard::RecordType::kMaskedServerCard, "server_id_offer");
  masked_server_credit_card.set_guid(guid);
  masked_server_credit_card.set_instrument_id(id);
  masked_server_credit_card.SetNetworkForMaskedCard(kDiscoverCard);
  masked_server_credit_card.SetNumber(u"9424");
  personal_data().test_payments_data_manager().AddServerCreditCard(
      masked_server_credit_card);

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
  personal_data().test_payments_data_manager().AddAutofillOfferData(offer_data);
}

void AutofillMetricsBaseTest::CreateTestAutofillProfiles() {
  AutofillProfile profile1(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile1, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile1.set_guid(kTestProfileId);
  personal_data().address_data_manager().AddProfile(profile1);

  AutofillProfile profile2(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                       "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                       "Lubbock", "Texas", "79401", "US", "2345678901");
  profile2.set_guid(kTestProfile2Id);
  personal_data().address_data_manager().AddProfile(profile2);
}

}  // namespace autofill::autofill_metrics
