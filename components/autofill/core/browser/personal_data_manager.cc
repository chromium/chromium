// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/timezone.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/manual_testing_import.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/cvc_storage_metrics.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"
#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"
#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/strike_databases/address_suggestion_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_migration_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"
#include "components/autofill/core/browser/strike_databases/history_clearable_strike_database.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/version_info/version_info.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {

using autofill_metrics::LogMandatoryReauthOfferOptInDecision;
using autofill_metrics::MandatoryReauthOfferOptInDecision;

namespace {

using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MigrateUserOptedInWalletSyncType {
  kNotMigrated = 0,
  kMigratedFromCanonicalEmail = 1,
  kMigratedFromNonCanonicalEmail = 2,
  kNotMigratedUnexpectedPrimaryAccountIdWithEmail = 3,
  kMaxValue = kNotMigratedUnexpectedPrimaryAccountIdWithEmail,
};

}  // namespace

PersonalDataManager::PersonalDataManager(
    const std::string& app_locale,
    const std::string& variations_country_code)
    : app_locale_(app_locale),
      variations_country_code_(variations_country_code) {}

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
  address_data_manager_ = std::make_unique<AddressDataManager>(
      profile_database, pref_service, strike_database,
      base::BindRepeating(&PersonalDataManager::NotifyPersonalDataObserver,
                          base::Unretained(this)),
      app_locale_);
  payments_data_manager_ = std::make_unique<PaymentsDataManager>(
      profile_database, account_database, image_fetcher,
      std::move(shared_storage_handler), pref_service, app_locale_, this);

  pref_service_ = pref_service;

  alternative_state_name_map_updater_ =
      std::make_unique<AlternativeStateNameMapUpdater>(local_state, this);

  // Listen for URL deletions from browsing history.
  history_service_ = history_service;
  if (history_service_)
    history_service_observation_.Observe(history_service_.get());

  // Listen for account cookie deletion by the user.
  identity_manager_ = identity_manager;
  if (identity_manager_)
    identity_manager_->AddObserver(this);

  SetSyncService(sync_service);

  AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());
  AutofillMetrics::LogIsAutofillProfileEnabledAtStartup(
      address_data_manager_->IsAutofillProfileEnabled());
  AutofillMetrics::LogIsAutofillCreditCardEnabledAtStartup(
      payments_data_manager_->IsAutofillPaymentMethodsEnabled());
  if (payments_data_manager_->IsAutofillPaymentMethodsEnabled()) {
    autofill_metrics::LogIsAutofillPaymentsCvcStorageEnabledAtStartup(
        IsPaymentCvcStorageEnabled());
  }

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
  if (sync_service_)
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;

  if (history_service_)
    history_service_observation_.Reset();
  history_service_ = nullptr;

  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;

  // Make sure that the `address_data_cleaner_` sync observer gets destroyed
  // before the SyncService's `Shutdown()`.
  address_data_cleaner_.reset();
}

void PersonalDataManager::OnURLsDeleted(
    history::HistoryService* /* history_service */,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration() && deletion_info.IsAllHistory()) {
    AutofillCrowdsourcingManager::ClearUploadHistory(pref_service_);
  }
  // TODO(b/322170538): Move to ADM.
  if (address_data_manager_->profile_save_strike_database_) {
    address_data_manager_->profile_save_strike_database_
        ->ClearStrikesWithHistory(deletion_info);
  }
  if (address_data_manager_->address_suggestion_strike_database_) {
    address_data_manager_->address_suggestion_strike_database_
        ->ClearStrikesWithHistory(deletion_info);
  }
}

void PersonalDataManager::OnStateChanged(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);

  // Use the ephemeral account storage when the user didn't enable the sync
  // feature explicitly. `sync_service` is nullptr-checked because this
  // method can also be used (apart from the Sync service observer's calls) in
  // SetSyncService() where setting a nullptr is possible.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  payments_data_manager_->SetUseAccountStorageForServerData(
      sync_service && !sync_service->IsSyncFeatureEnabled());

  if (identity_manager_ && sync_service_ &&
      !sync_service_->GetAccountInfo().IsEmpty()) {
    const CoreAccountInfo account = sync_service_->GetAccountInfo();
    if (!account_status_finder_ ||
        account_status_finder_->GetAccountInfo().account_id !=
            account.account_id) {
      account_status_finder_ =
          std::make_unique<const signin::AccountManagedStatusFinder>(
              identity_manager_, account, base::DoNothing());
    }
  } else {
    account_status_finder_.reset();
  }
}

