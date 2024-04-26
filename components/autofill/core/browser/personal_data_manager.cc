// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill {

PersonalDataManager::PersonalDataManager(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    PrefService* pref_service,
    PrefService* local_state,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    syncer::SyncService* sync_service,
    StrikeDatabaseBase* strike_database,
    AutofillImageFetcherBase* image_fetcher,
    std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler,
    std::string app_locale,
    std::string variations_country_code)
    : pref_service_(pref_service),
      app_locale_(std::move(app_locale)),
      history_service_(history_service) {
  address_data_manager_ = std::make_unique<AddressDataManager>(
      profile_database, pref_service, local_state, sync_service,
      identity_manager, strike_database,
      GeoIpCountryCode(variations_country_code), app_locale_);
  payments_data_manager_ = std::make_unique<PaymentsDataManager>(
      profile_database, account_database, image_fetcher,
      std::move(shared_storage_handler), pref_service, sync_service,
      identity_manager, GeoIpCountryCode(std::move(variations_country_code)),
      app_locale_);
  address_data_manager_observation_.Observe(address_data_manager_.get());
  payments_data_manager_observation_.Observe(payments_data_manager_.get());

  // Listen for URL deletions from browsing history.
  if (history_service_) {
    history_service_observation_.Observe(history_service_.get());
  }

  // WebDataService may not be available in tests.
  if (!profile_database) {
    return;
  }

  Refresh();

  AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  // Potentially import profiles for testing. `Init()` is called whenever the
  // corresponding Chrome profile is created. This is either during start-up or
  // when the Chrome profile is changed.
  MaybeImportDataForManualTesting(weak_factory_.GetWeakPtr());
}

PersonalDataManager::~PersonalDataManager() = default;

void PersonalDataManager::Shutdown() {
  if (history_service_)
    history_service_observation_.Reset();
  history_service_ = nullptr;

  address_data_manager_observation_.Reset();
  payments_data_manager_observation_.Reset();

  // The following members register observers, which needs to be unregistered
  // before the dependent service's `Shutdown()`.
  address_data_manager_.reset();
  payments_data_manager_.reset();
}

void PersonalDataManager::OnAddressDataChanged() {
  NotifyPersonalDataObserver();
}

void PersonalDataManager::OnPaymentsDataChanged() {
  NotifyPersonalDataObserver();
}

void PersonalDataManager::OnHistoryDeletions(
    history::HistoryService* /* history_service */,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration() && deletion_info.IsAllHistory()) {
    AutofillCrowdsourcingManager::ClearUploadHistory(pref_service_);
  }
  address_data_manager_->OnHistoryDeletions(deletion_info);
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  address_data_manager_->AddProfile(profile);
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  address_data_manager_->UpdateProfile(profile);
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  payments_data_manager_->AddCreditCard(credit_card);
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  payments_data_manager_->UpdateCreditCard(credit_card);
}

void PersonalDataManager::ClearAllServerDataForTesting() {
  payments_data_manager_->ClearAllServerDataForTesting();  // IN-TEST
}

void PersonalDataManager::AddServerCreditCardForTest(
    std::unique_ptr<CreditCard> credit_card) {
  payments_data_manager_->AddServerCreditCardForTest(
      std::move(credit_card));  // IN-TEST
}

bool PersonalDataManager::IsUsingAccountStorageForServerDataForTest() const {
  return payments_data_manager_->IsUsingAccountStorageForServerData();
}

void PersonalDataManager::SetSyncServiceForTest(
    syncer::SyncService* sync_service) {
  address_data_manager_->SetSyncServiceForTest(sync_service);   // IN-TEST
  payments_data_manager_->SetSyncServiceForTest(sync_service);  // IN-TEST
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (!payments_data_manager_->RemoveByGUID(guid)) {
    address_data_manager_->RemoveProfile(guid);
  }
}

CreditCard* PersonalDataManager::GetCreditCardByGUID(const std::string& guid) {
  return payments_data_manager_->GetCreditCardByGUID(guid);
}

CreditCard* PersonalDataManager::GetCreditCardByNumber(
    const std::string& number) {
  return payments_data_manager_->GetCreditCardByNumber(number);
}

