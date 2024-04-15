// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace autofill {

PersonalDataManager::PersonalDataManager(
    const std::string& app_locale,
    const std::string& variations_country_code)
    : app_locale_(app_locale),
      variations_country_code_(GeoIpCountryCode(variations_country_code)) {}

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : PersonalDataManager(app_locale, std::string()) {}

void PersonalDataManager::Init(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    PrefService* pref_service,
    PrefService* local_state,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service,
    syncer::SyncService* sync_service,
    StrikeDatabaseBase* strike_database,
    AutofillImageFetcherBase* image_fetcher,
    std::unique_ptr<AutofillSharedStorageHandler> shared_storage_handler) {
  // The TestPDM already initializes the (address|payments)_data_manager in it's
  // constructor with dedicated test instances. In general, `Init()` should not
  // be called on a TestPDM, since the TestPDM's purpose is to fake the PDM's
  // dependencies, rather than inject them through `Init()`.
  DCHECK(!address_data_manager_) << "Don't call Init() on a TestPDM";
  auto notify_observers = base::BindRepeating(
      &PersonalDataManager::NotifyPersonalDataObserver, base::Unretained(this));
  address_data_manager_ = std::make_unique<AddressDataManager>(
      profile_database, pref_service, sync_service, identity_manager,
      strike_database, notify_observers, variations_country_code_, app_locale_);
  payments_data_manager_ = std::make_unique<PaymentsDataManager>(
      profile_database, account_database, image_fetcher,
      std::move(shared_storage_handler), pref_service, sync_service,
      identity_manager, variations_country_code_, app_locale_,
      notify_observers);

  pref_service_ = pref_service;
  identity_manager_ = identity_manager;

  alternative_state_name_map_updater_ =
      std::make_unique<AlternativeStateNameMapUpdater>(local_state, this);

  // Listen for URL deletions from browsing history.
  history_service_ = history_service;
  if (history_service_)
    history_service_observation_.Observe(history_service_.get());

  AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  // WebDataService may not be available in tests.
  if (!profile_database) {
    return;
  }

  Refresh();

  address_data_cleaner_ = std::make_unique<AddressDataCleaner>(
      *this, sync_service, CHECK_DEREF(pref_service),
      alternative_state_name_map_updater_.get());

  // Potentially import profiles for testing. `Init()` is called whenever the
  // corresponding Chrome profile is created. This is either during start-up or
  // when the Chrome profile is changed.
  MaybeImportDataForManualTesting(weak_factory_.GetWeakPtr());
}

PersonalDataManager::~PersonalDataManager() = default;

void PersonalDataManager::Shutdown() {
  identity_manager_ = nullptr;

  if (history_service_)
    history_service_observation_.Reset();
  history_service_ = nullptr;

  // The following members register observers, which needs to be unregistered
  // before the dependent service's `Shutdown()`.
  address_data_cleaner_.reset();
  address_data_manager_.reset();
  payments_data_manager_.reset();
}

void PersonalDataManager::OnHistoryDeletions(
    history::HistoryService* /* history_service */,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration() && deletion_info.IsAllHistory()) {
    AutofillCrowdsourcingManager::ClearUploadHistory(pref_service_);
  }
  address_data_manager_->OnHistoryDeletions(deletion_info);
}

std::optional<CoreAccountInfo> PersonalDataManager::GetPrimaryAccountInfo()
    const {
  if (identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager_->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSignin);
  }

  return std::nullopt;
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::AddChangeCallback(base::OnceClosure callback) {
  change_callbacks_.push_back(std::move(callback));
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PersonalDataManager::RecordUseOf(
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card) {
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    payments_data_manager_->RecordUseOfCard(
        absl::get<const CreditCard*>(profile_or_credit_card));
  } else {
    address_data_manager_->RecordUseOf(
        *absl::get<const AutofillProfile*>(profile_or_credit_card));
  }
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  address_data_manager_->AddProfile(profile);
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  address_data_manager_->UpdateProfile(profile);
}

AutofillProfile* PersonalDataManager::GetProfileByGUID(
    const std::string& guid) const {
  return address_data_manager_->GetProfileByGUID(guid);
}

bool PersonalDataManager::IsCountryEligibleForAccountStorage(
    std::string_view country_code) const {
  return address_data_manager_->IsCountryEligibleForAccountStorage(
      country_code);
}

void PersonalDataManager::MigrateProfileToAccount(
    const AutofillProfile& profile) {
  address_data_manager_->MigrateProfileToAccount(profile);
}

std::string PersonalDataManager::AddAsLocalIban(Iban iban) {
  return payments_data_manager_->AddAsLocalIban(std::move(iban));
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
  payments_data_manager_->server_credit_cards_.push_back(
      std::move(credit_card));
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

bool PersonalDataManager::IsDataLoaded() const {
  return address_data_manager_->has_initial_load_finished_ &&
         payments_data_manager_->is_payments_data_loaded_;
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfiles(
    ProfileOrder order) const {
  return address_data_manager_->GetProfiles(order);
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesFromSource(
    AutofillProfile::Source profile_source,
    ProfileOrder order) const {
  return address_data_manager_->GetProfilesFromSource(profile_source, order);
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

std::vector<VirtualCardUsageData*>
PersonalDataManager::GetVirtualCardUsageData() const {
  return payments_data_manager_->GetVirtualCardUsageData();
}

void PersonalDataManager::Refresh() {
  address_data_manager_->LoadProfiles();
  payments_data_manager_->Refresh();
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesToSuggest()
    const {
  return address_data_manager_->GetProfilesToSuggest();
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesForSettings()
    const {
  return address_data_manager_->GetProfilesForSettings();
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

  for (base::OnceClosure& callback : change_callbacks_) {
    std::move(callback).Run();
  }
  change_callbacks_.clear();
}

}  // namespace autofill