CoreAccountInfo PersonalDataManager::GetAccountInfoForPaymentsServer() const {
  // Return the account of the active signed-in user irrespective of whether
  // they enabled sync or not.
  return identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

bool PersonalDataManager::IsSyncFeatureEnabledForPaymentsServerMetrics() const {
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_ && sync_service_->IsSyncFeatureEnabled();
}

void PersonalDataManager::OnAccountsCookieDeletedByUserAction() {
  // Clear all the Sync Transport feature opt-ins.
  prefs::ClearSyncTransportOptIns(pref_service_);
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

bool PersonalDataManager::IsPaymentsDownloadActive() const {
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty() ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return false;
  }
  // TODO(crbug.com/40066949): Simplify (merge with
  // IsPaymentsWalletSyncTransportEnabled()) once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_->IsSyncFeatureEnabled() ||
         sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

bool PersonalDataManager::IsPaymentsWalletSyncTransportEnabled() const {
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty() ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return false;
  }
  // TODO(crbug.com/40066949): Simplify (merge with IsPaymentsDownloadActive())
  // once ConsentLevel::kSync and SyncService::IsSyncFeatureEnabled() are
  // deleted from the codebase.
  return !sync_service_->IsSyncFeatureEnabled() &&
         sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

AutofillMetrics::PaymentsSigninState
PersonalDataManager::GetPaymentsSigninStateForMetrics() const {
  using PaymentsSigninState = AutofillMetrics::PaymentsSigninState;

  // Check if the user is signed out.
  if (!sync_service_ || !identity_manager_ ||
      sync_service_->GetAccountInfo().IsEmpty()) {
    return PaymentsSigninState::kSignedOut;
  }

  if (sync_service_->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    return PaymentsSigninState::kSyncPaused;
  }

  // Check if the user has turned on sync.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (sync_service_->IsSyncFeatureEnabled()) {
    return PaymentsSigninState::kSignedInAndSyncFeatureEnabled;
  }

  // Check if Wallet data types are supported.
  if (sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    return PaymentsSigninState::kSignedInAndWalletSyncTransportEnabled;
  }

  return PaymentsSigninState::kSignedIn;
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
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

bool PersonalDataManager::IsEligibleForAddressAccountStorage() const {
  // The CONTACT_INFO data type is only running for eligible users. See
  // ContactInfoModelTypeController.
  return sync_service_ &&
         sync_service_->GetActiveDataTypes().Has(syncer::CONTACT_INFO);
}

bool PersonalDataManager::IsCountryEligibleForAccountStorage(
    std::string_view country_code) const {
  constexpr char const* kUnsupportedCountries[] = {"CU", "IR", "KP", "SD",
                                                   "SY"};
  return !base::Contains(kUnsupportedCountries, country_code);
}

void PersonalDataManager::MigrateProfileToAccount(
    const AutofillProfile& profile) {
  address_data_manager_->MigrateProfileToAccount(profile);
}

std::string PersonalDataManager::AddAsLocalIban(Iban iban) {
  return payments_data_manager_->AddAsLocalIban(std::move(iban));
}

std::string PersonalDataManager::UpdateIban(const Iban& iban) {
  return payments_data_manager_->UpdateIban(iban);
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  payments_data_manager_->AddCreditCard(credit_card);
}

void PersonalDataManager::DeleteLocalCreditCards(
    const std::vector<CreditCard>& cards) {
  payments_data_manager_->DeleteLocalCreditCards(cards);
}

void PersonalDataManager::DeleteAllLocalCreditCards() {
  payments_data_manager_->DeleteAllLocalCreditCards();
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  payments_data_manager_->UpdateCreditCard(credit_card);
}

void PersonalDataManager::UpdateLocalCvc(const std::string& guid,
                                         const std::u16string& cvc) {
  payments_data_manager_->UpdateLocalCvc(guid, cvc);
}

void PersonalDataManager::UpdateServerCardsMetadata(
    const std::vector<CreditCard>& credit_cards) {
  payments_data_manager_->UpdateServerCardsMetadata(credit_cards);
}

void PersonalDataManager::AddServerCvc(int64_t instrument_id,
                                       const std::u16string& cvc) {
  payments_data_manager_->AddServerCvc(instrument_id, cvc);
}

void PersonalDataManager::UpdateServerCvc(int64_t instrument_id,
                                          const std::u16string& cvc) {
  payments_data_manager_->UpdateServerCvc(instrument_id, cvc);
}

void PersonalDataManager::RemoveServerCvc(int64_t instrument_id) {
  payments_data_manager_->RemoveServerCvc(instrument_id);
}

void PersonalDataManager::ClearServerCvcs() {
  payments_data_manager_->ClearServerCvcs();
}

void PersonalDataManager::ClearLocalCvcs() {
  payments_data_manager_->ClearLocalCvcs();
}

void PersonalDataManager::ClearAllServerDataForTesting() {
  payments_data_manager_->ClearAllServerDataForTesting();  // IN-TEST
}

void PersonalDataManager::ClearAllLocalData() {
  payments_data_manager_->GetLocalDatabase()->ClearAllLocalData();
  payments_data_manager_->local_credit_cards_.clear();
  payments_data_manager_->local_ibans_.clear();
  address_data_manager_->synced_local_profiles_.clear();
}

void PersonalDataManager::AddServerCreditCardForTest(
    std::unique_ptr<CreditCard> credit_card) {
  payments_data_manager_->server_credit_cards_.push_back(
      std::move(credit_card));
}

bool PersonalDataManager::IsUsingAccountStorageForServerDataForTest() const {
  return payments_data_manager_->IsUsingAccountStorageForServerData();
}

void PersonalDataManager::AddOfferDataForTest(
    std::unique_ptr<AutofillOfferData> offer_data) {
  payments_data_manager_->autofill_offer_data_.push_back(std::move(offer_data));
}

void PersonalDataManager::SetSyncServiceForTest(
    syncer::SyncService* sync_service) {
  // Before the sync service pointer gets changed, remove the observer.
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
    sync_service_ = nullptr;
  }
  SetSyncService(sync_service);
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
  std::vector<CreditCard*> result;
  if (!IsAutofillWalletImportEnabled())
    return result;
  return payments_data_manager_->GetServerCreditCards();
}

std::vector<CreditCard*> PersonalDataManager::GetCreditCards() const {
  return payments_data_manager_->GetCreditCards();
}

std::vector<const Iban*> PersonalDataManager::GetLocalIbans() const {
  std::vector<const Iban*> result;
  return payments_data_manager_->GetLocalIbans();
}

std::vector<const Iban*> PersonalDataManager::GetServerIbans() const {
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
  return payments_data_manager_->GetServerIbans();
}

std::vector<const Iban*> PersonalDataManager::GetIbans() const {
  return payments_data_manager_->GetIbans();
}

std::vector<const Iban*> PersonalDataManager::GetIbansToSuggest() const {
  return payments_data_manager_->GetIbansToSuggest();
}

PaymentsCustomerData* PersonalDataManager::GetPaymentsCustomerData() const {
  return payments_data_manager_->GetPaymentsCustomerData();
}

std::vector<CreditCardCloudTokenData*>
PersonalDataManager::GetCreditCardCloudTokenData() const {
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
  return payments_data_manager_->GetCreditCardCloudTokenData();
}

std::vector<AutofillOfferData*> PersonalDataManager::GetAutofillOffers() const {
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
  return payments_data_manager_->GetAutofillOffers();
}

std::vector<const AutofillOfferData*>
PersonalDataManager::GetActiveAutofillPromoCodeOffersForOrigin(
    GURL origin) const {
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
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

gfx::Image* PersonalDataManager::GetCachedCardArtImageForUrl(
    const GURL& card_art_url) const {
  if (!IsAutofillWalletImportEnabled())
    return nullptr;
  return payments_data_manager_->GetCachedCardArtImageForUrl(card_art_url);
}

std::vector<VirtualCardUsageData*>
PersonalDataManager::GetVirtualCardUsageData() const {
  if (!IsAutofillWalletImportEnabled()) {
    return {};
  }
  return payments_data_manager_->GetVirtualCardUsageData();
}

void PersonalDataManager::Refresh() {
  address_data_manager_->LoadProfiles();
  payments_data_manager_->Refresh();
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesToSuggest()
    const {
  return address_data_manager_->IsAutofillProfileEnabled()
             ? GetProfiles(ProfileOrder::kHighestFrecencyDesc)
             : std::vector<AutofillProfile*>{};
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesForSettings()
    const {
  return GetProfiles(ProfileOrder::kMostRecentlyModifiedDesc);
}

std::vector<CreditCard*> PersonalDataManager::GetCreditCardsToSuggest() const {
  return payments_data_manager_->GetCreditCardsToSuggest();
}

std::vector<BankAccount> PersonalDataManager::GetMaskedBankAccounts() const {
  return payments_data_manager_->GetMaskedBankAccounts();
}

bool PersonalDataManager::IsAutofillEnabled() const {
  return address_data_manager_->IsAutofillProfileEnabled() ||
         payments_data_manager_->IsAutofillPaymentMethodsEnabled();
}

bool PersonalDataManager::IsAutofillWalletImportEnabled() const {
  if (is_syncing_for_test_) {
    return true;
  }

  if (!sync_service_) {
    // Without `sync_service_`, namely in off-the-record profiles, wallet import
    // is effectively disabled.
    return false;
  }

  return sync_service_->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments);
}

bool PersonalDataManager::ShouldSuggestServerPaymentMethods() const {
  if (!IsAutofillWalletImportEnabled())
    return false;

  if (is_syncing_for_test_)
    return true;

  CHECK(sync_service_);

  // Check if the user is in sync transport mode for wallet data.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!sync_service_->IsSyncFeatureEnabled()) {
    // For SyncTransport, only show server payment methods if the user has opted
    // in to seeing them in the dropdown.
    if (!prefs::IsUserOptedInWalletSyncTransport(
            pref_service_, sync_service_->GetAccountInfo().account_id)) {
      return false;
    }
  }

  // Server payment methods should be suggested if the sync service is active.
  return sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
}

const std::string& PersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  const std::string& most_common_country =
      address_data_manager_->MostCommonCountryCodeFromProfiles();
  if (!most_common_country.empty()) {
    return most_common_country;
  }
  // Failing that, use the country code determined for experiment groups.
  return GetCountryCodeForExperimentGroup();
}

const std::string& PersonalDataManager::GetCountryCodeForExperimentGroup()
    const {
  // Set to |variations_country_code_| if it exists.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ = variations_country_code_;
  }

  // Failing that, guess based on system timezone.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ = base::CountryCodeForCurrentTimezone();
  }

  // Failing that, guess based on locale. This returns "US" if there is no good
  // guess.
  if (experiment_country_code_.empty()) {
    experiment_country_code_ =
        AutofillCountry::CountryCodeForLocale(app_locale());
  }

  return experiment_country_code_;
}