CreditCard* PersonalDataManager::GetCreditCardByInstrumentId(
    int64_t instrument_id) {
  return payments_data_manager_->GetCreditCardByInstrumentId(instrument_id);
}

CreditCard* PersonalDataManager::GetCreditCardByServerId(
    const std::string& server_id) {
  return payments_data_manager_->GetCreditCardByServerId(server_id);
}

void PersonalDataManager::AddCreditCardBenefitForTest(
    CreditCardBenefit benefit) {
  payments_data_manager_->AddCreditCardBenefitForTest(
      std::move(benefit));  // IN-TEST
}

bool PersonalDataManager::IsDataLoaded() const {
  return address_data_manager_->has_initial_load_finished() &&
         payments_data_manager_->is_payments_data_loaded();
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfiles(
    AddressDataManager::ProfileOrder order) const {
  return address_data_manager_->GetProfiles(order);
}

std::vector<CreditCard*> PersonalDataManager::GetLocalCreditCards() const {
  return payments_data_manager_->GetLocalCreditCards();
}

std::vector<CreditCard*> PersonalDataManager::GetServerCreditCards() const {
  return payments_data_manager_->GetServerCreditCards();
}

std::vector<CreditCard*> PersonalDataManager::GetCreditCards() const {
  return payments_data_manager_->GetCreditCards();
}

PaymentsCustomerData* PersonalDataManager::GetPaymentsCustomerData() const {
  return payments_data_manager_->GetPaymentsCustomerData();
}

std::vector<CreditCardCloudTokenData*>
PersonalDataManager::GetCreditCardCloudTokenData() const {
  return payments_data_manager_->GetCreditCardCloudTokenData();
}

std::vector<AutofillOfferData*> PersonalDataManager::GetAutofillOffers() const {
  return payments_data_manager_->GetAutofillOffers();
}

std::vector<const AutofillOfferData*>
PersonalDataManager::GetActiveAutofillPromoCodeOffersForOrigin(
    GURL origin) const {
  return payments_data_manager_->GetActiveAutofillPromoCodeOffersForOrigin(
      origin);
}

GURL PersonalDataManager::GetCardArtURL(const CreditCard& credit_card) const {
  return payments_data_manager_->GetCardArtURL(credit_card);
}

gfx::Image* PersonalDataManager::GetCreditCardArtImageForUrl(
    const GURL& card_art_url) const {
  return payments_data_manager_->GetCreditCardArtImageForUrl(card_art_url);
}

bool PersonalDataManager::HasPendingPaymentQueriesForTesting() const {
  return payments_data_manager_->HasPendingPaymentQueries();
}

void PersonalDataManager::SetSyncingForTest(bool is_syncing_for_test) {
  payments_data_manager_->SetSyncingForTest(is_syncing_for_test);
}

void PersonalDataManager::Refresh() {
  address_data_manager_->LoadProfiles();
  payments_data_manager_->Refresh();
}

std::vector<CreditCard*> PersonalDataManager::GetCreditCardsToSuggest() const {
  return payments_data_manager_->GetCreditCardsToSuggest();
}

bool PersonalDataManager::IsAutofillEnabled() const {
  return address_data_manager_->IsAutofillProfileEnabled() ||
         payments_data_manager_->IsAutofillPaymentMethodsEnabled();
}

void PersonalDataManager::SetPaymentMethodsMandatoryReauthEnabled(
    bool enabled) {
  payments_data_manager_->SetPaymentMethodsMandatoryReauthEnabled(enabled);
}

bool PersonalDataManager::IsPaymentMethodsMandatoryReauthEnabled() {
  return payments_data_manager_->IsPaymentMethodsMandatoryReauthEnabled();
}

AlternativeStateNameMapUpdater*
PersonalDataManager::get_alternative_state_name_map_updater_for_testing() {
  return address_data_manager_
      ->get_alternative_state_name_map_updater_for_testing();  // IN-TEST
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  payments_data_manager_->SetCreditCards(credit_cards);
}

void PersonalDataManager::NotifyPersonalDataObserver() {
  if (address_data_manager_->IsAwaitingPendingAddressChanges() ||
      payments_data_manager_->HasPendingPaymentQueries()) {
    return;
  }
  for (PersonalDataManagerObserver& observer : observers_) {
    observer.OnPersonalDataChanged();
  }
}

}  // namespace autofill
