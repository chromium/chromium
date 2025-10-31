// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_cleaner.h"
#include "components/autofill/core/browser/data_manager/addresses/home_and_work_metadata_store.h"
#include "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"
#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_precondition_checker.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

namespace {

// Orders all `profiles` by the specified `order` rule.
void OrderProfiles(std::vector<const AutofillProfile*>& profiles,
                   AddressDataManager::ProfileOrder order) {
  switch (order) {
    case AddressDataManager::ProfileOrder::kNone:
      break;
    case AddressDataManager::ProfileOrder::kHighestFrecencyDesc:
      std::ranges::sort(
          profiles, [comparison_time = AutofillClock::Now()](
                        const AutofillProfile* a, const AutofillProfile* b) {
            return a->HasGreaterRankingThan(b, comparison_time);
          });
      break;
    case AddressDataManager::ProfileOrder::kMostRecentlyModifiedDesc:
      std::ranges::sort(
          profiles, [](const AutofillProfile* a, const AutofillProfile* b) {
            return a->usage_history().modification_date() >
                   b->usage_history().modification_date();
          });
      break;
    case AddressDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc:
      std::ranges::sort(profiles, [](const AutofillProfile* a,
                                     const AutofillProfile* b) {
        return a->usage_history().use_date() > b->usage_history().use_date();
      });
      break;
  }
}

}  // namespace

AddressDataManager::AddressDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service,
    PrefService* pref_service,
    PrefService* local_state,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    strike_database::StrikeDatabaseBase* strike_database,
    GeoIpCountryCode variation_country_code,
    std::string app_locale)
    : variation_country_code_(std::move(variation_country_code)),
      webdata_service_(webdata_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      app_locale_(std::move(app_locale)) {
  alternative_state_name_map_updater_ =
      std::make_unique<AlternativeStateNameMapUpdater>(local_state, this);
  if (webdata_service_) {
    // The `webdata_service_` is null when the TestPDM is used.
    webdata_service_observer_.Observe(webdata_service_.get());
  }

  if (sync_service_ && identity_manager_) {
    contact_info_precondition_checker_ =
        std::make_unique<ContactInfoPreconditionChecker>(
            sync_service_, identity_manager_,
            /*on_precondition_changed=*/base::DoNothing());
  }

  SetPrefService(pref_service);
  SetStrikeDatabase(strike_database);
  // `IsAutofillProfileEnabled()` relies on the `pref_service_`, which is only
  // null when the `TestAddressDataManager` is used.
  if (pref_service_) {
    autofill_metrics::LogIsAutofillProfileEnabledAtStartup(
        IsAutofillProfileEnabled());
    if (!IsAutofillProfileEnabled()) {
      autofill_metrics::LogAutofillProfileDisabledReasonAtStartup(
          *pref_service_);
    }
    address_data_cleaner_ = std::make_unique<AddressDataCleaner>(
        *this, sync_service, *pref_service_,
        alternative_state_name_map_updater_.get());

    if (identity_manager && sync_service &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForNameAndEmail)) {
      account_name_email_store_ = std::make_unique<AccountNameEmailStore>(
          *this, *identity_manager, *sync_service, *pref_service_);
    }
  }
}

AddressDataManager::~AddressDataManager() {
  CancelPendingQuery(pending_profile_query_);
}

void AddressDataManager::Shutdown() {
  // These classes' sync observers need to be unregistered.
  contact_info_precondition_checker_.reset();
  address_data_cleaner_.reset();
  home_and_work_metadata_.reset();
  account_name_email_store_.reset();
}

void AddressDataManager::AddObserver(AddressDataManager::Observer* obs) {
  observers_.AddObserver(obs);
}

void AddressDataManager::RemoveObserver(AddressDataManager::Observer* obs) {
  observers_.RemoveObserver(obs);
}

void AddressDataManager::OnAutofillChangedBySync(syncer::DataType data_type) {
  if (data_type == syncer::DataType::AUTOFILL_PROFILE ||
      data_type == syncer::DataType::CONTACT_INFO) {
    LoadProfiles();
  }
}

void AddressDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  CHECK_EQ(handle, pending_profile_query_);
  pending_profile_query_ = 0;
  if (!result) {
    return;
  }
  CHECK_EQ(result->GetType(), AUTOFILL_PROFILES_RESULT);
  std::vector<AutofillProfile> profiles_from_db =
      static_cast<WDResult<std::vector<AutofillProfile>>*>(result.get())
          ->GetValue();
  if (!home_and_work_metadata_) {
    profiles_ = std::move(profiles_from_db);
  } else {
    profiles_ = home_and_work_metadata_->ApplyMetadata(
        std::move(profiles_from_db), !has_initial_load_finished_);
  }

  if (!has_initial_load_finished_) {
    has_initial_load_finished_ = true;
    // `AccountNameEmailStore::MaybeUpdateOrCreateAccountNameEmail()` is
    // responsible for creating or updating the `kAccountNameEmail` profile.
    // This requires profiles from the database to be loaded, so any old
    // `kAccountNameEmail` profile can be accessed. Updates to the account info
    // are generally caught by an identity observer. But if the account info
    // becomes available before the initial load has finished, the additional
    // call here is necessary to apply these updates.
    if (account_name_email_store_) {
      account_name_email_store_->MaybeUpdateOrCreateAccountNameEmail();
    } else {
      // In case the feature got disabled the profile should be cleaned up.
      if (!GetProfilesByRecordType(
               AutofillProfile::RecordType::kAccountNameEmail)
               .empty()) {
        RemoveProfile(GetProfilesByRecordType(
                          AutofillProfile::RecordType::kAccountNameEmail)[0]
                          ->guid());
      }
    }
    LogStoredDataMetrics();
  }
  NotifyObservers();
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfiles(
    ProfileOrder order) const {
  std::vector<const AutofillProfile*> profiles =
      base::ToVector(profiles_, [](const AutofillProfile& p) { return &p; });
  OrderProfiles(profiles, order);
  return profiles;
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesByRecordType(
    AutofillProfile::RecordType record_type,
    ProfileOrder order) const {
  return GetProfilesByRecordType(DenseSet({record_type}), order);
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesByRecordType(
    DenseSet<AutofillProfile::RecordType> record_types,
    ProfileOrder order) const {
  std::vector<const AutofillProfile*> profiles = GetProfiles(order);
  std::erase_if(profiles, [&](const AutofillProfile* p) {
    return !record_types.contains(p->record_type());
  });
  return profiles;
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesToSuggest()
    const {
  if (!IsAutofillProfileEnabled()) {
    return {};
  }

  std::vector<const AutofillProfile*> profiles =
      GetProfiles(ProfileOrder::kHighestFrecencyDesc);

  // If the `pref_service_` doesn't exits the special logic which depends on
  // prefs shouldn't run.
  if (!pref_service_) {
    CHECK_IS_TEST();
    return profiles;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForNameAndEmail)) {
    return profiles;
  }

  // `prefs::kAutofillNameAndEmailProfileNotSelectedCounter` counts how many
  // times the suggestion for kAccountNameEmail profile was shown and wasn't
  // accepted.
  const bool should_promote_name_email_profile =
      !pref_service_->GetBoolean(prefs::kAutofillWasNameAndEmailProfileUsed) &&
      (pref_service_->GetInteger(
          prefs::kAutofillNameAndEmailProfileNotSelectedCounter) == 0);
  // Move the kAccountNameEmail profile to the front (or back) depending on
  // the `should_promote_name_email_profile`.
  std::ranges::stable_partition(
      profiles, [should_promote_name_email_profile](
                    const AutofillProfile* p) {
        bool is_name_email_profile =
            p->record_type() == AutofillProfile::RecordType::kAccountNameEmail;
        // stable_partition() moves all elements where the predicate returns
        // true to the front. The name/email profile should be in front when
        // it hasn't been used before and in the back otherwise.
        return should_promote_name_email_profile
                   ? is_name_email_profile
                   : !is_name_email_profile;
      });

  return profiles;
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesForSettings()
    const {
  return GetProfiles(ProfileOrder::kMostRecentlyModifiedDesc);
}

const AutofillProfile* AddressDataManager::GetProfileByGUID(
    const std::string& guid) const {
  std::vector<const AutofillProfile*> profiles = GetProfiles();
  auto it = std::ranges::find(
      profiles, guid,
      [](const AutofillProfile* profile) { return profile->guid(); });
  return it != profiles.end() ? *it : nullptr;
}

void AddressDataManager::AddProfile(const AutofillProfile& profile) {
  if (!webdata_service_ || !IsAutofillProfileEnabled()) {
    return;
  }
  if (profile.IsEmpty(app_locale_)) {
    // TODO(crbug.com/40100455): This call is only used to notify tests to stop
    // waiting. Since no profile is added, this case shouldn't trigger
    // `OnAddressDataChanged()`.
    NotifyObservers();
    return;
  }
  ongoing_profile_changes_.emplace_back(
      AutofillProfileChange(AutofillProfileChange::ADD, profile.guid(),
                            profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange();
}

void AddressDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (!webdata_service_) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillDeduplicateAccountAddresses)) {
    UpdateProfileInDB(profile);
    return;
  }

  // The profile is a duplicate of an existing profile if it has a distinct GUID
  // but the same content.
  // Duplicates can exist across record types.
  const std::vector<const AutofillProfile*> profiles =
      GetProfilesByRecordType(profile.record_type());
  auto duplicate_profile_iter = std::ranges::find_if(
      profiles, [&profile](const AutofillProfile* other_profile) {
        return profile.guid() != other_profile->guid() &&
               other_profile->Compare(profile) == 0;
      });

  // Remove the profile if it is a duplicate of another already existing
  // profile.
  if (duplicate_profile_iter != profiles.end()) {
    // Keep the more recently used version of the profile.
    if (profile.usage_history().use_date() >
        (*duplicate_profile_iter)->usage_history().use_date()) {
      UpdateProfileInDB(profile);
      RemoveProfile((*duplicate_profile_iter)->guid());
    } else {
      RemoveProfile(profile.guid());
    }
    return;
  }

  UpdateProfileInDB(profile);
}

void AddressDataManager::RemoveProfile(
    const std::string& guid,
    bool non_permanent_account_profile_removal) {
  RemoveProfileImpl(guid, non_permanent_account_profile_removal);
}

void AddressDataManager::RemoveLocalProfilesModifiedBetween(base::Time begin,
                                                            base::Time end) {
  for (const AutofillProfile* profile :
       GetProfilesByRecordType(AutofillProfile::RecordType::kLocalOrSyncable)) {
    if (profile->usage_history().modification_date() >= begin &&
        (end.is_null() || profile->usage_history().modification_date() < end)) {
      RemoveProfile(profile->guid());
    }
  }
}

bool AddressDataManager::IsEligibleForAddressAccountStorage() const {
  if (!sync_service_) {
    return false;
  }

  // The CONTACT_INFO data type is only running for eligible users. See
  // ContactInfoDataTypeController.
  return sync_service_->GetActiveDataTypes().Has(syncer::CONTACT_INFO);
}

void AddressDataManager::MigrateProfileToAccount(
    const AutofillProfile& profile) {
  CHECK_EQ(profile.record_type(),
           AutofillProfile::RecordType::kLocalOrSyncable);
  AutofillProfile account_profile = profile.ConvertToAccountProfile();
  DCHECK_NE(profile.guid(), account_profile.guid());
  // Update the database (and this way indirectly Sync).
  RemoveProfile(profile.guid());
  AddProfile(account_profile);
}

void AddressDataManager::OnAutofillProfilePrefChanged() {
  LoadProfiles();
  autofill_metrics::MaybeLogAutofillProfileDisabled(
      CHECK_DEREF(pref_service_.get()));
}

void AddressDataManager::LoadProfiles() {
  if (!webdata_service_) {
    return;
  }
  CancelPendingQuery(pending_profile_query_);
  pending_profile_query_ = webdata_service_->GetAutofillProfiles(
      base::BindOnce(&AddressDataManager::OnWebDataServiceRequestDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AddressDataManager::RecordUseOf(const AutofillProfile& profile) {
  const AutofillProfile* adm_profile = GetProfileByGUID(profile.guid());
  if (!adm_profile) {
    return;
  }
  AutofillProfile updated_profile = *adm_profile;
  updated_profile.RecordAndLogUse();
  if (home_and_work_metadata_) {
    home_and_work_metadata_->RecordProfileFill(updated_profile);
  }
  UpdateProfile(updated_profile);
}

AddressCountryCode AddressDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  return AutofillCountry::GetDefaultCountryCodeForNewAddress(
      variation_country_code_, app_locale_);
}

bool AddressDataManager::IsProfileMigrationBlocked(
    const std::string& guid) const {
  const AutofillProfile* profile = GetProfileByGUID(guid);
  DCHECK(profile == nullptr || !profile->IsAccountProfile());
  if (!GetProfileMigrationStrikeDatabase()) {
    return false;
  }
  return GetProfileMigrationStrikeDatabase()->ShouldBlockFeature(guid);
}

void AddressDataManager::AddStrikeToBlockProfileMigration(
    const std::string& guid) {
  if (!GetProfileMigrationStrikeDatabase()) {
    return;
  }
  GetProfileMigrationStrikeDatabase()->AddStrike(guid);
}

void AddressDataManager::AddMaxStrikesToBlockProfileMigration(
    const std::string& guid) {
  if (AutofillProfileMigrationStrikeDatabase* db =
          GetProfileMigrationStrikeDatabase()) {
    db->AddStrikes(db->GetMaxStrikesLimit() - db->GetStrikes(guid), guid);
  }
}

void AddressDataManager::RemoveStrikesToBlockProfileMigration(
    const std::string& guid) {
  if (!GetProfileMigrationStrikeDatabase()) {
    return;
  }
  GetProfileMigrationStrikeDatabase()->ClearStrikes(guid);
}

bool AddressDataManager::IsNewProfileImportBlockedForDomain(
    const GURL& url) const {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return false;
  }

  return GetProfileSaveStrikeDatabase()->ShouldBlockFeature(url.GetHost());
}

void AddressDataManager::AddStrikeToBlockNewProfileImportForDomain(
    const GURL& url) {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return;
  }
  GetProfileSaveStrikeDatabase()->AddStrike(url.GetHost());
}

void AddressDataManager::RemoveStrikesToBlockNewProfileImportForDomain(
    const GURL& url) {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return;
  }
  GetProfileSaveStrikeDatabase()->ClearStrikes(url.GetHost());
}

bool AddressDataManager::IsProfileUpdateBlocked(const std::string& guid) const {
  if (!GetProfileUpdateStrikeDatabase()) {
    return false;
  }
  return GetProfileUpdateStrikeDatabase()->ShouldBlockFeature(guid);
}

void AddressDataManager::AddStrikeToBlockProfileUpdate(
    const std::string& guid) {
  if (!GetProfileUpdateStrikeDatabase()) {
    return;
  }
  GetProfileUpdateStrikeDatabase()->AddStrike(guid);
}

void AddressDataManager::RemoveStrikesToBlockProfileUpdate(
    const std::string& guid) {
  if (!GetProfileUpdateStrikeDatabase()) {
    return;
  }
  GetProfileUpdateStrikeDatabase()->ClearStrikes(guid);
}

bool AddressDataManager::AreAddressSuggestionsBlocked(
    FormSignature form_signature,
    FieldSignature field_signature,
    const GURL& gurl) const {
  if (!GetAddressSuggestionStrikeDatabase()) {
    return false;
  }
  return GetAddressSuggestionStrikeDatabase()->ShouldBlockFeature(
      AddressSuggestionStrikeDatabase::GetId(form_signature, field_signature,
                                             gurl));
}

void AddressDataManager::AddStrikeToBlockAddressSuggestions(
    FormSignature form_signature,
    FieldSignature field_signature,
    const GURL& gurl) {
  if (!GetAddressSuggestionStrikeDatabase()) {
    return;
  }
  GetAddressSuggestionStrikeDatabase()->AddStrike(
      AddressSuggestionStrikeDatabase::GetId(form_signature, field_signature,
                                             gurl));
}

void AddressDataManager::ClearStrikesToBlockAddressSuggestions(
    FormSignature form_signature,
    FieldSignature field_signature,
    const GURL& gurl) {
  if (!GetAddressSuggestionStrikeDatabase()) {
    return;
  }
  GetAddressSuggestionStrikeDatabase()->ClearStrikes(
      AddressSuggestionStrikeDatabase::GetId(form_signature, field_signature,
                                             gurl));
}

void AddressDataManager::OnHistoryDeletions(
    const history::DeletionInfo& deletion_info) {
  if (profile_save_strike_database_) {
    profile_save_strike_database_->ClearStrikesWithHistory(deletion_info);
  }
  if (address_suggestion_strike_database_) {
    address_suggestion_strike_database_->ClearStrikesWithHistory(deletion_info);
  }
}

void AddressDataManager::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
  profile_enabled_pref_ = std::make_unique<BooleanPrefMember>();
  // `pref_service_` can be nullptr in tests. Using base::Unretained(this) is
  // safe because observer instances are destroyed once `this` is destroyed.
  if (pref_service_) {
    profile_enabled_pref_->Init(
        prefs::kAutofillProfileEnabled, pref_service_,
        base::BindRepeating(&AddressDataManager::OnAutofillProfilePrefChanged,
                            base::Unretained(this)));
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForHomeAndWork)) {
      home_and_work_metadata_ = std::make_unique<HomeAndWorkMetadataStore>(
          pref_service_, sync_service_,
          base::BindRepeating(&AddressDataManager::LoadProfiles,
                              base::Unretained(this)));
    }
  }
}

