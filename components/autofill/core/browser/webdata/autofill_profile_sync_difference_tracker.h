// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNC_DIFFERENCE_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNC_DIFFERENCE_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"

namespace syncer {
class ModelError;
}  // namespace syncer

namespace autofill {

class AutofillProfile;
class AutofillProfileComparator;
class AutofillTable;

// This is used to respond to ApplySyncChanges() and MergeSyncData(). Attempts
// to lazily load local data, and then react to sync data by maintaining
// internal state until flush calls are made, at which point the applicable
// modification should be sent towards local and sync directions.
class AutofillProfileSyncDifferenceTracker {
 public:
  explicit AutofillProfileSyncDifferenceTracker(AutofillTable* table);
  virtual ~AutofillProfileSyncDifferenceTracker();

  // Adds a new |remote| entry to the diff tracker, originating from the sync
  // server. The provided |remote| entry must be valid.
  base::Optional<syncer::ModelError> IncorporateRemoteProfile(
      std::unique_ptr<AutofillProfile> remote);

  // Informs the diff tracker that the entry with |storage_key| has been deleted
  // from the sync server. |storage_key| must be non-empty.
  virtual base::Optional<syncer::ModelError> IncorporateRemoteDelete(
      const std::string& storage_key);

  // Writes all local changes to the provided autofill |table_|. After flushing,
  // not further remote changes should get incorporated.
  base::Optional<syncer::ModelError> FlushToLocal(
      base::OnceClosure autofill_changes_callback);

  // Writes into |profiles_to_upload_to_sync| all autofill profiles to be sent
  // to the sync server, and into |profiles_to_delete_from_sync| the storage
  // keys of all profiles to be deleted from the server. After flushing, no
  // further remote changes should get incorporated.
  virtual base::Optional<syncer::ModelError> FlushToSync(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles_to_upload_to_sync,
      std::vector<std::string>* profiles_to_delete_from_sync);

 protected:
  // If the entry is found, |entry| will be return, otherwise base::nullopt is
  // returned.
  base::Optional<AutofillProfile> ReadEntry(const std::string& storage_key);

  // Tries to find a local entry that is mergeable with |remote| (according to
  // |comparator|). If such an entry is found, it is returned. Otherwise,
  // base::nullopt is returned.
  base::Optional<AutofillProfile> FindMergeableLocalEntry(
      const AutofillProfile& remote,
      const AutofillProfileComparator& comparator);

  // Informs the tracker that a local entry with |storage_key| should get
  // deleted.
  void DeleteFromLocal(const std::string& storage_key);

  // Accessor for data that is only stored local. Initializes the data if
  // needed. Returns nullptr if initialization failed.
  std::map<std::string, std::unique_ptr<AutofillProfile>>*
  GetLocalOnlyEntries();

  // Helper function called by GetLocalOnlyEntries().
  bool InitializeLocalOnlyEntriesIfNeeded();

  // The table for reading local data.
  AutofillTable* const table_;

  // This class loads local data from |table_| lazily. This field tracks if that
  // has happened or not yet.
  bool local_only_entries_initialized_ = false;

  // We use unique_ptrs for storing AutofillProfile to avoid unnecessary copies.

  // Local data, mapped by storage key. Use GetLocalOnlyEntries() to access it.
  std::map<std::string, std::unique_ptr<AutofillProfile>> local_only_entries_;

  // Contain changes (originating from sync) that need to be saved to the local
  // store.
  std::set<std::string> delete_from_local_;
  std::vector<std::unique_ptr<AutofillProfile>> add_to_local_;
  std::vector<std::unique_ptr<AutofillProfile>> update_to_local_;

  // Contains data for entries that existed on both sync and local sides
  // and need to be saved back to sync.
  std::vector<std::unique_ptr<AutofillProfile>> save_to_sync_;
  // Contains storage keys for entries that existed on both sync and local
  // sides and need to be deleted from sync (because the conflict resolution
  // preferred the local copies).
  std::set<std::string> delete_from_sync_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncDifferenceTracker);
};

class AutofillProfileInitialSyncDifferenceTracker
    : public AutofillProfileSyncDifferenceTracker {
 public:
  explicit AutofillProfileInitialSyncDifferenceTracker(AutofillTable* table);
  ~AutofillProfileInitialSyncDifferenceTracker() override;

  base::Optional<syncer::ModelError> IncorporateRemoteDelete(
      const std::string& storage_key) override;

  base::Optional<syncer::ModelError> FlushToSync(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles_to_upload_to_sync,
      std::vector<std::string>* profiles_to_delete_from_sync) override;

  // Performs an additional pass through remote entries incorporated from sync
  // to find any similarities with local entries. Should be run after all
  // entries get incorporated but before flushing results to local/sync.
  base::Optional<syncer::ModelError> MergeSimilarEntriesForInitialSync(
      const std::string& app_locale);

 private:
  // Returns a local entry that is mergeable with |remote| if it exists.
  // Otherwise, returns base::nullopt.
  base::Optional<AutofillProfile> FindMergeableLocalEntry(
      const AutofillProfile& remote,
      const AutofillProfileComparator& comparator);

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileInitialSyncDifferenceTracker);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNC_DIFFERENCE_TRACKER_H_