bool PersonalDataManager::IsCardPresentAsBothLocalAndServerCards(
    const CreditCard& credit_card) {
  for (CreditCard* card_from_list : GetCreditCards()) {
    if (credit_card.IsLocalOrServerDuplicateOf(*card_from_list)) {
      return true;
    }
  }
  return false;
}

const CreditCard* PersonalDataManager::GetServerCardForLocalCard(
    const CreditCard* local_card) const {
  DCHECK(local_card);
  if (local_card->record_type() != CreditCard::RecordType::kLocalCard) {
    return nullptr;
  }

  std::vector<CreditCard*> server_cards = GetServerCreditCards();
  auto it =
      base::ranges::find_if(server_cards, [&](const CreditCard* server_card) {
        return local_card->IsLocalOrServerDuplicateOf(*server_card);
      });

  if (it != server_cards.end()) {
    return *it;
  }

  return nullptr;
}

bool PersonalDataManager::IsSyncFeatureEnabledForAutofill() const {
  // TODO(crbug.com/40066949): Remove this method in favor of
  // `IsUserSelectableTypeEnabled` once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_ != nullptr && sync_service_->IsSyncFeatureEnabled() &&
         IsUserSelectableTypeEnabled(syncer::UserSelectableType::kAutofill);
}

bool PersonalDataManager::IsUserSelectableTypeEnabled(
    syncer::UserSelectableType type) const {
  return sync_service_ != nullptr &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(type);
}