void AddressDataManager::SetStrikeDatabase(
    strike_database::StrikeDatabaseBase* strike_database) {
  if (!strike_database) {
    return;
  }
  profile_migration_strike_database_ =
      std::make_unique<AutofillProfileMigrationStrikeDatabase>(strike_database);
  profile_save_strike_database_ =
      std::make_unique<AutofillProfileSaveStrikeDatabase>(strike_database);
  profile_update_strike_database_ =
      std::make_unique<AutofillProfileUpdateStrikeDatabase>(strike_database);
  address_suggestion_strike_database_ =
      std::make_unique<AddressSuggestionStrikeDatabase>(strike_database);
  if (base::FeatureList::IsEnabled(
          features::kAutofillAddressSuggestionsOnTypingHasStrikeDatabase)) {
    address_on_typing_suggestion_strike_database_ =
        std::make_unique<AddressOnTypingSuggestionStrikeDatabase>(
            strike_database);
  }
}

AddressOnTypingSuggestionStrikeDatabase*
AddressDataManager::GetAddressOnTypingSuggestionStrikeDatabase() {
  return const_cast<AddressOnTypingSuggestionStrikeDatabase*>(
      std::as_const(*this).GetAddressOnTypingSuggestionStrikeDatabase());
}

const AddressOnTypingSuggestionStrikeDatabase*
AddressDataManager::GetAddressOnTypingSuggestionStrikeDatabase() const {
  return address_on_typing_suggestion_strike_database_.get();
}

