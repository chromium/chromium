// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_
#define COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace feed {

class JournalMutation;
class JournalOperation;
class JournalStorageProto;

using InitStatus = leveldb_proto::Enums::InitStatus;

// FeedJournalDatabase is leveldb backend store for Feed's journal storage data.
// Feed's journal data are key-value pairs.  In order to support callers from
// different threads, this class posts all database operations to an owned
// sequenced task runner.
class FeedJournalDatabase {
 public:
  // Returns the journal data as a vector of strings when calling loading data
  // or keys.
  using JournalLoadCallback =
      base::OnceCallback<void(bool, std::vector<std::string>)>;

  // Return whether the entry exists when calling for checking
  // the entry's existence.
  using CheckExistingCallback = base::OnceCallback<void(bool, bool)>;

  // Returns whether the commit operation succeeded when calling for database
  // operations.
  using ConfirmationCallback = base::OnceCallback<void(bool)>;

  using JournalMap = base::flat_map<std::string, JournalStorageProto>;

  using StorageEntryVector =
      leveldb_proto::ProtoDatabase<JournalStorageProto>::KeyEntryVector;

  // Initializes the database with |proto_database_provider| and
  // |database_folder|.
  FeedJournalDatabase(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& database_folder);

  // Creates storage using the given |storage_database| for local storage.
  // Useful for testing.
  explicit FeedJournalDatabase(
      std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
          storage_database,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~FeedJournalDatabase();

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may already started, or initialization
  // failed.
  bool IsInitialized() const;

  // Loads the journal data for the |key| and passes it to |callback|.
  void LoadJournal(const std::string& key, JournalLoadCallback callback);

  // Checks if the journal for the |key| exists, and return the result to
  // |callback|.
  void DoesJournalExist(const std::string& key, CheckExistingCallback callback);

  // Commits the operations in the |journal_mutation|. |callback| will be called
  // when all the operations are committed. Or if any operation failed, database
  // will stop process any operations and passed error to |callback|.
  void CommitJournalMutation(std::unique_ptr<JournalMutation> journal_mutation,
                             ConfirmationCallback callback);

  // Loads all journal keys in the storage, and passes them to |callback|.
  void LoadAllJournalKeys(JournalLoadCallback callback);

  // Delete all journals, |callback| will be called when all journals are
  // deleted or if there is an error.
  void DeleteAllJournals(ConfirmationCallback callback);

 private:
  // This method performs JournalOperation in the |journal_mutation|.
  // If the first operation in |journal_mutation| is JOURNAL_DELETE, journal
  // can be empty, otherwise we need to load |journal| from database and
  // then pass to this method.
  void PerformOperations(std::unique_ptr<JournalStorageProto> journal,
                         std::unique_ptr<JournalMutation> journal_mutation,
                         ConfirmationCallback callback);
  void CommitOperations(base::TimeTicks start_time,
                        std::unique_ptr<JournalStorageProto> journal,
                        JournalMap copy_to_journal,
                        ConfirmationCallback callback);

  // The following *Internal methods must be executed from |task_runner_|.
  void InitInternal();
  void GetEntryInternal(
      const std::string& key,
      leveldb_proto::Callbacks::Internal<JournalStorageProto>::GetCallback
          callback);
  void LoadKeysInternal(JournalLoadCallback callback);
  void DeleteAllEntriesInternal(ConfirmationCallback callback);
  void UpdateEntriesInternal(
      std::unique_ptr<StorageEntryVector> entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      base::TimeTicks start_time,
      ConfirmationCallback callback);

  // Callback methods given to |storage_database_| for async responses.
  void OnDatabaseInitialized(InitStatus status);
  void OnGetEntryForLoadJournal(base::TimeTicks start_time,
                                JournalLoadCallback callback,
                                bool success,
                                std::unique_ptr<JournalStorageProto> journal);
  void OnGetEntryForDoesJournalExist(
      base::TimeTicks start_time,
      CheckExistingCallback callback,
      bool success,
      std::unique_ptr<JournalStorageProto> journal);
  void OnGetEntryForCommitJournalMutation(
      std::unique_ptr<JournalMutation> journal_mutation,
      ConfirmationCallback callback,
      bool success,
      std::unique_ptr<JournalStorageProto> journal);
  void OnLoadKeysForLoadAllJournalKeys(
      base::TimeTicks start_time,
      JournalLoadCallback callback,
      bool success,
      std::unique_ptr<std::vector<std::string>> keys);
  void OnOperationCommitted(base::TimeTicks start_time,
                            ConfirmationCallback callback,
                            bool success);

  JournalStorageProto CopyJournal(const std::string& new_journal_name,
                                  const JournalStorageProto& source_journal);

  // Status of the database initialization.
  InitStatus database_status_;

  // Task runner on which to execute database calls.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The database for storing journal storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
      storage_database_;

  base::WeakPtrFactory<FeedJournalDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedJournalDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_
