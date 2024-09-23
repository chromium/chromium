// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/service_worker_database.mojom.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/navigation_preload_state.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Location;
}

namespace leveldb {
class DB;
class Env;
class WriteBatch;
}  // namespace leveldb

namespace storage {

// Class to persist serviceworker registration data in a database.
// Should NOT be used on the IO thread since this does blocking
// file io. The ServiceWorkerStorage class owns this class and
// is responsible for only calling it serially on background
// non-IO threads (ala SequencedWorkerPool).
// TODO(crbug.com/40103973): Update the above comments once the instance of this
// class lives in the Storage Service.
class ServiceWorkerDatabase {
 public:
  // We do leveldb stuff in |path| or in memory if |path| is empty.
  explicit ServiceWorkerDatabase(const base::FilePath& path);

  ServiceWorkerDatabase(const ServiceWorkerDatabase&) = delete;
  ServiceWorkerDatabase& operator=(const ServiceWorkerDatabase&) = delete;

  ~ServiceWorkerDatabase();

  using Status = mojom::ServiceWorkerDatabaseStatus;

  static const char* StatusToString(Status status);

  using FeatureToTokensMap =
      base::flat_map<std::string /* feature_name */,
                     std::vector<std::string /* token */>>;

  // Contains information of a deleted service worker version. Used as an output
  // of WriteRegistration() and DeleteRegistration().
  struct DeletedVersion {
    int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
    int64_t version_id = blink::mojom::kInvalidServiceWorkerVersionId;
    uint64_t resources_total_size_bytes = 0;
    std::vector<int64_t /*=resource_id*/> newly_purgeable_resources;

    DeletedVersion();
    DeletedVersion(const DeletedVersion&);
    ~DeletedVersion();
  };

  // Reads next available ids from the database. Returns OK if they are
  // successfully read. Fills the arguments with an initial value and returns
  // OK if they are not found in the database. Otherwise, returns an error.
  Status GetNextAvailableIds(int64_t* next_avail_registration_id,
                             int64_t* next_avail_version_id,
                             int64_t* next_avail_resource_id);

  // Reads keys that have one or more registration from the
  // database. Returns OK if they are successfully read or not found.
  // Otherwise, returns an error.
  Status GetStorageKeysWithRegistrations(std::set<blink::StorageKey>* key);

  // Reads registrations for |key| from the database. Returns OK if they are
  // successfully read or not found. Otherwise, returns an error.
  Status GetRegistrationsForStorageKey(
      const blink::StorageKey& key,
      std::vector<mojom::ServiceWorkerRegistrationDataPtr>* registrations,
      std::vector<std::vector<mojom::ServiceWorkerResourceRecordPtr>>*
          opt_resources_list);

  // Reads the total resource size stored in the database for |key|.
  Status GetUsageForStorageKey(const blink::StorageKey& key,
                               int64_t& out_usage);

  // Reads all registrations from the database. Returns OK if successfully read
  // or not found. Otherwise, returns an error.
  Status GetAllRegistrations(
      std::vector<mojom::ServiceWorkerRegistrationDataPtr>* registrations);

  // Saving, retrieving, and updating registration data.
  // (will bump next_avail_xxxx_ids as needed)
  // (resource ids will be added/removed from the uncommitted/purgeable
  // lists as needed)

  // Reads a registration for |registration_id| and resource records associated
  // with it from the database. Returns OK if they are successfully read.
  // Otherwise, returns an error.
  Status ReadRegistration(
      int64_t registration_id,
      const blink::StorageKey& key,
      mojom::ServiceWorkerRegistrationDataPtr* registration,
      std::vector<mojom::ServiceWorkerResourceRecordPtr>* resources);

  // Looks up the key for the registration with |registration_id|. Returns OK
  // if a registration was found and read successfully. Otherwise, returns an
  // error.
  Status ReadRegistrationStorageKey(int64_t registration_id,
                                    blink::StorageKey* key);