AutofillProfileMigrationStrikeDatabase*
AddressDataManager::GetProfileMigrationStrikeDatabase() {
  return const_cast<AutofillProfileMigrationStrikeDatabase*>(
      std::as_const(*this).GetProfileMigrationStrikeDatabase());
}

const AutofillProfileMigrationStrikeDatabase*
AddressDataManager::GetProfileMigrationStrikeDatabase() const {
  return profile_migration_strike_database_.get();
}

AutofillProfileSaveStrikeDatabase*
AddressDataManager::GetProfileSaveStrikeDatabase() {
  return const_cast<AutofillProfileSaveStrikeDatabase*>(
      std::as_const(*this).GetProfileSaveStrikeDatabase());
}

const AutofillProfileSaveStrikeDatabase*
AddressDataManager::GetProfileSaveStrikeDatabase() const {
  return profile_save_strike_database_.get();
}

AutofillProfileUpdateStrikeDatabase*
AddressDataManager::GetProfileUpdateStrikeDatabase() {
  return const_cast<AutofillProfileUpdateStrikeDatabase*>(
      std::as_const(*this).GetProfileUpdateStrikeDatabase());
}

const AutofillProfileUpdateStrikeDatabase*
AddressDataManager::GetProfileUpdateStrikeDatabase() const {
  return profile_update_strike_database_.get();
}