void PersonalDataManager::SetAutofillSelectableTypeEnabled(bool enabled) {
  if (sync_service_ != nullptr) {
    sync_service_->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kAutofill, enabled);
  }
}

void PersonalDataManager::SetPaymentMethodsMandatoryReauthEnabled(
    bool enabled) {
  prefs::SetPaymentMethodsMandatoryReauthEnabled(pref_service_, enabled);
}

bool PersonalDataManager::IsPaymentMethodsMandatoryReauthEnabled() {
  return prefs::IsPaymentMethodsMandatoryReauthEnabled(pref_service_);
}

bool PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsMandatoryReauth)) {
    return false;
  }

  // There is no need to show the promo if the feature is already enabled.
  if (prefs::IsPaymentMethodsMandatoryReauthEnabled(pref_service_)) {
#if BUILDFLAG(IS_ANDROID)
    // The mandatory reauth feature is always enabled on automotive, there
    // is/was no opt-in. As such, there is no need to log anything here on
    // automotive.
    if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
      LogMandatoryReauthOfferOptInDecision(
          MandatoryReauthOfferOptInDecision::kAlreadyOptedIn);
    }
#else
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kAlreadyOptedIn);
#endif  // BUILDFLAG(IS_ANDROID)
    return false;
  }

  // If the user has explicitly opted out of this feature previously, then we
  // should not show the opt-in promo.
  if (prefs::IsPaymentMethodsMandatoryReauthSetExplicitly(pref_service_)) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kAlreadyOptedOut);
    return false;
  }

  // We should only show the opt-in promo if we have not reached the maximum
  // number of shows for the promo.
  bool allowed_by_strike_database =
      prefs::IsPaymentMethodsMandatoryReauthPromoShownCounterBelowMaxCap(
          pref_service_);
  if (!allowed_by_strike_database) {
    LogMandatoryReauthOfferOptInDecision(
        MandatoryReauthOfferOptInDecision::kBlockedByStrikeDatabase);
  }
  return allowed_by_strike_database;
