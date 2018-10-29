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
#include "components/leveldb_proto/proto_database.h"

namespace feed {

class ContentMutation;
class ContentOperation;
class ContentStorageProto;

// FeedContentDatabase is leveldb backend store for Feed's content storage data.
// Feed's content data are key-value pairs.
class FeedContentDatabase {
 public:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    INIT_FAILURE,
  };

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

  // Initializes the database with |database_folder|.
  explicit FeedContentDatabase(const base::FilePath& database_folder);

  // Initializes the database with |database_folder|. Creates storage using the
  // given |storage_database| for local storage. Useful for testing.
  FeedContentDatabase(
      const base::FilePath& database_folder,
      std::unique_ptr<leveldb_proto::ProtoDatabase<ContentStorageProto>>
          storage_database);

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

  // Callback methods given to |storage_database_| for async responses.
  void OnDatabaseInitialized(bool success);
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
  State database_status_;

  // The database for storing content storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<ContentStorageProto>>
      storage_database_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FeedContentDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedContentDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_CONTENT_DATABASE_H_