AddressSuggestionStrikeDatabase*
AddressDataManager::GetAddressSuggestionStrikeDatabase() {
  return const_cast<AddressSuggestionStrikeDatabase*>(
      std::as_const(*this).GetAddressSuggestionStrikeDatabase());
}

const AddressSuggestionStrikeDatabase*
AddressDataManager::GetAddressSuggestionStrikeDatabase() const {
  return address_suggestion_strike_database_.get();
}

void AddressDataManager::NotifyObservers() {
  if (IsAwaitingPendingAddressChanges()) {
    return;
  }
  for (Observer& o : observers_) {
    o.OnAddressDataChanged();
  }
}

bool AddressDataManager::IsAutofillProfileEnabled() const {
  return prefs::IsAutofillProfileEnabled(pref_service_);
}

bool AddressDataManager::IsSyncFeatureEnabledForAutofill() const {
  // TODO(crbug.com/40066949): Remove this method in favor of
  // `IsUserSelectableTypeEnabled` once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  return sync_service_ != nullptr && sync_service_->IsSyncFeatureEnabled() &&
         IsAutofillUserSelectableTypeEnabled();
}

bool AddressDataManager::IsAutofillUserSelectableTypeEnabled() const {
  return sync_service_ != nullptr &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kAutofill);
}

