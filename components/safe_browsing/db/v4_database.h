// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/db/v4_store.h"
#include "components/safe_browsing/proto/webui.pb.h"

class TestSafeBrowsingDatabaseHelper;

namespace safe_browsing {

class V4Database;

// Scheduled when the database has been read from disk and is ready to process
// resource reputation requests.
using NewDatabaseReadyCallback =
    base::Callback<void(std::unique_ptr<V4Database>)>;

// Scheduled when the checksum for all the stores in the database has been
// verified to match the expected value. Stores for which the checksum did not
// match are passed as the argument and need to be reset.
using DatabaseReadyForUpdatesCallback =
    base::Callback<void(const std::vector<ListIdentifier>&)>;

// This callback is scheduled once the database has finished processing the
// update requests for all stores and is ready to process the next set of update
// requests.
using DatabaseUpdatedCallback = base::Closure;

// Maps the ListIdentifiers to their corresponding in-memory stores, which
// contain the hash prefixes for that ListIdentifier as well as manage their
// storage on disk.
using StoreMap = std::unordered_map<ListIdentifier, std::unique_ptr<V4Store>>;

// Associates metadata for a list with its ListIdentifier.
class ListInfo {
 public:
  ListInfo(const bool fetch_updates,
           const std::string& filename,
           const ListIdentifier& list_id,
           const SBThreatType sb_threat_type);
  ~ListInfo();

  const ListIdentifier& list_id() const { return list_id_; }
  const std::string& filename() const { return filename_; }
  SBThreatType sb_threat_type() const { return sb_threat_type_; }
  bool fetch_updates() const { return fetch_updates_; }

 private:
  // Whether to fetch and store updates for this list.
  bool fetch_updates_;

  // The ASCII name of the file on disk. This file is created inside the
  // user-data directory. For instance, the ListIdentifier could be for URL
  // expressions for UwS on Windows platform, and the corresponding file on disk
  // could be named: "UrlUws.store"
  std::string filename_;

  // The list being read from/written to the disk.
  ListIdentifier list_id_;

  // The threat type enum value for this store.
  SBThreatType sb_threat_type_;

  ListInfo() = delete;
};

using ListInfos = std::vector<ListInfo>;

// Factory for creating V4Database. Tests implement this factory to create fake
// databases for testing.
class V4DatabaseFactory {
 public:
  virtual ~V4DatabaseFactory() {}
  virtual std::unique_ptr<V4Database> Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      std::unique_ptr<StoreMap> store_map);
};

// The on-disk databases are shared among all profiles, as it doesn't contain
// user-specific data. This object is not thread-safe, i.e. all its methods
// should be used on the same thread that it was created on, unless specified
// otherwise.
// The hash-prefixes of each type are managed by a V4Store (including saving to
// and reading from disk).
// The V4Database serves as a single place to manage all the V4Stores.
class V4Database {
 public:
  // Factory method to create a V4Database. It creates the database on the
  // provided |db_task_runner| containing stores in |store_file_name_map|. When
  // the database creation is complete, it runs the NewDatabaseReadyCallback on
  // the same thread as it was called.
  static void Create(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfos& list_infos,
      NewDatabaseReadyCallback new_db_callback);

  // Destroys the provided v4_database on its task_runner since this may be a
  // long operation.
  static void Destroy(std::unique_ptr<V4Database> v4_database);

  virtual ~V4Database();

  // Updates the stores with the response received from the SafeBrowsing service
  // and calls the db_updated_callback when done.
  void ApplyUpdate(std::unique_ptr<ParsedServerResponse> parsed_server_response,
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

  // Searches for a hash prefix matching the |full_hash| in stores in the
  // database, filtered by |stores_to_check|, and returns the identifier of the
  // store along with the matching hash prefix in |matched_hash_prefix_map|.
  virtual void GetStoresMatchingFullHash(
      const FullHash& full_hash,
      const StoresToCheck& stores_to_check,
      StoreAndHashPrefixes* matched_store_and_full_hashes);

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

  // Populates the DatabaseInfo message of the safe_browsing_page proto.
  void CollectDatabaseInfo(DatabaseManagerInfo::DatabaseInfo* database_info);

 protected:
  V4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
             std::unique_ptr<StoreMap> store_map);

  // The collection of V4Stores, keyed by ListIdentifier.
  // The map itself lives on the V4Database's parent thread, but its V4Store
  // objects live on the db_task_runner_thread.
  // TODO(vakh): Consider writing a container object which encapsulates or
  // harmonizes thread affinity for the associative container and the data.
  const std::unique_ptr<StoreMap> store_map_;

 private:
  friend class ::TestSafeBrowsingDatabaseHelper;
  friend class V4DatabaseFactory;
  friend class V4EmbeddedTestServerBrowserTest;
  friend class V4DatabaseTest;
  friend class V4SafeBrowsingServiceTest;
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestSetupDatabaseWithFakeStores);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest,
                           TestSetupDatabaseWithFakeStoresFailsReset);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithNewStates);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithNoNewState);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithEmptyUpdate);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestApplyUpdateWithInvalidUpdate);
  FRIEND_TEST_ALL_PREFIXES(V4DatabaseTest, TestSomeStoresMatchFullHash);

  // Factory method to create a V4Database. When the database creation is
  // complete, it calls the NewDatabaseReadyCallback on |callback_task_runner|.
  static void CreateOnTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfos& list_infos,
      const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
      NewDatabaseReadyCallback callback);

  // Makes the passed |factory| the factory used to instantiate a V4Database.
  // Only for tests.
  static void RegisterDatabaseFactoryForTest(
      std::unique_ptr<V4DatabaseFactory> factory);

  // Makes the passed |factory| the factory used to instantiate a V4Store. Only
  // for tests.
  static void RegisterStoreFactoryForTest(
      std::unique_ptr<V4StoreFactory> factory);

  // Callback called when a new store has been created and is ready to be used.
  // This method updates the store_map_ to point to the new store, which causes
  // the old store to get deleted.
  void UpdatedStoreReady(ListIdentifier identifier,
                         std::unique_ptr<V4Store> store);

  // See |VerifyChecksum|.
  void OnChecksumVerified(
      DatabaseReadyForUpdatesCallback db_ready_for_updates_callback,
      const std::vector<ListIdentifier>& stores_to_reset);

  bool IsStoreAvailable(const ListIdentifier& identifier) const;

  const scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  DatabaseUpdatedCallback db_updated_callback_;

  // The number of stores for which the update request is pending. When this
  // goes down to 0, that indicates that the database has updated all the stores
  // that needed updating and is ready for the next update. It should only be
  // accessed on the IO thread.
  int pending_store_updates_;

  // Only meant to be dereferenced and invalidated on the IO thread and hence
  // named. For details, see the comment at the top of weak_ptr.h
  base::WeakPtrFactory<V4Database> weak_factory_on_io_{this};

  DISALLOW_COPY_AND_ASSIGN(V4Database);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_DATABASE_H_
