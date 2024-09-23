// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_LEVELDB_SITE_DATA_STORE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_LEVELDB_SITE_DATA_STORE_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/persistence/site_data/site_data_store.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace performance_manager {

// Manages a LevelDB database used by a site data store.
// TODO(sebmarchand):
//   - Constrain the size of the database: Use a background task to trim the
//     database if it becomes too big and ensure that this fails nicely when the
//     disk is full.
//   - Batch the write operations to reduce the number of I/O events.
//
// All the DB operations are done asynchronously on a sequence allowed to do
// I/O operations.
class LevelDBSiteDataStore : public SiteDataStore {
 public:
  explicit LevelDBSiteDataStore(const base::FilePath& db_path);

  LevelDBSiteDataStore(const LevelDBSiteDataStore&) = delete;
  LevelDBSiteDataStore& operator=(const LevelDBSiteDataStore&) = delete;

  ~LevelDBSiteDataStore() override;

  // SiteDataStore:
  void ReadSiteDataFromStore(
      const url::Origin& origin,
      SiteDataStore::ReadSiteDataFromStoreCallback callback) override;
  void WriteSiteDataIntoStore(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto) override;
  void RemoveSiteDataFromStore(
      const std::vector<url::Origin>& site_origins) override;
  void ClearStore() override;
  void GetStoreSize(GetStoreSizeCallback callback) override;
  void SetInitializationCallbackForTesting(base::OnceClosure callback) override;

  void DatabaseIsInitializedForTesting(base::OnceCallback<void(bool)> reply_cb);

  // Run a task against the raw DB implementation and wait for it to complete.
  void RunTaskWithRawDBForTesting(base::OnceCallback<void(leveldb::DB*)> task,
                                  base::OnceClosure after_task_run_closure);

  // Make the new instances of this class use an in memory database rather than
  // creating it on disk. When the ScopedClosureRunner goes out of scope, it
  // will stop using an in-memory database.
  static base::ScopedClosureRunner UseInMemoryDBForTesting();

  static const size_t kDbVersion;
  static const char kDbMetadataKey[];

 private:
  class AsyncHelper;

  // The task runner used to run all the blocking operations.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Helper object that should be used to trigger all the operations that need
  // to run on |blocking_task_runner_|, it is guaranteed that the AsyncHelper
  // held by this object will only be destructed once all the tasks that have
  // been posted to it have completed.
  std::unique_ptr<AsyncHelper, base::OnTaskRunnerDeleter> async_helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_LEVELDB_SITE_DATA_STORE_H_