#else
  return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
}

void PersonalDataManager::
    IncrementPaymentMethodsMandatoryReauthPromoShownCounter() {
  prefs::IncrementPaymentMethodsMandatoryReauthPromoShownCounter(pref_service_);
}

bool PersonalDataManager::IsPaymentCvcStorageEnabled() {
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableCvcStorageAndFilling) &&
         prefs::IsPaymentCvcStorageEnabled(pref_service_);
}

AutofillImageFetcherBase* PersonalDataManager::GetImageFetcher() const {
  return payments_data_manager_->image_fetcher_;
}

bool PersonalDataManager::IsAutofillSyncToggleAvailable() const {
  auto is_unsupported_passphrase_user = [&] {
    if (!sync_service_) {
      return false;
    }
    return sync_service_->GetUserSettings()->IsUsingExplicitPassphrase() &&
           !base::FeatureList::IsEnabled(
               syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
  };
  auto is_unsupported_dasher_user = [&] {
    if (!account_status_finder_) {
      return false;
    }
    using StatusOutcome = signin::AccountManagedStatusFinder::Outcome;
    StatusOutcome outcome = account_status_finder_->GetOutcome();
    return outcome == StatusOutcome::kEnterprise &&
           !base::FeatureList::IsEnabled(
               syncer::kSyncEnableContactInfoDataTypeForDasherUsers);
  };
  auto is_child_account = [&] {
    if (!sync_service_ || !identity_manager_ ||
        !identity_manager_->AreRefreshTokensLoaded()) {
      return false;
    }
    return identity_manager_
               ->FindExtendedAccountInfo(sync_service_->GetAccountInfo())
               .capabilities.is_subject_to_parental_controls() ==
           signin::Tribool::kTrue;
  };
  return sync_service_ && !sync_service_->GetAccountInfo().IsEmpty() &&
         !sync_service_->HasSyncConsent() &&
         !sync_service_->GetUserSettings()->IsTypeManagedByPolicy(
             syncer::UserSelectableType::kAutofill) &&
         !is_unsupported_passphrase_user() && !is_unsupported_dasher_user() &&
         !is_child_account() &&
         base::FeatureList::IsEnabled(
             syncer::kSyncEnableContactInfoDataTypeInTransportMode) &&
         base::FeatureList::IsEnabled(
             syncer::kSyncDecoupleAddressPaymentSettings) &&
         ::switches::IsExplicitBrowserSigninUIOnDesktopEnabled(
             ::switches::ExplicitBrowserSigninPhase::kFull) &&
         pref_service_->GetBoolean(::prefs::kExplicitBrowserSignin);
}

void PersonalDataManager::AddFullServerCreditCardForTesting(
    const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::RecordType::kFullServerCard, credit_card.record_type());
  DCHECK(!credit_card.IsEmpty(app_locale_));
  DCHECK(!credit_card.server_id().empty());
  DCHECK(payments_data_manager_->GetServerDatabase())
      << "Adding server card without server storage.";

  // Don't add a duplicate.
  if (base::ranges::any_of(payments_data_manager_->server_credit_cards_,
                           [&](const auto& element) {
                             return element->guid() == credit_card.guid();
                           }) ||
      base::ranges::any_of(payments_data_manager_->server_credit_cards_,
                           [&](const auto& element) {
                             return element->Compare(credit_card) == 0;
                           })) {
    return;
  }

  // Add the new credit card to the web database.
  payments_data_manager_->GetServerDatabase()->AddFullServerCreditCard(
      credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

std::optional<signin::AccountManagedStatusFinder::Outcome>
PersonalDataManager::GetAccountStatusForTesting() const {
  return account_status_finder_
             ? account_status_finder_->GetOutcome()
             : std::optional<signin::AccountManagedStatusFinder::Outcome>();
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  payments_data_manager_->SetCreditCards(credit_cards);
}

bool PersonalDataManager::SaveCardLocallyIfNew(
    const CreditCard& imported_card) {
  CHECK(!imported_card.number().empty());

  std::vector<CreditCard> credit_cards;
  for (auto& card : payments_data_manager_->local_credit_cards_) {
    if (card->MatchingCardDetails(imported_card)) {
      return false;
    }
    credit_cards.push_back(*card);
  }
  credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);

  OnCreditCardSaved(/*is_local_card=*/true);
  return true;
}

std::string PersonalDataManager::OnAcceptedLocalCreditCardSave(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  return SaveImportedCreditCard(imported_card);
}

std::string PersonalDataManager::OnAcceptedLocalIbanSave(Iban imported_iban) {
  DCHECK(!imported_iban.value().empty());
  // If an existing IBAN is found, call `UpdateIban()`, otherwise,
  // `AddAsLocalIban()`. `local_ibans_` will be in sync with the local web
  // database as of `Refresh()` which will be called by both `UpdateIban()` and
  // `AddAsLocalIban()`.
  for (auto& iban : payments_data_manager_->local_ibans_) {
    if (iban->value() == imported_iban.value()) {
      // Set the GUID of the IBAN to the one that matches it in
      // `local_ibans_` so that UpdateIban() will be able to update the
      // specific IBAN.
      imported_iban.set_identifier(Iban::Guid(iban->guid()));
      return UpdateIban(imported_iban);
    }
  }
  return AddAsLocalIban(std::move(imported_iban));
}

void PersonalDataManager::SetSyncService(syncer::SyncService* sync_service) {
  CHECK(!sync_service_);

  sync_service_ = sync_service;
  if (sync_service_) {
    sync_service_->AddObserver(this);
  }

  // TODO(crbug.com/1497734): This call is believed no longer necessary here for
  // production (as we no longer re-mask cards in this method), but tests may
  // depend on it still. Investigate and remove if possible.
  OnStateChanged(sync_service_);
}

std::string PersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;

  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (auto& card : payments_data_manager_->local_credit_cards_) {
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged && card->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = card->guid();
      merged = true;
    }

    credit_cards.push_back(*card);
  }

  if (!merged)
    credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);

  // After a card is saved locally, notifies the observers.
  OnCreditCardSaved(/*is_local_card=*/true);

  return guid;
}