bool AddressDataManager::IsAutofillSyncToggleAvailable() const {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return false;
  }

  if (!pref_service_->GetBoolean(::prefs::kExplicitBrowserSignin)) {
    return false;
  }

  if (!sync_service_) {
    return false;
  }

  // Do not show the toggle if Sync is disabled on in error.
  if (sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED ||
      sync_service_->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED) {
    return false;
  }

  // Do not show the toggle for syncing users.
  if (sync_service_->HasSyncConsent()) {
    return false;
  }

  if (sync_service_->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kAutofill)) {
    return false;
  }

  return contact_info_precondition_checker_ &&
         contact_info_precondition_checker_->GetPreconditionState() ==
             syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void AddressDataManager::SetAutofillSelectableTypeEnabled(bool enabled) {
  if (sync_service_ != nullptr) {
    sync_service_->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kAutofill, enabled);
  }
}

std::optional<CoreAccountInfo> AddressDataManager::GetPrimaryAccountInfo()
    const {
  if (identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return identity_manager_->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSignin);
  }
  return std::nullopt;
}

#if BUILDFLAG(IS_IOS)
void AddressDataManager::MaybeCreateAccountNameEmailProfile(
    std::string account_name,
    std::string email) {
  if (account_name_email_store_ &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForNameAndEmail)) {
    account_name_email_store_->MaybeUpdateOrCreateAccountNameEmail(account_name,
                                                                   email);
  }
}
#endif

void AddressDataManager::CancelPendingQuery(
    WebDataServiceBase::Handle& handle) {
  if (!webdata_service_ || !handle) {
    return;
  }
  webdata_service_->CancelRequest(handle);
  handle = 0;
}

void AddressDataManager::OnAutofillProfileChanged(
    const AutofillProfileChange& change) {
  const std::string& guid = change.key();
  const AutofillProfile& profile = change.data_model();
  DCHECK(guid == profile.guid());
  if (ongoing_profile_changes_.empty()) {
    return;
  }

  const AutofillProfile* existing_profile = GetProfileByGUID(guid);
  switch (change.type()) {
    case AutofillProfileChange::ADD:
      if (!existing_profile &&
          std::ranges::none_of(
              GetProfilesByRecordType(profile.record_type()),
              [&](const auto& o) { return o->Compare(profile) == 0; })) {
        profiles_.push_back(profile);
      }
      break;
    case AutofillProfileChange::UPDATE:
      if (existing_profile &&
          !existing_profile->EqualsForUpdatePurposes(profile)) {
        profiles_.erase(std::ranges::find(profiles_, existing_profile->guid(),
                                          &AutofillProfile::guid));
        profiles_.push_back(profile);
      }
      break;
    case AutofillProfileChange::HIDE_IN_AUTOFILL:
    case AutofillProfileChange::REMOVE:
      if (existing_profile) {
        profiles_.erase(std::ranges::find(profiles_, existing_profile->guid(),
                                          &AutofillProfile::guid));
      }
      break;
  }

  OnProfileChangeDone();
}

