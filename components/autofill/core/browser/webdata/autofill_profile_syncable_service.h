// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/autofill_specifics.pb.h"

namespace browser_sync {
class ProfileSyncServiceAutofillTest;
}  // namespace browser_sync

namespace autofill {

class AutofillProfile;
class AutofillTable;
class AutofillWebDataService;

extern const char kAutofillProfileTag[];

// The sync implementation for local AutofillProfiles, which can be managed in
// settings and can be written to the sync server. (Server profiles cannot be
// managed in settings and can only be read from the sync server.)
//
// MergeDataAndStartSyncing() called first, it does cloud->local and
// local->cloud syncs. Then for each cloud change we receive
// ProcessSyncChanges() and for each local change Observe() is called.
class AutofillProfileSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService,
      public AutofillWebDataServiceObserverOnDBSequence {
 public:
  ~AutofillProfileSyncableService() override;

  // Creates a new AutofillProfileSyncableService and hangs it off of
  // |web_data_service|, which takes ownership. This method should only be
  // called on |web_data_service|'s DB sequence.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* webdata_backend,
      const std::string& app_locale);

  // Retrieves the AutofillProfileSyncableService stored on |web_data_service|.
  static AutofillProfileSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

  static syncer::ModelType model_type() { return syncer::AUTOFILL_PROFILE; }

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;

  // Provides a StartSyncFlare to the SyncableService. See
  // sync_start_util for more.
  void InjectStartSyncFlare(
      const syncer::SyncableService::StartSyncFlare& flare);

 protected:
  AutofillProfileSyncableService(AutofillWebDataBackend* webdata_backend,
                                 const std::string& app_locale);

  // A convenience wrapper of a bunch of state we pass around while
  // associating models, and send to the WebDatabase for persistence.
  // We do this so we hold the write lock for only a small period.
  // When storing the web db we are out of the write lock.
  struct DataBundle;

  // Helper to query WebDatabase for the current autofill state.
  // Made virtual for ease of mocking in unit tests.
  virtual bool LoadAutofillData(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles);

  // Helper to persist any changes that occured during model association to
  // the WebDatabase.
  // Made virtual for ease of mocking in unit tests.
  virtual bool SaveChangesToWebData(const DataBundle& bundle);

  // For unit tests.
  AutofillProfileSyncableService();
  void set_sync_processor(syncer::SyncChangeProcessor* sync_processor);

  // Creates syncer::SyncData based on supplied |profile|.
  // Exposed for unit tests.
  static syncer::SyncData CreateData(const AutofillProfile& profile);

 private:
  friend class browser_sync::ProfileSyncServiceAutofillTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           UpdateField);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeSimilarProfiles_AdditionalInfoInBothProfiles);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeSimilarProfiles_DifferentUseDates);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeSimilarProfiles_DifferentNames);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeSimilarProfiles_NonZeroUseCounts);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           OverwriteProfileWithServerData_NonSettingsOrigin);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           OverwriteProfileWithServerData_SettingsOrigin);

  // The map of the guid to profiles owned by the |profiles_| vector.
  typedef std::map<std::string, AutofillProfile*> GUIDToProfileMap;

  // Helper function that overwrites |profile| with data from proto-buffer
  // |specifics|.
  static bool OverwriteProfileWithServerData(
      const sync_pb::AutofillProfileSpecifics& specifics,
      AutofillProfile* profile);

  // Writes |profile| data into supplied |profile_specifics|.
  static void WriteAutofillProfile(const AutofillProfile& profile,
                                   sync_pb::EntitySpecifics* profile_specifics);

  // Creates |profile_map| from the supplied |profiles| vector. Necessary for
  // fast processing of the changes.
  void CreateGUIDToProfileMap(
      const std::vector<std::unique_ptr<AutofillProfile>>& profiles,
      GUIDToProfileMap* profile_map);

  // Creates or updates a profile based on |data|. Looks at the guid of the data
  // and if a profile with such guid is present in |profile_map| updates it. If
  // not, searches through it for similar profiles. If similar profile is
  // found substitutes it for the new one, otherwise adds a new profile. Returns
  // iterator pointing to added/updated profile.
  GUIDToProfileMap::iterator CreateOrUpdateProfile(
      const syncer::SyncData& data,
      GUIDToProfileMap* profile_map,
      DataBundle* bundle);

  // Syncs |change| to the cloud.
  void ActOnChange(const AutofillProfileChange& change);

  AutofillTable* GetAutofillTable() const;

  // Helper to compare the local value and cloud value of a field, copy into
  // the local value if they differ, and return whether the change happened.
  static bool UpdateField(ServerFieldType field_type,
                          const std::string& new_value,
                          AutofillProfile* autofill_profile);

  // Calls merge_into->OverwriteWith() and then checks if the
  // |merge_into| has extra data. Returns true if the merge has made a change to
  // |merge_into|. This should be used only for similar profiles ie. profiles
  // where the PrimaryValue() matches.
  static bool MergeSimilarProfiles(const AutofillProfile& merge_from,
                                   AutofillProfile* merge_into,
                                   const std::string& app_locale);

  AutofillWebDataBackend* webdata_backend_;  // WEAK
  std::string app_locale_;
  ScopedObserver<AutofillWebDataBackend,
                 AutofillProfileSyncableService> scoped_observer_;

  // Cached Autofill profiles. *Warning* deleted profiles are still in the
  // vector - use the |profiles_map_| to iterate through actual profiles.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_;
  GUIDToProfileMap profiles_map_;

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  syncer::SyncableService::StartSyncFlare flare_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncableService);
};

// This object is used in unit tests as well, so it defined here.
struct AutofillProfileSyncableService::DataBundle {
  DataBundle();
  DataBundle(const DataBundle& other);
  ~DataBundle();

  std::vector<std::string> profiles_to_delete;
  std::vector<AutofillProfile*> profiles_to_update;
  std::vector<AutofillProfile*> profiles_to_add;

  // When we go through sync we find profiles that are similar but unmatched.
  // Merge such profiles.
  GUIDToProfileMap candidates_to_merge;
  // Profiles that have multi-valued fields that are not in sync.
  std::vector<AutofillProfile*> profiles_to_sync_back;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