bool PersonalDataManager::IsKnownCard(const CreditCard& credit_card) const {
  const auto stripped_pan = CreditCard::StripSeparators(credit_card.number());
  for (const auto& card : payments_data_manager_->local_credit_cards_) {
    if (stripped_pan == CreditCard::StripSeparators(card->number()))
      return true;
  }

  const auto masked_info = credit_card.NetworkAndLastFourDigits();
  for (const auto& card : payments_data_manager_->server_credit_cards_) {
    switch (card->record_type()) {
      case CreditCard::RecordType::kFullServerCard:
        if (stripped_pan == CreditCard::StripSeparators(card->number()))
          return true;
        break;
      case CreditCard::RecordType::kMaskedServerCard:
        if (masked_info == card->NetworkAndLastFourDigits())
          return true;
        break;
      default:
        NOTREACHED();
    }
  }

  return false;
}

bool PersonalDataManager::IsServerCard(const CreditCard* credit_card) const {
  // Check whether the current card itself is a server card.
  if (credit_card->record_type() != CreditCard::RecordType::kLocalCard) {
    return true;
  }

  std::vector<CreditCard*> server_credit_cards = GetServerCreditCards();
  // Check whether the current card is already uploaded.
  for (const CreditCard* server_card : server_credit_cards) {
    if (credit_card->MatchingCardDetails(*server_card)) {
      return true;
    }
  }
  return false;
}