  // Writes |registration| and |resources| into the database and does following
  // things:
  //   - If an old version of the registration exists, deletes it and fills
  //     |deleted_version| with the old version registration data object.
  //     Otherwise, sets |deleted_version->version_id| to -1.
  //   - Bumps the next registration id and the next version id if needed.
  //   - Removes |resources| from the uncommitted list if exist.
  // Returns OK they are successfully written. Otherwise, returns an error.
  Status WriteRegistration(
      const mojom::ServiceWorkerRegistrationData& registration,
      const std::vector<mojom::ServiceWorkerResourceRecordPtr>& resources,
      DeletedVersion* deleted_version);

  // Updates a registration for |registration_id| to an active state. Returns OK
  // if it's successfully updated. Otherwise, returns an error.
  Status UpdateVersionToActive(int64_t registration_id,
                               const blink::StorageKey& key);

  // Updates last check time of a registration for |registration_id| by |time|.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateLastCheckTime(int64_t registration_id,
                             const blink::StorageKey& key,
                             const base::Time& time);

  // Updates the navigation preload state for the specified registration.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateNavigationPreloadEnabled(int64_t registration_id,
                                        const blink::StorageKey& key,
                                        bool enable);
  Status UpdateNavigationPreloadHeader(int64_t registration_id,
                                       const blink::StorageKey& key,
                                       const std::string& value);
  // Updates a fetch handler type for the specified registration.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateFetchHandlerType(
      int64_t registration_id,
      const blink::StorageKey& key,
      const blink::mojom::ServiceWorkerFetchHandlerType type);

  // Updates script resource records for the specified registration.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateResourceSha256Checksums(
      int64_t registration_id,
      const blink::StorageKey& key,
      const base::flat_map<int64_t, std::string>& updated_sha256_checksums);

  // Deletes a registration for |registration_id| and moves resource records
  // associated with it into the purgeable list. If deletion occurred, fills
  // |deleted_version| with the version that was deleted; otherwise, sets
  // |deleted_version->version_id| to -1.
  // Returns OK if it's successfully deleted or not found in the database.
  // Otherwise, returns an error.
  Status DeleteRegistration(int64_t registration_id,
                            const blink::StorageKey& key,
                            DeletedVersion* deleted_version);

  // Reads user data for |registration_id| and |user_data_names| from the
  // database and writes them to |user_data_values|. Returns OK only if all keys
  // are found; otherwise NOT_FOUND, and |user_data_values| will be empty.
  Status ReadUserData(int64_t registration_id,
                      const std::vector<std::string>& user_data_names,
                      std::vector<std::string>* user_data_values);

  // Reads user data for |registration_id| and |user_data_name_prefix| from the
  // database and writes them to |user_data_values|. Returns OK if they are
  // successfully read or not found.
  Status ReadUserDataByKeyPrefix(int64_t registration_id,
                                 const std::string& user_data_name_prefix,
                                 std::vector<std::string>* user_data_values);

  // Reads user keys and associated data for |registration_id| and
  // |user_data_name_prefix| from the database and writes them to
  // |user_data_map|. The map keys are stripped of |user_data_name_prefix|.
  // Returns OK if they are successfully read or not found.
  Status ReadUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& user_data_name_prefix,
      base::flat_map<std::string, std::string>* user_data_map);

  // Writes |name_value_pairs| into the database. Returns NOT_FOUND if the
  // registration specified by |registration_id| does not exist in the database.
  Status WriteUserData(
      int64_t registration_id,
      const blink::StorageKey& key,
      const std::vector<mojom::ServiceWorkerUserDataPtr>& user_data);

  // Deletes user data for |registration_id| and |user_data_names| from the
  // database. Returns OK if all are successfully deleted or not found in the
  // database.
  Status DeleteUserData(int64_t registration_id,
                        const std::vector<std::string>& user_data_names);

  // Deletes user data for |registration_id| and |user_data_name_prefixes| from
  // the database. Returns OK if all are successfully deleted or not found in
  // the database.
  Status DeleteUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& user_data_name_prefixes);

  // Removes traces of deleted data on disk.
  Status RewriteDB();

  // Reads user data for all registrations that have data with |user_data_name|
  // from the database. Returns OK if they are successfully read or not found.
  Status ReadUserDataForAllRegistrations(
      const std::string& user_data_name,
      std::vector<mojom::ServiceWorkerUserDataPtr>* user_data);

  // Reads user data for all registrations that have data with
  // |user_data_name_prefix| from the database. Returns OK if they are
  // successfully read or not found.
  Status ReadUserDataForAllRegistrationsByKeyPrefix(
      const std::string& user_data_name_prefix,
      std::vector<mojom::ServiceWorkerUserDataPtr>* user_data);

  // Deletes user data for all registrations that have data with
  // |user_data_name_prefix| from the database. Returns OK if all are
  // successfully deleted or not found in the database.
  Status DeleteUserDataForAllRegistrationsByKeyPrefix(
      const std::string& user_data_name_prefix);

  // Resources should belong to one of following resource lists: uncommitted,
  // committed and purgeable.
  // As new resources are put into the diskcache, they go into the uncommitted
  // list. When a registration is saved that refers to those ids, they're moved
  // to the committed list. When a resource no longer has any registrations or
  // caches referring to it, it's added to the purgeable list. Periodically,
  // the purgeable list can be purged from the diskcache. At system startup, all
  // uncommitted ids are moved to the purgeable list.

  // Reads resource ids from the uncommitted list. Returns OK on success.
  // Otherwise clears |ids| and returns an error.
  Status GetUncommittedResourceIds(std::vector<int64_t>* ids);

  // Writes resource ids into the uncommitted list. Returns OK on success.
  // Otherwise writes nothing and returns an error.
  Status WriteUncommittedResourceIds(const std::vector<int64_t>& ids);

  // Reads resource ids from the purgeable list. Returns OK on success.
  // Otherwise clears |ids| and returns an error.
  Status GetPurgeableResourceIds(std::vector<int64_t>* ids);

  // Deletes resource ids from the purgeable list. Returns OK on success.
  // Otherwise deletes nothing and returns an error.
  Status ClearPurgeableResourceIds(const std::vector<int64_t>& ids);

  // Writes resource ids into the purgeable list and removes them from the
  // uncommitted list. Returns OK on success. Otherwise writes nothing and
  // returns an error.
  Status PurgeUncommittedResourceIds(const std::vector<int64_t>& ids);

  // Deletes all data for |keys|, namely, unique origin, registrations and
  // resource records.
  //
  // Specifically, this will delete data from any context where the origin
  // matches or where the context is cross-site and the origin is same-site with
  // the top-level-site.
  //
  // Resources are moved to the purgeable list. Returns OK if
  // they are successfully deleted or not found in the database. Otherwise,
  // returns an error.
  Status DeleteAllDataForOrigins(
      const std::set<url::Origin>& origins,
      std::vector<int64_t>* newly_purgeable_resources);

  // Completely deletes the contents of the database.
  // Be careful using this function.
  Status DestroyDatabase();

 private:
  // Opens the database at the |path_|. This is lazily called when the first
  // database API is called. Returns OK if the database is successfully opened.
  // Returns NOT_FOUND if the database does not exist and |create_if_missing| is
  // false. Otherwise, returns an error.
  Status LazyOpen(bool create_if_missing);

  // Helper for LazyOpen(). |status| must be the return value from LazyOpen()
  // and this must be called just after LazyOpen() is called. Returns true if
  // the database is new or nonexistent, that is, it has never been used.
  bool IsNewOrNonexistentDatabase(Status status);

  // Reads the next available id for |id_key|. Returns OK if it's successfully
  // read. Fills |next_avail_id| with an initial value and returns OK if it's
  // not found in the database. Otherwise, returns an error.
  Status ReadNextAvailableId(const char* id_key, int64_t* next_avail_id);

  // Reads registration data for |registration_id| from the database. Returns OK
  // if successfully reads. Otherwise, returns an error.
  Status ReadRegistrationData(
      int64_t registration_id,
      const blink::StorageKey& key,
      mojom::ServiceWorkerRegistrationDataPtr* registration);

  // Parses |serialized| as a RegistrationData object and pushes it into |out|.
  ServiceWorkerDatabase::Status ParseRegistrationData(
      const std::string& serialized,
      const blink::StorageKey& key,
      mojom::ServiceWorkerRegistrationDataPtr* out);

  // Populates |batch| with operations to write |registration|. It does not
  // actually write to db yet.
  void WriteRegistrationDataInBatch(
      const mojom::ServiceWorkerRegistrationData& registration,
      leveldb::WriteBatch* batch);

  // Reads resource records for |registration| from the database. Returns OK if
  // it's successfully read or not found in the database. Otherwise, returns an
  // error.
  Status ReadResourceRecords(
      const mojom::ServiceWorkerRegistrationData& registration,
      std::vector<mojom::ServiceWorkerResourceRecordPtr>* resources);

  // Parses |serialized| as a ResourceRecord object and pushes it into |out|.
  ServiceWorkerDatabase::Status ParseResourceRecord(
      const std::string& serialized,
      mojom::ServiceWorkerResourceRecordPtr* out);

  void WriteResourceRecordInBatch(
      const mojom::ServiceWorkerResourceRecord& resource,
      int64_t version_id,
      leveldb::WriteBatch* batch);

  // Deletes resource records for |version_id| from the database. Returns OK if
  // they are successfully deleted or not found in the database. Otherwise,
  // returns an error.
  Status DeleteResourceRecords(int64_t version_id,
                               std::vector<int64_t>* newly_purgeable_resources,
                               leveldb::WriteBatch* batch);

  // Reads resource ids for |id_key_prefix| from the database. Returns OK if
  // it's successfully read or not found in the database. Otherwise, returns an
  // error.
  Status ReadResourceIds(const char* id_key_prefix, std::vector<int64_t>* ids);

  // Write resource ids for |id_key_prefix| into the database. Returns OK on
  // success. Otherwise, returns writes nothing and returns an error.
  Status WriteResourceIdsInBatch(const char* id_key_prefix,
                                 const std::vector<int64_t>& ids,
                                 leveldb::WriteBatch* batch);

  // Deletes resource ids for |id_key_prefix| from the database. Returns OK if
  // it's successfully deleted or not found in the database. Otherwise, returns
  // an error.
  Status DeleteResourceIdsInBatch(const char* id_key_prefix,
                                  const std::vector<int64_t>& ids,
                                  leveldb::WriteBatch* batch);

  // Deletes all user data for |registration_id| from the database. Returns OK
  // if they are successfully deleted or not found in the database.
  Status DeleteUserDataForRegistration(int64_t registration_id,
                                       leveldb::WriteBatch* batch);

  // Reads the current schema version from the database. If the database hasn't
  // been written anything yet, sets |db_version| to 0 and returns OK.
  Status ReadDatabaseVersion(int64_t* db_version);

  // Writes a batch into the database.
  // NOTE: You must call this when you want to put something into the database
  // because this initializes the database if needed.
  Status WriteBatch(leveldb::WriteBatch* batch);

  // Bumps the next available id if |used_id| is greater than or equal to the
  // cached one.
  void BumpNextRegistrationIdIfNeeded(int64_t used_id,
                                      leveldb::WriteBatch* batch);
  void BumpNextResourceIdIfNeeded(int64_t used_id, leveldb::WriteBatch* batch);
  void BumpNextVersionIdIfNeeded(int64_t used_id, leveldb::WriteBatch* batch);

  bool IsOpen();

  void Disable(const base::Location& from_here, Status status);
  void HandleOpenResult(const base::Location& from_here, Status status);
  void HandleReadResult(const base::Location& from_here, Status status);
  void HandleWriteResult(const base::Location& from_here, Status status);

  const base::FilePath path_;
  std::unique_ptr<leveldb::Env> env_;
  std::unique_ptr<leveldb::DB> db_;

  int64_t next_avail_registration_id_;
  int64_t next_avail_resource_id_;
  int64_t next_avail_version_id_;

  enum State {
    DATABASE_STATE_UNINITIALIZED,
    DATABASE_STATE_INITIALIZED,
    DATABASE_STATE_DISABLED,
  };
  State state_;

  bool IsDatabaseInMemory() const;

  SEQUENCE_CHECKER(sequence_checker_);

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase_InMemory);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           DatabaseVersion_ValidSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           DatabaseVersion_ObsoleteSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           DatabaseVersion_CorruptedSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, GetNextAvailableIds);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           Registration_UninitializedDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           UserData_UninitializedDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, DestroyDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, InvalidWebFeature);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           NoCrossOriginEmbedderPolicyValue);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, NoFetchHandlerType);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, FetchHandlerType);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           RouterRulesLegacyPathname);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
