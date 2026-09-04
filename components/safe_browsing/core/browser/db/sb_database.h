// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_DATABASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_DATABASE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/sb_store.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

class SafeBrowsingServiceTest;
class TestSafeBrowsingDatabaseHelper;

// TODO(crbug.com/362791941): Handle references to v4.
// TODO(crbug.com/362791941): replace all |comments| with `comments` for v5.
namespace safe_browsing {

class SBDatabase;
class SBStoreFactory;

// Scheduled when the database has been read from disk and is ready to process
// resource reputation requests.
using NewDatabaseReadyCallback = base::OnceCallback<void(
    std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter>)>;

// Scheduled when the checksum for all the stores in the database has been
// verified to match the expected value. Stores for which the checksum did not
// match are passed as the argument and need to be reset.
using DatabaseReadyForUpdatesCallback =
    base::OnceCallback<void(const std::vector<ListIdentifier>&)>;

// This callback is scheduled once the database has finished processing the
// update requests for all stores and is ready to process the next set of update
// requests.
using DatabaseUpdatedCallback = base::RepeatingClosure;

// Maps the ListIdentifiers to their corresponding in-memory stores, which
// contain the hash prefixes for that ListIdentifier as well as manage their
// storage on disk.
using StoreMap = std::unordered_map<ListIdentifier, SBStorePtr>;

// Results and timings of a local database lookup.
struct DbLookupResult {
  // The matching full hashes and stores found in the local database.
  FullHashToStoreAndHashPrefixesMap results;

  // The timestamp when the lookup task was posted from the UI thread to the DB
  // thread. Used to calculate DB thread queue delay.
  base::TimeTicks db_thread_post_time;

  // The timestamp when the lookup task started executing on the DB thread.
  base::TimeTicks db_thread_start_time;

  // The timestamp when the lookup task completed execution on the DB thread.
  // Used to calculate UI thread return hop delay.
  base::TimeTicks db_thread_end_time;
};

// Factory for creating SBDatabase. Tests implement this factory to create fake
// databases for testing.
class SBDatabaseFactory {
 public:
  virtual ~SBDatabaseFactory() = default;
  virtual std::unique_ptr<SBDatabase, base::OnTaskRunnerDeleter> Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map);
};

