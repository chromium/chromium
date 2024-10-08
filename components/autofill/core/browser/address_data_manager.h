// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
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
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace signin {
class IdentityManager;
}

class PrefService;

namespace autofill {

class AddressDataCleaner;
class AlternativeStateNameMapUpdater;
class ContactInfoPreconditionChecker;

// Contains all address-related logic of the `PersonalDataManager`. See comment
// above the `PersonalDataManager` first. In the `AddressDataManager` (ADM),
// `Refresh()` is called `LoadProfiles()`.
// Owned by the PDM.
//
// Technical details on how modifications are implemented:
// The ADM queues pending changes in `ongoing_profile_changes_`. For each
// profile, they are executed in order and the next change is only posted to the
// DB sequence once the previous change has finished. After each change that
// finishes, the `AutofillWebDataService` notifies the ADM via
// `OnAutofillProfileChanged(change)` - and the ADM updates its state
// accordingly. No `LoadProfiles()` is performed.
// Queuing the pending modifications is necessary, so the ADM can do consistency
// checks against the latest state. For example, a remove should only be
// performed if the profile exists. Without the queuing, if a remove operation
// was posted before the add operation has finished, the remove would
// incorrectly get rejected by the ADM.
class AddressDataManager : public AutofillWebDataServiceObserverOnUISequence,
                           public WebDataServiceConsumer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Triggered after all pending read and write operations have finished.
    virtual void OnAddressDataChanged() = 0;
  };

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
                     PrefService* local_state,
                     syncer::SyncService* sync_service,
                     signin::IdentityManager* identity_manager,
                     StrikeDatabaseBase* strike_database,
                     GeoIpCountryCode variation_country_code,
                     const std::string& app_locale);

  ~AddressDataManager() override;
  AddressDataManager(const AddressDataManager&) = delete;
  AddressDataManager& operator=(const AddressDataManager&) = delete;

  // Only intended to be called during shutdown of the parent `KeyedService`.
  void Shutdown();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Adds a callback which will be triggered on the next address data change,
  // at the same time `Observer::OnAddressDataChanged()` of `observers_` is
  // called.
  void AddChangeCallback(base::OnceClosure callback);

  // AutofillWebDataServiceObserverOnUISequence:
  void OnAutofillChangedBySync(syncer::DataType data_type) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Returns pointers to the AddressDataManager's underlying vector of Profiles.
  // Their lifetime is until the web database is updated with new information,
  // either through the PDM or via sync.
  // `GetProfiles()` returns local-or-syncable and account profiles. Using
  // `GetProfilesByRecordType()`, profiles from a single record type can be
  // retrieved. The profiles are returned in the specified `order`.
  // Incomplete H/W addresses (lat/long) are filtered, since they are not
  // useful for autofilling. They are exposed to Chrome to decide if a promotion
  // flow is applicable.
  std::vector<const AutofillProfile*> GetProfiles(
      ProfileOrder order = ProfileOrder::kNone) const;
  std::vector<const AutofillProfile*> GetProfilesByRecordType(
      AutofillProfile::RecordType record_type,
      ProfileOrder order = ProfileOrder::kNone) const;

  // Returns the profiles to suggest to the user for filling, ordered by
  // frecency.
  std::vector<const AutofillProfile*> GetProfilesToSuggest() const;

  // Returns all `GetProfiles()` in the order that the should be shown in the
  // settings.
  std::vector<const AutofillProfile*> GetProfilesForSettings() const;

  // Returns the profile with the specified `guid`, or nullptr if there is no
  // profile such profile. See `GetProfiles()` for the lifetime of the pointer.
  const AutofillProfile* GetProfileByGUID(const std::string& guid) const;

  // Adds |profile| to the web database.
  virtual void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  virtual void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile by `guid`.
  virtual void RemoveProfile(const std::string& guid);

  // Removes all local profiles modified on or after `delete_begin` and strictly
  // before `delete_end`. Used for browsing data deletion purposes.
  // TODO(crbug.com/363970493): Consider account addresses somehow?
  void RemoveLocalProfilesModifiedBetween(base::Time begin, base::Time end);

  // Determines whether the logged in user (if any) is eligible to store
  // Autofill address profiles to their account.
  virtual bool IsEligibleForAddressAccountStorage() const;

  // Users based in unsupported countries and profiles with a country value set
  // to an unsupported country are not eligible for account storage. This
  // function determines if the `country_code` is eligible.
  bool IsCountryEligibleForAccountStorage(std::string_view country_code) const;

  // Migrates a given kLocalOrSyncable `profile` to kAccount. This has multiple
  // side-effects for the profile:
  // - It is stored in a different backend.
  // - It receives a new GUID.
  // Like all database operations, the migration happens asynchronously.
  // `profile` (the kLocalOrSyncable one) will not be available in the
  // AddressDataManager anymore once the migrating has finished.
  void MigrateProfileToAccount(const AutofillProfile& profile);

  // Asynchronously loads all `AutofillProfile`s (from all record types) into
  // the class's state. See `synced_local_profiles_` and `account_profiles_`.
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
  // PersonalDataManagerObserver:: OnPersonalDataChanged() will be called.
  bool IsAwaitingPendingAddressChanges() const {
    return ProfileChangesAreOngoing() || pending_profile_query_ != 0;
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
  // TODO(crbug.com/40943238): Remove when toggle becomes available on the Sync
  // page for non-syncing users.
  bool IsAutofillSyncToggleAvailable() const;

  // Sets the Sync UserSelectableType::kAutofill toggle value.
  // TODO(crbug.com/40943238): Used for the toggle on the Autofill Settings page
  // only. It controls syncing of autofill data stored in user accounts for
  // non-syncing users. Remove when toggle becomes available on the Sync page.
  void SetAutofillSelectableTypeEnabled(bool enabled);

  // Returns the account info of currently signed-in user, or std::nullopt if
  // the user is not signed-in or the identity manager is not available.
  std::optional<CoreAccountInfo> GetPrimaryAccountInfo() const;

  bool has_initial_load_finished() const { return has_initial_load_finished_; }

  const std::string& app_locale() const { return app_locale_; }

  void SetSyncServiceForTest(syncer::SyncService* sync_service) {
    sync_service_ = sync_service;
  }

  bool auto_accept_address_imports_for_testing() const {
    return auto_accept_address_imports_for_testing_;
  }

  AlternativeStateNameMapUpdater*
  get_alternative_state_name_map_updater_for_testing() {
    return alternative_state_name_map_updater_.get();
  }

 protected:
  friend class AddressDataManagerTestApi;

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

  void NotifyObservers();

  // A copy of the profiles of all different record types stored in
  // `AddressAutofillTable` in unspecified order.
  std::vector<AutofillProfile> profiles_;

  // Tracks whether the first `LoadProfiles()` call has already finished.
  bool has_initial_load_finished_ = false;

  GeoIpCountryCode variation_country_code_;

 private:
  // A profile change with a boolean representing if the change is ongoing or
  // not. "Ongoing" means that the change is taking place asynchronously on the
  // DB sequence at the moment. Ongoing changes are still part of
  // `ongoing_profile_changes_` to prevent other changes from being scheduled.
  using QueuedAutofillProfileChange = std::pair<AutofillProfileChange, bool>;

  void CancelPendingQuery(WebDataServiceBase::Handle& handle);

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

  // Called when `prefs::kAutofillProfileEnabled` changed.
  void OnAutofillProfilePrefChanged();

  base::ObserverList<Observer> observers_;

  std::unique_ptr<ContactInfoPreconditionChecker>
      contact_info_precondition_checker_;

  WebDataServiceBase::Handle pending_profile_query_ = 0;

  // The WebDataService used to schedule tasks on the `AddressAutofillTable`.
  scoped_refptr<AutofillWebDataService> webdata_service_;

  // Used to check whether address Autofill is enabled. May be null in tests,
  // but must otherwise outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The identity manager that this instance uses. May be null in tests, but
  // must otherwise outlive this instance.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // May be null in tests, but must otherwise outlive this instance.
  raw_ptr<syncer::SyncService> sync_service_;

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

  // Used to populate AlternativeStateNameMap with the geographical state data
  // (including their abbreviations and localized names).
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;

  // The AddressDataCleaner is used to apply various cleanups (e.g.
  // deduplication, disused address removal) at browser startup or when the sync
  // starts.
  std::unique_ptr<AddressDataCleaner> address_data_cleaner_;

  // The list of change callbacks. All of them are being triggered in
  // `NotifyObservers()` and then the list is cleared.
  std::vector<base::OnceClosure> change_callbacks_;

  // If true, new addresses imports are automatically accepted without a prompt.
  // Only to be used for testing.
  bool auto_accept_address_imports_for_testing_ = false;

  const std::string app_locale_;

  base::WeakPtrFactory<AddressDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_
