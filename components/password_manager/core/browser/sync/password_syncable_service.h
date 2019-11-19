// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNCABLE_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNCABLE_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace autofill {
struct PasswordForm;
}

namespace syncer {
class SyncErrorFactory;
}

namespace password_manager {

class PasswordStoreSync;

// The implementation of the SyncableService API for passwords.
class PasswordSyncableService : public syncer::SyncableService {
 public:
  // Since the constructed |PasswordSyncableService| is typically owned by the
  // |password_store|, the constructor doesn't take ownership of the
  // |password_store|.
  explicit PasswordSyncableService(PasswordStoreSync* password_store);
  ~PasswordSyncableService() override;

  // syncer::SyncableService:
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // Notifies the Sync engine of changes to the password database.
  void ActOnPasswordStoreChanges(const PasswordStoreChangeList& changes);

  // Provides a StartSyncFlare to the SyncableService. See
  // chrome/browser/sync/glue/sync_start_util.h for more.
  void InjectStartSyncFlare(
      const syncer::SyncableService::StartSyncFlare& flare);

 private:
  // Map from password sync tag to password form.
  typedef std::map<std::string, autofill::PasswordForm*> PasswordEntryMap;

  struct SyncEntries;

  // Retrieves the entries from password db and fills both |password_entries|
  // and |passwords_entry_map|. |passwords_entry_map| can be NULL.
  bool ReadFromPasswordStore(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* password_entries,
      PasswordEntryMap* passwords_entry_map) const;

  // Uses the |PasswordStore| APIs to change entries.
  void WriteToPasswordStore(const SyncEntries& entries, bool is_merge);

  // Examines |data|, an entry in sync db, and updates |sync_entries| or
  // |updated_db_entries| accordingly. An element is removed from
  // |unmatched_data_from_password_db| if its tag is identical to |data|'s.
  static void CreateOrUpdateEntry(
      const syncer::SyncData& data,
      PasswordEntryMap* unmatched_data_from_password_db,
      SyncEntries* sync_entries,
      syncer::SyncChangeList* updated_db_entries);

  // Returns true if corrupted passwords should be deleted from the local
  // database when merging data.
  // This is true if the feature DeleteCorruptedPasswords is disabled, as it
  // recovers both Sync and non-Sync users internally in LoginDatabase.
  bool ShouldRecoverPasswordsDuringMerge() const;

  // The factory that creates sync errors. |SyncError| has rich data
  // suitable for debugging.
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  // |sync_processor_| will mirror the |PasswordStore| changes in the sync db.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // The password store that adds/updates/deletes password entries. Not owned.
  PasswordStoreSync* const password_store_;

  // A signal activated by this class to start sync as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  // True if processing sync changes is in progress.
  bool is_processing_sync_changes_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PasswordSyncableService);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_SYNCABLE_SERVICE_H_