// The on-disk databases are shared among all profiles, as it doesn't contain
// user-specific data. This object is not thread-safe, i.e. all its methods
// should be used on the same thread that it was created on, unless specified
// otherwise.
// The hash-prefixes of each type are managed by an SBStore (including saving to
// and reading from disk).
// The SBDatabase serves as a single place to manage all the SBStores.
class SBDatabase {
 public:
  // Factory method to create a SBDatabase. It creates the database on the
  // provided |db_task_runner| containing stores in |store_file_name_map|. When
  // the database creation is complete, it runs the NewDatabaseReadyCallback on
  // the same thread as it was called.
  // NOTE: Within |new_db_callback| the client should invoke
  // SBDatabase::InitializeOnUIThread() on the UI thread.
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfos& list_infos,
      NewDatabaseReadyCallback new_db_callback);

  // Initialize state that lives on the UI thread.
  void InitializeOnUIThread();

  // Destroy state that lives on the UI thread.
  void StopOnUIThread();

  SBDatabase(const SBDatabase&) = delete;
  SBDatabase& operator=(const SBDatabase&) = delete;

  virtual ~SBDatabase();

  // Updates the stores with the response received from the SafeBrowsing service
  // and calls the db_updated_callback when done.
  void ApplyUpdate(std::unique_ptr<SBUpdateResponseMap> update_map,
                   DatabaseUpdatedCallback db_updated_callback);

  // Returns the current state of each of the stores being managed.
  std::unique_ptr<StoreStateMap> GetStoreStateMap();

  // Check if all the selected stores are available and populated.
  // Returns false if any of |stores_to_check| don't have valid data.
  // A store may be unavailble if either it hasn't yet gotten a proper
  // full-update (just after install, or corrupted/missing file), or if it's
  // not supported in this build (i.e. Chromium).
  virtual bool AreAllStoresAvailable(
      const StoresToCheck& stores_to_check) const;

  // Check if any of the stores are available and populated.
  // Returns false if all of |stores_to_check| don't have valid data.
  virtual bool AreAnyStoresAvailable(
      const StoresToCheck& stores_to_check) const;

  // Searches for hash prefixes matching the |full_hashes| in stores in the
  // database, filtered by |stores_to_check|. The callback is run
  // asynchronously, with the identifier of the stores along with the matching
  // hash prefixes.
  virtual void GetStoresMatchingFullHash(
      const std::vector<FullHashStr>& full_hashes,
      const StoresToCheck& stores_to_check,
      base::OnceCallback<void(DbLookupResult)> callback);

  // Returns the file size of the store in bytes. Returns 0 if the store is not
  // found.
  virtual int64_t GetStoreSizeInBytes(const ListIdentifier& store) const;

  // Resets the stores in |stores_to_reset| to an empty state. This is done if
  // the checksum doesn't match the expected value.
  void ResetStores(const std::vector<ListIdentifier>& stores_to_reset);

  // Schedules verification of the checksum of each store read from disk on task
  // runner. If the checksum doesn't match, that store is passed to the
  // |db_ready_for_updates_callback|. At the end,
  // |db_ready_for_updates_callback| is scheduled (on the same thread as it was
  // called) to indicate that the database updates can now be scheduled.
  void VerifyChecksum(
      DatabaseReadyForUpdatesCallback db_ready_for_updates_callback);

  // Records the size of each of the stores managed by this database, along
  // with the combined size of all the stores.
  void RecordFileSizeHistograms();

  // Deletes any *.store* files in `base_path` that are not currently in use by
  // any of the active stores in this database.
  // `base_path`: The directory containing the store files.
  void DeleteUnusedStoreFiles(const base::FilePath& base_path);

  // Populates the DatabaseInfo message of the safe_browsing_page proto.
  void CollectDatabaseInfo(DatabaseManagerInfo::DatabaseInfo* database_info);

 protected:
  SBDatabase(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
             std::unique_ptr<StoreMap> store_map);

  // The collection of SBStores, keyed by ListIdentifier.
  // The map itself lives on the SBDatabase's parent thread, but its SBStore
  // objects live on the db_task_runner_thread.
  // TODO(vakh): Consider writing a container object which encapsulates or
  // harmonizes thread affinity for the associative container and the data.
  const std::unique_ptr<StoreMap> store_map_;

 private:
  friend class ::SafeBrowsingServiceTest;
  friend class ::TestSafeBrowsingDatabaseHelper;
  friend class SBDatabaseFactory;
  friend class SBEmbeddedTestServerBrowserTest;
  friend class SBDatabaseTest;
  friend class SBSafeBrowsingServiceTestBase;
  friend class SBSafeBrowsingServiceTest;
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest, TestSetupDatabaseWithFakeStores);
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest,
                           TestSetupDatabaseWithFakeStoresFailsReset);
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest, TestApplyUpdateWithNewStates);
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest, TestApplyUpdateWithNoNewState);
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest, TestApplyUpdateWithEmptyUpdate);
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest, TestApplyUpdateWithInvalidUpdate);
  FRIEND_TEST_ALL_PREFIXES(SBDatabaseTest, TestSomeStoresMatchFullHash);

  // Factory method to create a SBDatabase. When the database creation is
  // complete, it calls the NewDatabaseReadyCallback on |callback_task_runner|.
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfos& list_infos,
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback callback);

  // Makes the passed |factory| the factory used to instantiate a SBDatabase.
  // Only for tests.
  static void RegisterDatabaseFactoryForTest(
      std::unique_ptr<SBDatabaseFactory> factory);

  // Makes the passed |factory| the factory used to instantiate SBStores. Only
  // for tests.
  static void RegisterStoreFactoryForTest(
      std::unique_ptr<SBStoreFactory> factory);

  // Instantiates a store.
  static SBStorePtr CreateStore(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfo& list_info);

  // Callback called when a new store has been created and is ready to be used.
  // This method updates the store_map_ to point to the new store, which causes
  // the old store to get deleted.
  void UpdatedStoreReady(ListIdentifier identifier, SBStorePtr store);

  // See |VerifyChecksum|.
  void OnChecksumVerified(
      DatabaseReadyForUpdatesCallback db_ready_for_updates_callback,
      const std::vector<ListIdentifier>& stores_to_reset);

  bool IsStoreAvailable(const ListIdentifier& identifier) const;

  // Log the difference in time between database updates in a UMA histogram.
  void RecordDatabaseUpdateLatency();

  // Used to verify that certain methods are called on the UI thread.
  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  DatabaseUpdatedCallback db_updated_callback_;

  // The number of stores for which the update request is pending. When this
  // goes down to 0, that indicates that the database has updated all the stores
  // that needed updating and is ready for the next update. It should only be
  // accessed on the IO thread.
  int pending_store_updates_;

  // Variable used to keep track of latency of database updates.
  base::Time last_update_;

  // Only meant to be dereferenced and invalidated on the IO thread and hence
  // named. For details, see the comment at the top of weak_ptr.h
  base::WeakPtrFactory<SBDatabase> weak_factory_on_io_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_DATABASE_H_