bool PersonalDataManager::ShouldShowCardsFromAccountOption() const {
// The feature is only for Linux, Windows, Mac, and Fuchsia.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  // This option should only be shown for users that have not enabled the Sync
  // Feature and that have server credit cards available.
  // TODO(crbug.com/40066949): Simplify once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  if (!sync_service_ || sync_service_->IsSyncFeatureEnabled() ||
      GetServerCreditCards().empty()) {
    return false;
  }

  bool is_opted_in = prefs::IsUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAccountInfo().account_id);

  // The option should only be shown if the user has not already opted-in.
  return !is_opted_in;
#else
  return false;
#endif  // #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) ||
        // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
}

void PersonalDataManager::OnUserAcceptedCardsFromAccountOption() {
  DCHECK(IsPaymentsWalletSyncTransportEnabled());
  prefs::SetUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAccountInfo().account_id,
      /*opted_in=*/true);
}

void PersonalDataManager::LogServerCardLinkClicked() const {
  AutofillMetrics::LogServerCardLinkClicked(GetPaymentsSigninStateForMetrics());
}

void PersonalDataManager::LogServerIbanLinkClicked() const {
  autofill_metrics::LogServerIbanLinkClicked(
      GetPaymentsSigninStateForMetrics());
}

void PersonalDataManager::OnUserAcceptedUpstreamOffer() {
  // If the user is in sync transport mode for Wallet, record an opt-in.
  if (IsPaymentsWalletSyncTransportEnabled()) {
    prefs::SetUserOptedInWalletSyncTransport(
        pref_service_, sync_service_->GetAccountInfo().account_id,
        /*opted_in=*/true);
  }
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

void PersonalDataManager::OnCreditCardSaved(bool is_local_card) {}

scoped_refptr<AutofillWebDataService> PersonalDataManager::GetLocalDatabase() {
  DCHECK(payments_data_manager_->database_helper_);
  return payments_data_manager_->GetLocalDatabase();
}

}  // namespace autofill
