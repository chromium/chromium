// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/strike_databases/address_suggestion_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_migration_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_update_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_member.h"
#include "components/sync/service/sync_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace signin {
class IdentityManager;
}

class PrefService;

namespace autofill {

class ContactInfoPreconditionChecker;
class PersonalDataManager;

// Intended to contain all address-related logic of the `PersonalDataManager`.
// Owned by the PDM.
// TODO(b/322170538): Move all address-related logic from the PDM to this file.
class AddressDataManager : public AutofillWebDataServiceObserverOnUISequence,
                           public WebDataServiceConsumer {
 public:
  // Profiles can be retrieved from the AddressDataManager in different orders.
  enum class ProfileOrder {
    // Arbitrary order.
    kNone,
    // In descending order of frecency
    // (`AutofillProfile::HasGreaterRankingThan())`.
    kHighestFrecencyDesc,
    // Most recently modified profiles first.
    kMostRecentlyModifiedDesc,
    // Most recently used profiles first.
    kMostRecentlyUsedFirstDesc,
    kMaxValue = kMostRecentlyUsedFirstDesc
  };

  AddressDataManager(scoped_refptr<AutofillWebDataService> webdata_service,
                     PrefService* pref_service,
                     syncer::SyncService* sync_service,
                     signin::IdentityManager* identity_manager,
                     StrikeDatabaseBase* strike_database,
                     base::RepeatingClosure notify_pdm_observers,
                     GeoIpCountryCode variation_country_code,
                     const std::string& app_locale);

  ~AddressDataManager() override;
  AddressDataManager(const AddressDataManager&) = delete;
  AddressDataManager& operator=(const AddressDataManager&) = delete;

  // AutofillWebDataServiceObserverOnUISequence:
  void OnAutofillChangedBySync(syncer::ModelType model_type) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Returns pointers to the AddressDataManager's underlying vector of Profiles.
  // Their lifetime is until the web database is updated with new information,
  // either through the PDM or via sync.
  // `GetProfiles()` returns local-or-syncable and account profiles. Using
  // `GetProfilesFromSource()`, profiles from a single source can be retrieved.
  // The profiles are returned in the specified `order`.
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfiles(
      ProfileOrder order = ProfileOrder::kNone) const;
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfilesFromSource(
      AutofillProfile::Source profile_source,
      ProfileOrder order = ProfileOrder::kNone) const;

  // Returns the profiles to suggest to the user for filling, ordered by
  // frecency.
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // Returns all `GetProfiles()` in the order that the should be shown in the
  // settings.
  // TODO(crbug.com/1487119): Change return type to
  // std::vector<const AutofillProfile*>
  std::vector<AutofillProfile*> GetProfilesForSettings() const;

  // Returns the profile with the specified `guid`, or nullptr if there is no
  // profile such profile. See `GetProfiles()` for the lifetime of the pointer.
  // TODO(crbug.com/1487119): Change return type to const AutofillProfile*
  AutofillProfile* GetProfileByGUID(const std::string& guid) const;

  // Adds |profile| to the web database.
  virtual void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  virtual void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile by `guid`.
  virtual void RemoveProfile(const std::string& guid);

  // Determines whether the logged in user (if any) is eligible to store
  // Autofill address profiles to their account.
  virtual bool IsEligibleForAddressAccountStorage() const;

  // Users based in unsupported countries and profiles with a country value set
  // to an unsupported country are not eligible for account storage. This
  // function determines if the `country_code` is eligible.
  bool IsCountryEligibleForAccountStorage(std::string_view country_code) const;

  // Migrates a given kLocalOrSyncable `profile` to source kAccount. This has
  // multiple side-effects for the profile:
  // - It is stored in a different backend.
  // - It receives a new GUID.
  // Like all database operations, the migration happens asynchronously.
  // `profile` (the kLocalOrSyncable one) will not be available in the
  // PersonalDataManager anymore once the migrating has finished.
  void MigrateProfileToAccount(const AutofillProfile& profile);

  // Asynchronously loads all `AutofillProfile`s (from all sources) into the
  // class's state. See `synced_local_profiles_` and `account_profiles_`.
  virtual void LoadProfiles();

  // Updates the `profile`'s use count and use date in the database.
  virtual void RecordUseOf(const AutofillProfile& profile);

  // Returns an uppercase ISO 3166-1 alpha-2 country code, which represents our
  // best guess for the country a user is likely to use when inputting a new
  // address. This is used as the default in settings and on form import, if no
  // country field was observed in the submitted form.
  virtual AddressCountryCode GetDefaultCountryCodeForNewAddress() const;

  // Returns true if a `kLocalOrSyncable` profile identified by its guid is
  // blocked for migration to a `kAccount` profile.
  bool IsProfileMigrationBlocked(const std::string& guid) const;

  // Adds a strike to block a profile identified by its `guid` for migrations.
  // Does nothing if the strike database is not available.
  void AddStrikeToBlockProfileMigration(const std::string& guid);

  // Adds enough strikes to the profile identified by `guid` to block migrations
  // for it.
  void AddMaxStrikesToBlockProfileMigration(const std::string& guid);

  // Removes potential strikes to block a profile identified by its `guid` for
  // migrations. Does nothing if the strike database is not available.
  void RemoveStrikesToBlockProfileMigration(const std::string& guid);

  // Returns true if the import of new profiles should be blocked on `url`.
  // Returns false if the strike database is not available, the `url` is not
  // valid or has no host.
  bool IsNewProfileImportBlockedForDomain(const GURL& url) const;

  // Add a strike for blocking the import of new profiles on `url`.
  // Does nothing if the strike database is not available, the `url` is not
  // valid or has no host.
  void AddStrikeToBlockNewProfileImportForDomain(const GURL& url);

  // Removes potential strikes for the import of new profiles from `url`.
  // Does nothing if the strike database is not available, the `url` is not
  // valid or has no host.
  void RemoveStrikesToBlockNewProfileImportForDomain(const GURL& url);

  // Returns true if a profile identified by its `guid` is blocked for updates.
  // Returns false if the database is not available.
  bool IsProfileUpdateBlocked(const std::string& guid) const;

  // Adds a strike to block a profile identified by its `guid` for updates.
  // Does nothing if the strike database is not available.
  void AddStrikeToBlockProfileUpdate(const std::string& guid);

  // Removes potential strikes to block a profile identified by its `guid` for
  // updates. Does nothing if the strike database is not available.
  void RemoveStrikesToBlockProfileUpdate(const std::string& guid);

  // Returns true if a specific field on the web identified by its host form
  // signature, field signature and domain is blocked for address suggestions.
  // Returns false if the database is not available.
  bool AreAddressSuggestionsBlocked(FormSignature form_signature,
                                    FieldSignature field_signature,
                                    const GURL& gurl) const;

  // Adds a strike to block a specific field on the web identified by its host
  // form signature, field signature and domain from having address suggestions
  // displayed. Does nothing if the database is not available.
  void AddStrikeToBlockAddressSuggestions(FormSignature form_signature,
                                          FieldSignature field_signature,
                                          const GURL& gurl);

  // Clears all strikes to block a specific field on the web identified by its
  // host form signature, field signature and domain from having address
  // suggestions displayed. Does nothing if the database is not available.
  void ClearStrikesToBlockAddressSuggestions(FormSignature form_signature,
                                             FieldSignature field_signature,
                                             const GURL& gurl);

  // Clear all relevant strike database strikes whenever some browsing history
  // is deleted. This currently doesn't depend on a history observer directly,
  // but is forwarded from the PDM's history observer.
  void OnHistoryDeletions(const history::DeletionInfo& deletion_info);

  // Returns true if the PDM is currently awaiting an address-related responses
  // from the database. In this case, the PDM's address data is currently
  // potentially inconsistent with the database. Once the state has converged,
  // PersonalDataManagerObserver:: OnPersonalDataFinishedProfileTasks() will be
  // called.
  bool IsAwaitingPendingAddressChanges() const {
    return ProfileChangesAreOngoing() || HasPendingQueries();
  }

  void CancelAllPendingQueries() {
    CancelPendingQuery(pending_synced_local_profiles_query_);
    CancelPendingQuery(pending_account_profiles_query_);
  }

  // Returns the value of the AutofillProfileEnabled pref.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns true if Sync-the-feature is enabled and
  // UserSelectableType::kAutofill is among the user's selected data types.
  // TODO(crbug.com/40066949): Remove this method once ConsentLevel::kSync and
  // SyncService::IsSyncFeatureEnabled() are deleted from the codebase.
  bool IsSyncFeatureEnabledForAutofill() const;

  // Returns true if `syncer::UserSelectableType::kAutofill` is enabled.
  bool IsAutofillUserSelectableTypeEnabled() const;

  // Defines whether the Sync toggle on the Autofill Settings page is visible.
  // TODO(crbug.com/1502843): Remove when toggle becomes available on the Sync
  // page for non-syncing users.
  bool IsAutofillSyncToggleAvailable() const;

  // Sets the Sync UserSelectableType::kAutofill toggle value.
  // TODO(crbug.com/1502843): Used for the toggle on the Autofill Settings page
  // only. It controls syncing of autofill data stored in user accounts for
  // non-syncing users. Remove when toggle becomes available on the Sync page.
  void SetAutofillSelectableTypeEnabled(bool enabled);

  void SetSyncServiceForTest(syncer::SyncService* sync_service) {
    sync_service_ = sync_service;
  }

 protected:
  // Profiles of different sources are stored in different vectors.
  // Several function need to read/write from the correct vector, depending
  // on the source of the profile they are dealing with. This helper function
  // returns the vector where profiles of the given `source` are stored.
  const std::vector<std::unique_ptr<AutofillProfile>>& GetProfileStorage(
      AutofillProfile::Source source) const;
  std::vector<std::unique_ptr<AutofillProfile>>& GetProfileStorage(
      AutofillProfile::Source source) {
    return const_cast<std::vector<std::unique_ptr<AutofillProfile>>&>(
        const_cast<const AddressDataManager*>(this)->GetProfileStorage(source));
  }

  void SetPrefService(PrefService* pref_service);
  void SetStrikeDatabase(StrikeDatabaseBase* strike_database);

  // Used to get a pointer to the strike database for migrating existing
  // profiles. Note, the result can be a nullptr, for example, on incognito
  // mode.
  AutofillProfileMigrationStrikeDatabase* GetProfileMigrationStrikeDatabase();
  virtual const AutofillProfileMigrationStrikeDatabase*
  GetProfileMigrationStrikeDatabase() const;

  // Used to get a pointer to the strike database for importing new profiles.
  // Note, the result can be a nullptr, for example, on incognito
  // mode.
  AutofillProfileSaveStrikeDatabase* GetProfileSaveStrikeDatabase();
  virtual const AutofillProfileSaveStrikeDatabase*
  GetProfileSaveStrikeDatabase() const;

  // Used to get a pointer to the strike database for updating existing
  // profiles. Note, the result can be a nullptr, for example, on incognito
  // mode.
  AutofillProfileUpdateStrikeDatabase* GetProfileUpdateStrikeDatabase();
  virtual const AutofillProfileUpdateStrikeDatabase*
  GetProfileUpdateStrikeDatabase() const;

  // Used to get a pointer to the strike database for updating existing
  // profiles. Note, the result can be a nullptr, for example, on incognito
  // mode.
  AddressSuggestionStrikeDatabase* GetAddressSuggestionStrikeDatabase();
  virtual const AddressSuggestionStrikeDatabase*
  GetAddressSuggestionStrikeDatabase() const;

  // TODO(b/322170538): Remove once the PDM observer is split.
  base::RepeatingClosure notify_pdm_observers_;

  // Tracks whether the first `LoadProfiles()` call has already finished.
  bool has_initial_load_finished_ = false;

  GeoIpCountryCode variation_country_code_;

 private:
  // TODO(b/322170538): Remove once all code writing to `synced_local_profiles_`
  // and `account_profile_` moved to this class.
  friend class PersonalDataManager;

  // A profile change with a boolean representing if the change is ongoing or
  // not. "Ongoing" means that the change is taking place asynchronously on the
  // DB sequence at the moment. Ongoing changes are still part of
  // `ongoing_profile_changes_` to prevent other changes from being scheduled.
  using QueuedAutofillProfileChange = std::pair<AutofillProfileChange, bool>;

  void CancelPendingQuery(WebDataServiceBase::Handle& handle);

  bool HasPendingQueries() const {
    return pending_synced_local_profiles_query_ ||
           pending_account_profiles_query_;
  }

  // Triggered when a profile is added/updated/removed on db.
  void OnAutofillProfileChanged(const AutofillProfileChange& change);

  // Update a profile in AutofillTable asynchronously. The change only surfaces
  // in the PDM after the task on the DB sequence has finished.
  void UpdateProfileInDB(const AutofillProfile& profile);

  // Look at the next profile change for profile with guid = |guid|, and handle
  // it.
  void HandleNextProfileChange(const std::string& guid);
  // returns true if there is any profile change that's still ongoing.
  bool ProfileChangesAreOngoing() const;
  // returns true if there is any ongoing change for profile with guid = |guid|
  // that's still ongoing.
  bool ProfileChangesAreOngoing(const std::string& guid) const;
  // Remove the change from the |ongoing_profile_changes_|, handle next task or
  // Refresh.
  void OnProfileChangeDone(const std::string& guid);

  // Logs metrics around the number of stored profiles after the initial load
  // has finished.
  void LogStoredDataMetrics() const;

  std::unique_ptr<ContactInfoPreconditionChecker>
      contact_info_precondition_checker_;

  // A copy of the profiles stored in `AddressAutofillTable`. They come from
  // two sources:
  // - kLocalOrSyncable: Stored in `synced_local_profiles_`.
  // - kAccount: Stored in `account_profiles_`.
  std::vector<std::unique_ptr<AutofillProfile>> synced_local_profiles_;
  std::vector<std::unique_ptr<AutofillProfile>> account_profiles_;

  // Handles to pending read queries for `synced_local_profiles_` and
  // `account_profiles_`. 0 means that no reads are pending.
  WebDataServiceBase::Handle pending_synced_local_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_account_profiles_query_ = 0;

  // The WebDataService used to schedule tasks on the `AddressAutofillTable`.
  scoped_refptr<AutofillWebDataService> webdata_service_;

  // Used to check whether address Autofill is enabled. May be null in tests,
  // but must otherwise outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // May be null in tests, but must otherwise outlive this instance.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // Make sure to get notified about changes to `AddressAutofillTable` via sync.
  base::ScopedObservation<AutofillWebDataService,
                          AutofillWebDataServiceObserverOnUISequence>
      webdata_service_observer_{this};

  // A timely ordered list of ongoing changes for each profile.
  std::unordered_map<std::string, std::deque<QueuedAutofillProfileChange>>
      ongoing_profile_changes_;

  // An observer to listen for changes to prefs::kAutofillProfileEnabled.
  std::unique_ptr<BooleanPrefMember> profile_enabled_pref_;

  // The database that is used to count guid-keyed strikes to suppress the
  // migration-prompt of new profiles.
  std::unique_ptr<AutofillProfileMigrationStrikeDatabase>
      profile_migration_strike_database_;

  // The database that is used to count domain-keyed strikes to suppress the
  // import of new profiles.
  std::unique_ptr<AutofillProfileSaveStrikeDatabase>
      profile_save_strike_database_;

  // The database that is used to count guid-keyed strikes to suppress updates
  // of existing profiles.
  std::unique_ptr<AutofillProfileUpdateStrikeDatabase>
      profile_update_strike_database_;

  // The database that is used to count form-field-domain-keyed strikes to
  // suppress the display of the Autofill popup for address suggestions on a
  // field.
  std::unique_ptr<AddressSuggestionStrikeDatabase>
      address_suggestion_strike_database_;

  const std::string app_locale_;

  base::WeakPtrFactory<AddressDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_
