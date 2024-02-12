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
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class PersonalDataManager;

// Intended to contain all address-related logic of the `PersonalDataManager`.
// Owned by the PDM.
// TODO(b/322170538): Move all address-related logic from the PDM to this file.
class AddressDataManager : public WebDataServiceConsumer {
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
                     base::RepeatingClosure notify_pdm_observers,
                     const std::string& app_locale);

  ~AddressDataManager() override;
  AddressDataManager(const AddressDataManager&) = delete;
  AddressDataManager& operator=(const AddressDataManager&) = delete;

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

  void CancelAllPendingQueries() {
    CancelPendingQuery(pending_synced_local_profiles_query_);
    CancelPendingQuery(pending_account_profiles_query_);
  }

  bool HasPendingQueries() const {
    return pending_synced_local_profiles_query_ ||
           pending_account_profiles_query_;
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

  // TODO(b/322170538): Remove once the PDM observer is split.
  base::RepeatingClosure notify_pdm_observers_;

  // Tracks whether the first `LoadProfiles()` call has already finished.
  bool has_initial_load_finished_ = false;

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

  // Finds the country code that occurs most frequently among all profiles.
  // Prefers verified profiles over unverified ones.
  std::string MostCommonCountryCodeFromProfiles() const;

  // Logs metrics around the number of stored profiles after the initial load
  // has finished.
  void LogStoredDataMetrics() const;

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

  // A timely ordered list of ongoing changes for each profile.
  std::unordered_map<std::string, std::deque<QueuedAutofillProfileChange>>
      ongoing_profile_changes_;

  const std::string app_locale_;

  base::WeakPtrFactory<AddressDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_DATA_MANAGER_H_
