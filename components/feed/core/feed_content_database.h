// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_CONTENT_DATABASE_H_
#define COMPONENTS_FEED_CORE_FEED_CONTENT_DATABASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace feed {

class ContentMutation;
class ContentOperation;
class ContentStorageProto;

using InitStatus = leveldb_proto::Enums::InitStatus;

// FeedContentDatabase is leveldb backend store for Feed's content storage data.
// Feed's content data are key-value pairs. In order to support callers from
// different threads, this class posts all database operations to an owned
// sequenced task runner.
class FeedContentDatabase {
 public:
  using KeyAndData = std::pair<std::string, std::string>;

  // Returns the storage data as a vector of key-value pairs when calling
  // loading data.
  using ContentLoadCallback =
      base::OnceCallback<void(bool, std::vector<KeyAndData>)>;

  // Returns the content keys as a vector when calling loading all content keys.
  using ContentKeyCallback =
      base::OnceCallback<void(bool, std::vector<std::string>)>;

  // Returns whether the commit operation succeeded when calling for database
  // operations, or return whether the entry exists when calling for checking
  // the entry's existence.
  using ConfirmationCallback = base::OnceCallback<void(bool)>;

  using StorageEntryVector =
      leveldb_proto::ProtoDatabase<ContentStorageProto>::KeyEntryVector;

  // Initializes the database with |proto_database_provider| and
  // |database_folder|.
  FeedContentDatabase(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& database_folder);

  // Creates storage using the given |storage_database| for local storage.
  // Useful for testing.
  explicit FeedContentDatabase(
      std::unique_ptr<leveldb_proto::ProtoDatabase<ContentStorageProto>>
          storage_database,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~FeedContentDatabase();

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may already started, or initialization
  // failed.
  bool IsInitialized() const;

  // Loads the content data for the |keys| and passes them to |callback|.
  void LoadContent(const std::vector<std::string>& keys,
                   ContentLoadCallback callback);

  // Loads the content data whose key matches |prefix|, and passes them to
  // |callback|.
  void LoadContentByPrefix(const std::string& prefix,
                           ContentLoadCallback callback);

  // Loads all content keys in the storage, and passes them to |callback|.
  void LoadAllContentKeys(ContentKeyCallback callback);

  // Commits the operations in the |content_mutation|. |callback| will be called
  // when all the operations are committed. Or if any operation failed, database
  // will stop process any operations and passed error to |callback|.
  void CommitContentMutation(std::unique_ptr<ContentMutation> content_mutation,
                             ConfirmationCallback callback);

 private:
  // These methods work with |CommitContentMutation|. They process
  // |ContentOperation| in |ContentMutation| which is passed to
  // |PerformNextOperation| by |CommitContentMutation|.
  void PerformNextOperation(std::unique_ptr<ContentMutation> content_mutation,
                            ConfirmationCallback callback);
  void UpsertContent(ContentOperation operation,
                     std::unique_ptr<ContentMutation> content_mutation,
                     ConfirmationCallback callback);
  void DeleteContent(ContentOperation operation,
                     std::unique_ptr<ContentMutation> content_mutation,
                     ConfirmationCallback callback);
  void DeleteContentByPrefix(ContentOperation operation,
                             std::unique_ptr<ContentMutation> content_mutation,
                             ConfirmationCallback callback);
  void DeleteAllContent(ContentOperation operation,
                        std::unique_ptr<ContentMutation> content_mutation,
                        ConfirmationCallback callback);

  // The following *Internal methods must be executed from |task_runner_|.
  void InitInternal();
  void LoadEntriesWithFilterInternal(const leveldb_proto::KeyFilter& key_filter,
                                     ContentLoadCallback callback);
  void LoadKeysInternal(ContentKeyCallback callback);
  void UpdateEntriesInternal(
      std::unique_ptr<StorageEntryVector> entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      std::unique_ptr<ContentMutation> content_mutation,
      ConfirmationCallback callback);
  void UpdateEntriesWithRemoveFilterInternal(
      const leveldb_proto::KeyFilter& key_filter,
      std::unique_ptr<ContentMutation> content_mutation,
      ConfirmationCallback callback);

  // Callback methods given to |storage_database_| for async responses.
  void OnDatabaseInitialized(InitStatus status);
  void OnLoadEntriesForLoadContent(
      base::TimeTicks start_time,
      ContentLoadCallback callback,
      bool success,
      std::unique_ptr<std::vector<ContentStorageProto>> content);
  void OnLoadKeysForLoadAllContentKeys(
      base::TimeTicks start_time,
      ContentKeyCallback callback,
      bool success,
      std::unique_ptr<std::vector<std::string>> keys);
  void OnOperationCommitted(std::unique_ptr<ContentMutation> content_mutation,
                            ConfirmationCallback callback,
                            bool success);

  // Status of the database initialization.
  InitStatus database_status_;

  // Task runner on which to execute database calls.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The database for storing content storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<ContentStorageProto>>
      storage_database_;

  base::WeakPtrFactory<FeedContentDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedContentDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_CONTENT_DATABASE_H_