void AddressDataManager::UpdateProfileInDB(const AutofillProfile& profile) {
  ongoing_profile_changes_.emplace_back(
      AutofillProfileChange(AutofillProfileChange::UPDATE, profile.guid(),
                            profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange();
}

void AddressDataManager::HandleNextProfileChange() {
  if (ongoing_profile_changes_.empty()) {
    return;
  }

  auto& [change, is_ongoing] = ongoing_profile_changes_.front();
  if (is_ongoing) {
    return;
  }

  const AutofillProfile& profile = change.data_model();
  const AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());

  switch (change.type()) {
    case AutofillProfileChange::HIDE_IN_AUTOFILL:
    case AutofillProfileChange::REMOVE: {
      if (!existing_profile) {
        OnProfileChangeDone();
        return;
      }
      webdata_service_->RemoveAutofillProfile(
          profile.guid(), change.type(),
          base::BindOnce(&AddressDataManager::OnAutofillProfileChanged,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case AutofillProfileChange::ADD: {
      if (existing_profile ||
          std::ranges::any_of(GetProfilesByRecordType(profile.record_type()),
                              [&](const AutofillProfile* o) {
                                return o->Compare(profile) == 0;
                              })) {
        OnProfileChangeDone();
        return;
      }
      webdata_service_->AddAutofillProfile(
          profile, base::BindOnce(&AddressDataManager::OnAutofillProfileChanged,
                                  weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case AutofillProfileChange::UPDATE: {
      if (!existing_profile ||
          existing_profile->EqualsForUpdatePurposes(profile)) {
        OnProfileChangeDone();
        return;
      }
      // At this point, the `existing_profile` is consistent with
      // AutofillTable's state. Reset observations for all types that change due
      // to this update.
      AutofillProfile updated_profile = profile;
      updated_profile.token_quality().ResetObservationsForDifferingTokens(
          *existing_profile);
      // Unless only metadata has changed, which operator== ignores, update the
      // modification date. This happens e.g. when increasing the use count.
      if (*existing_profile != updated_profile) {
        updated_profile.usage_history().set_modification_date(
            AutofillClock::Now());
      }
      webdata_service_->UpdateAutofillProfile(
          updated_profile,
          base::BindOnce(&AddressDataManager::OnAutofillProfileChanged,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
  }
  if (home_and_work_metadata_) {
    home_and_work_metadata_->ApplyChange(change);
  }
  if (account_name_email_store_) {
    account_name_email_store_->ApplyChange(change);
  }
  is_ongoing = true;
}

void AddressDataManager::OnProfileChangeDone() {
  ongoing_profile_changes_.pop_front();
  NotifyObservers();
  HandleNextProfileChange();
}

void AddressDataManager::LogStoredDataMetrics() const {
  const std::vector<const AutofillProfile*> profile_pointers = GetProfiles();
  autofill_metrics::LogStoredProfileMetrics(profile_pointers);
  autofill_metrics::LogStoredProfileTokenQualityMetrics(profile_pointers);
  autofill_metrics::LogStoredProfileCountWithAlternativeName(profile_pointers);
  // TODO(crbug.com/357074792): Once the feature is launched, remove the
  // code inside the if-statement, it won't be needed anymore.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDeduplicateAccountAddresses)) {
    autofill_metrics::LogLocalProfileSupersetMetrics(
        std::move(profile_pointers), app_locale_);
  }
}

void AddressDataManager::RemoveProfileImpl(
    const std::string& guid,
    bool non_permanent_account_profile_removal) {
  if (!webdata_service_) {
    return;
  }

  // Find the profile to remove. Even though a `guid` uniquely identifies a
  // profile, downstream code (most notably sync) needs to know the `RecordType`
  // of the removed profile as well (since only account profiles are uploaded).
  // `ongoing_profile_changes_` are executed in order, so the last ongoing
  // change for `guid` describe the profile's state by the time this removal
  // should happen.
  auto it = std::find_if(ongoing_profile_changes_.rbegin(),
                         ongoing_profile_changes_.rend(),
                         [&](const auto& x) { return x.first.key() == guid; });
  const AutofillProfile* profile = it == ongoing_profile_changes_.rend()
                                       ? GetProfileByGUID(guid)
                                       : &it->first.data_model();
  if (!profile) {
    NotifyObservers();
    return;
  }

  ongoing_profile_changes_.emplace_back(
      AutofillProfileChange((profile->IsAccountProfile() &&
                             non_permanent_account_profile_removal) ||
                                    profile->IsHomeAndWorkProfile()
                                ? AutofillProfileChange::HIDE_IN_AUTOFILL
                                : AutofillProfileChange::REMOVE,
                            guid, *profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange();
}

}  // namespace autofill
