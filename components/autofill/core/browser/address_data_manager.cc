// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_manager.h"

#include <iterator>
#include <memory>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"
#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"
#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"
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
      base::ranges::sort(
          profiles, [comparison_time = AutofillClock::Now()](
                        const AutofillProfile* a, const AutofillProfile* b) {
            return a->HasGreaterRankingThan(b, comparison_time);
          });
      break;
    case AddressDataManager::ProfileOrder::kMostRecentlyModifiedDesc:
      base::ranges::sort(
          profiles, [](const AutofillProfile* a, const AutofillProfile* b) {
            return a->modification_date() > b->modification_date();
          });
      break;
    case AddressDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc:
      base::ranges::sort(
          profiles, [](const AutofillProfile* a, const AutofillProfile* b) {
            return a->use_date() > b->use_date();
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
    StrikeDatabaseBase* strike_database,
    GeoIpCountryCode variation_country_code,
    const std::string& app_locale)
    : variation_country_code_(std::move(variation_country_code)),
      webdata_service_(webdata_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      app_locale_(app_locale) {
  alternative_state_name_map_updater_ =
      std::make_unique<AlternativeStateNameMapUpdater>(local_state, this);
  if (webdata_service_) {
    // The `webdata_service_` is null when the TestPDM is used.
    webdata_service_->SetAutofillProfileChangedCallback(
        base::BindRepeating(&AddressDataManager::OnAutofillProfileChanged,
                            weak_factory_.GetWeakPtr()));
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
  }
}

AddressDataManager::~AddressDataManager() {
  CancelPendingQuery(pending_profile_query_);
}

void AddressDataManager::Shutdown() {
  // These classes' sync observers needs to be unregistered.
  contact_info_precondition_checker_.reset();
  address_data_cleaner_.reset();
}

void AddressDataManager::AddObserver(AddressDataManager::Observer* obs) {
  observers_.AddObserver(obs);
}

void AddressDataManager::RemoveObserver(AddressDataManager::Observer* obs) {
  observers_.RemoveObserver(obs);
}

void AddressDataManager::AddChangeCallback(base::OnceClosure callback) {
  change_callbacks_.push_back(std::move(callback));
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
  if (result) {
    CHECK_EQ(result->GetType(), AUTOFILL_PROFILES_RESULT);
    std::vector<AutofillProfile> profiles_from_db =
        static_cast<WDResult<std::vector<AutofillProfile>>*>(result.get())
            ->GetValue();
    profiles_ = std::move(profiles_from_db);
  }

  if (!has_initial_load_finished_) {
    has_initial_load_finished_ = true;
    LogStoredDataMetrics();
  }
  NotifyObservers();
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfiles(
    ProfileOrder order) const {
  std::vector<const AutofillProfile*> profiles =
      base::ToVector(profiles_, [](const AutofillProfile& p) { return &p; });
  // Filter incomplete H/W addresses.
  std::erase_if(profiles, [&](const AutofillProfile* p) {
    switch (p->record_type()) {
      case AutofillProfile::RecordType::kLocalOrSyncable:
      case AutofillProfile::RecordType::kAccount:
        return false;
      case AutofillProfile::RecordType::kAccountHome:
      case AutofillProfile::RecordType::kAccountWork:
        return !IsMinimumAddress(*p);
    }
  });
  OrderProfiles(profiles, order);
  return profiles;
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesByRecordType(
    AutofillProfile::RecordType record_type,
    ProfileOrder order) const {
  std::vector<const AutofillProfile*> profiles = GetProfiles(order);
  std::erase_if(profiles, [&](const AutofillProfile* p) {
    return p->record_type() != record_type;
  });
  return profiles;
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesToSuggest()
    const {
  return IsAutofillProfileEnabled()
             ? GetProfiles(ProfileOrder::kHighestFrecencyDesc)
             : std::vector<const AutofillProfile*>{};
}

std::vector<const AutofillProfile*> AddressDataManager::GetProfilesForSettings()
    const {
  return GetProfiles(ProfileOrder::kMostRecentlyModifiedDesc);
}

const AutofillProfile* AddressDataManager::GetProfileByGUID(
    const std::string& guid) const {
  std::vector<const AutofillProfile*> profiles = GetProfiles();
  auto it = base::ranges::find(
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
    // `OnPersonalDataChanged()`.
    NotifyObservers();
    return;
  }
  ongoing_profile_changes_[profile.guid()].emplace_back(
      AutofillProfileChange(AutofillProfileChange::ADD, profile.guid(),
                            profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange(profile.guid());
}

void AddressDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (!webdata_service_) {
    return;
  }

  // If the profile is empty, remove it unconditionally.
  if (profile.IsEmpty(app_locale_)) {
    RemoveProfile(profile.guid());
    return;
  }

  // The profile is a duplicate of an existing profile if it has a distinct GUID
  // but the same content.
  // Duplicates can exist across record types.
  const std::vector<const AutofillProfile*> profiles =
      GetProfilesByRecordType(profile.record_type());
  auto duplicate_profile_iter = base::ranges::find_if(
      profiles, [&profile](const AutofillProfile* other_profile) {
        return profile.guid() != other_profile->guid() &&
               other_profile->Compare(profile) == 0;
      });

  // Remove the profile if it is a duplicate of another already existing
  // profile.
  if (duplicate_profile_iter != profiles.end()) {
    // Keep the more recently used version of the profile.
    if (profile.use_date() > (*duplicate_profile_iter)->use_date()) {
      UpdateProfileInDB(profile);
      RemoveProfile((*duplicate_profile_iter)->guid());
    } else {
      RemoveProfile(profile.guid());
    }
    return;
  }

  UpdateProfileInDB(profile);
}

void AddressDataManager::RemoveProfile(const std::string& guid) {
  if (!webdata_service_) {
    return;
  }

  // Find the profile to remove.
  // TODO(crbug.com/40258814): This shouldn't be necessary. Providing a `guid`
  // to the `AutofillProfileChange()` should suffice for removals.
  const AutofillProfile* profile =
      ProfileChangesAreOngoing(guid)
          ? &ongoing_profile_changes_[guid].back().first.data_model()
          : GetProfileByGUID(guid);
  if (!profile) {
    NotifyObservers();
    return;
  }

  ongoing_profile_changes_[guid].emplace_back(
      AutofillProfileChange(AutofillProfileChange::REMOVE, guid, *profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange(guid);
}

void AddressDataManager::RemoveLocalProfilesModifiedBetween(base::Time begin,
                                                            base::Time end) {
  for (const AutofillProfile* profile :
       GetProfilesByRecordType(AutofillProfile::RecordType::kLocalOrSyncable)) {
    if (profile->modification_date() >= begin &&
        (end.is_null() || profile->modification_date() < end)) {
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

bool AddressDataManager::IsCountryEligibleForAccountStorage(
    std::string_view country_code) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAccountStorageForIneligibleCountries)) {
    return true;
  }
  constexpr char const* kUnsupportedCountries[] = {"CU", "IR", "KP", "SD",
                                                   "SY"};
  return !base::Contains(kUnsupportedCountries, country_code);
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
  pending_profile_query_ = webdata_service_->GetAutofillProfiles(this);
}

void AddressDataManager::RecordUseOf(const AutofillProfile& profile) {
  const AutofillProfile* adm_profile = GetProfileByGUID(profile.guid());
  if (!adm_profile) {
    return;
  }
  AutofillProfile updated_profile = *adm_profile;
  updated_profile.RecordAndLogUse();
  UpdateProfile(updated_profile);

  if (base::FeatureList::IsEnabled(features::kAutofillTrackMultipleUseDates)) {
    size_t uses = 1;
    while (uses < updated_profile.usage_history_size() &&
           updated_profile.use_date(uses + 1)) {
      uses++;
    }
    base::UmaHistogramExactLinear("Autofill.NumberOfLastUsedDatesAfterFilling",
                                  uses,
                                  updated_profile.usage_history_size() + 1);
  }
}

AddressCountryCode AddressDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  std::string country = variation_country_code_->empty()
                            ? AutofillCountry::CountryCodeForLocale(app_locale_)
                            : variation_country_code_.value();
  return AddressCountryCode(country);
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

  return GetProfileSaveStrikeDatabase()->ShouldBlockFeature(url.host());
}

void AddressDataManager::AddStrikeToBlockNewProfileImportForDomain(
    const GURL& url) {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return;
  }
  GetProfileSaveStrikeDatabase()->AddStrike(url.host());
}

void AddressDataManager::RemoveStrikesToBlockNewProfileImportForDomain(
    const GURL& url) {
  if (!GetProfileSaveStrikeDatabase() || !url.is_valid() || !url.has_host()) {
    return;
  }
  GetProfileSaveStrikeDatabase()->ClearStrikes(url.host());
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
  }
}

void AddressDataManager::SetStrikeDatabase(
    StrikeDatabaseBase* strike_database) {
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
  if (!IsAwaitingPendingAddressChanges()) {
    for (Observer& o : observers_) {
      o.OnAddressDataChanged();
    }
    for (base::OnceClosure& callback : change_callbacks_) {
      std::move(callback).Run();
    }
    change_callbacks_.clear();
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
  // These checks should be removed once the feature is fully launched.
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeInTransportMode) ||
      !pref_service_->GetBoolean(::prefs::kExplicitBrowserSignin)) {
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
  if (!ProfileChangesAreOngoing(guid)) {
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
        profiles_.erase(base::ranges::find(profiles_, existing_profile->guid(),
                                           &AutofillProfile::guid));
        profiles_.push_back(profile);
      }
      break;
    case AutofillProfileChange::REMOVE:
      if (existing_profile) {
        profiles_.erase(base::ranges::find(profiles_, existing_profile->guid(),
                                           &AutofillProfile::guid));
      }
      break;
  }

  OnProfileChangeDone(guid);
}

void AddressDataManager::UpdateProfileInDB(const AutofillProfile& profile) {
  if (!ProfileChangesAreOngoing(profile.guid())) {
    const AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
    if (!existing_profile ||
        existing_profile->EqualsForUpdatePurposes(profile)) {
      NotifyObservers();
      return;
    }
  }

  ongoing_profile_changes_[profile.guid()].emplace_back(
      AutofillProfileChange(AutofillProfileChange::UPDATE, profile.guid(),
                            profile),
      /*is_ongoing=*/false);
  HandleNextProfileChange(profile.guid());
}

void AddressDataManager::HandleNextProfileChange(const std::string& guid) {
  if (!ProfileChangesAreOngoing(guid)) {
    return;
  }

  auto& [change, is_ongoing] = ongoing_profile_changes_[guid].front();
  if (is_ongoing) {
    return;
  }

  const AutofillProfile* existing_profile = GetProfileByGUID(guid);
  const AutofillProfile& profile = change.data_model();
  DCHECK(guid == profile.guid());

  switch (change.type()) {
    case AutofillProfileChange::REMOVE: {
      if (!existing_profile) {
        OnProfileChangeDone(guid);
        return;
      }
      webdata_service_->RemoveAutofillProfile(guid);
      break;
    }
    case AutofillProfileChange::ADD: {
      if (existing_profile ||
          std::ranges::any_of(GetProfilesByRecordType(profile.record_type()),
                              [&](const AutofillProfile* o) {
                                return o->Compare(profile) == 0;
                              })) {
        OnProfileChangeDone(guid);
        return;
      }
      webdata_service_->AddAutofillProfile(profile);
      break;
    }
    case AutofillProfileChange::UPDATE: {
      if (!existing_profile ||
          existing_profile->EqualsForUpdatePurposes(profile)) {
        OnProfileChangeDone(guid);
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
        updated_profile.set_modification_date(AutofillClock::Now());
      }
      webdata_service_->UpdateAutofillProfile(updated_profile);
      break;
    }
  }
  is_ongoing = true;
}

bool AddressDataManager::ProfileChangesAreOngoing(
    const std::string& guid) const {
  auto it = ongoing_profile_changes_.find(guid);
  return it != ongoing_profile_changes_.end() && !it->second.empty();
}

bool AddressDataManager::ProfileChangesAreOngoing() const {
  for (const auto& [guid, change] : ongoing_profile_changes_) {
    if (ProfileChangesAreOngoing(guid)) {
      return true;
    }
  }
  return false;
}

void AddressDataManager::OnProfileChangeDone(const std::string& guid) {
  ongoing_profile_changes_[guid].pop_front();
  NotifyObservers();
  HandleNextProfileChange(guid);
}

void AddressDataManager::LogStoredDataMetrics() const {
  const std::vector<const AutofillProfile*> profiles = GetProfiles();
  autofill_metrics::LogStoredProfileMetrics(profiles);
  autofill_metrics::LogStoredProfileTokenQualityMetrics(profiles);

  if (base::FeatureList::IsEnabled(
          features::kAutofillLogDeduplicationMetrics)) {
    // Since the computation of deduplication metrics is expensive, the
    // recording is delayed by 30 seconds (arbitrary number) to prevent startup
    // time regressions.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<const AddressDataManager> adm) {
              if (!adm) {
                return;
              }
              autofill_metrics::LogDeduplicationStartupMetrics(
                  adm->GetProfiles(), adm->app_locale());
            },
            weak_factory_.GetWeakPtr()),
        base::Seconds(30));
  }

  autofill_metrics::LogLocalProfileSupersetMetrics(std::move(profiles),
                                                   app_locale_);
}

}  // namespace autofill
