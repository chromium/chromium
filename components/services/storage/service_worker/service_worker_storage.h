// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "components/services/storage/service_worker/service_worker_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

class ServiceWorkerDiskCache;
class ServiceWorkerResourceMetadataWriterImpl;
class ServiceWorkerResourceReaderImpl;
class ServiceWorkerResourceWriterImpl;
class ServiceWorkerStorageControlImplTest;

namespace service_worker_storage_unittest {
class ServiceWorkerStorageTest;
class ServiceWorkerStorageDiskTest;
FORWARD_DECLARE_TEST(ServiceWorkerStorageDiskTest, DeleteAndStartOver);
FORWARD_DECLARE_TEST(ServiceWorkerStorageDiskTest,
                     DeleteAndStartOver_UnrelatedFileExists);
FORWARD_DECLARE_TEST(ServiceWorkerStorageDiskTest,
                     DeleteAndStartOver_OpenedFileExists);
FORWARD_DECLARE_TEST(ServiceWorkerStorageTest, DisabledStorage);
}  // namespace service_worker_storage_unittest

// The maximum scope URL count for cache per the storage key.
inline constexpr size_t kMaxServiceWorkerScopeUrlCountPerStorageKey = 100;

void OverrideMaxServiceWorkerScopeUrlCountForTesting(
    std::optional<size_t> max_count);

// This class provides an interface to store and retrieve ServiceWorker
// registration data. The lifetime is equal to ServiceWorkerRegistry that is
// an owner of this class. When a storage operation fails, this is marked as
// disabled and all subsequent requests are aborted until the registry is
// restarted.
class ServiceWorkerStorage {
 public:
  // This class is a communication channel between the UI thread and the thread
  // pool. This class is thread safe.
  class StorageSharedBuffer
      : public base::RefCountedThreadSafe<StorageSharedBuffer> {
   public:
    StorageSharedBuffer();
    StorageSharedBuffer(bool enable_registered_storage_keys,
                        bool enable_registration_scopes,
                        bool enable_find_registration_result);
    StorageSharedBuffer(const StorageSharedBuffer&) = delete;
    StorageSharedBuffer& operator=(const StorageSharedBuffer&) = delete;

    void PutRegisteredKeys(
        const std::vector<blink::StorageKey>& registered_keys)
        LOCKS_EXCLUDED(lock_);
    std::optional<std::vector<blink::StorageKey>> TakeRegisteredKeys()
        LOCKS_EXCLUDED(lock_);

    void PutRegistrationScopes(const blink::StorageKey& storage_key,
                               const std::vector<GURL>& scopes)
        LOCKS_EXCLUDED(lock_);
    std::map<blink::StorageKey, std::vector<GURL>> TakeRegistrationScopes()
        LOCKS_EXCLUDED(lock_);

    void PutFindRegistrationResult(
        const GURL& client_url,
        const blink::StorageKey& key,
        mojom::ServiceWorkerFindRegistrationResultPtr find_registration_result)
        LOCKS_EXCLUDED(lock_);
    mojom::ServiceWorkerFindRegistrationResultPtr TakeFindRegistrationResult(
        const GURL& client_url,
        const blink::StorageKey& key) LOCKS_EXCLUDED(lock_);

    bool enable_find_registration_result() const {
      return enable_find_registration_result_;
    }

   private:
    friend class base::RefCountedThreadSafe<StorageSharedBuffer>;
    ~StorageSharedBuffer();

    const bool enable_registered_storage_keys_;
    const bool enable_registration_scopes_;
    const bool enable_find_registration_result_;
    std::optional<std::vector<blink::StorageKey>> GUARDED_BY(lock_)
        registered_keys_;
    std::map<blink::StorageKey, std::vector<GURL>> GUARDED_BY(lock_)
        registration_scopes_;
    std::map<std::pair<GURL, blink::StorageKey>,
             mojom::ServiceWorkerFindRegistrationResultPtr>
        GUARDED_BY(lock_) find_registration_results_;
    base::Lock lock_;
  };

  using StorageKeyState = mojom::ServiceWorkerStorageStorageKeyState;
  using RegistrationList = std::vector<mojom::ServiceWorkerRegistrationDataPtr>;
  using ResourceList = std::vector<mojom::ServiceWorkerResourceRecordPtr>;
  using GetRegisteredStorageKeysCallback =
      base::OnceCallback<void(const std::vector<blink::StorageKey>& keys)>;
  using FindRegistrationForClientUrlDataCallback =
      base::OnceCallback<void(mojom::ServiceWorkerRegistrationDataPtr data,
                              std::unique_ptr<ResourceList> resources,
                              const std::optional<std::vector<GURL>>& scopes,
                              ServiceWorkerDatabase::Status status)>;
  using FindRegistrationDataCallback =
      base::OnceCallback<void(mojom::ServiceWorkerRegistrationDataPtr data,
                              std::unique_ptr<ResourceList> resources,
                              ServiceWorkerDatabase::Status status)>;
  using GetRegistrationsDataCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status status,
      std::unique_ptr<RegistrationList> registrations,
      std::unique_ptr<std::vector<ResourceList>> resource_lists)>;
  using GetUsageForStorageKeyCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status,
                              int64_t usage)>;
  using GetAllRegistrationsCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status,
                              std::unique_ptr<RegistrationList> registrations)>;
  using StoreRegistrationDataCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status status,
      int64_t deleted_version_id,
      uint64_t deleted_resources_size,
      const std::vector<int64_t>& newly_purgeable_resources)>;
  using DeleteRegistrationCallback = base::OnceCallback<void(
      ServiceWorkerDatabase::Status status,
      StorageKeyState storage_key_state,
      int64_t deleted_version_id,
      uint64_t deleted_resources_size,
      const std::vector<int64_t>& newly_purgeable_resources)>;
  using ResourceIdsCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status,
                              const std::vector<int64_t>& resource_ids)>;

  using DatabaseStatusCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status status)>;
  using GetUserDataInDBCallback =
      mojom::ServiceWorkerStorageControl::GetUserDataCallback;
  using GetUserKeysAndDataInDBCallback =
      mojom::ServiceWorkerStorageControl::GetUserKeysAndDataByKeyPrefixCallback;
  using GetUserDataForAllRegistrationsInDBCallback =
      base::OnceCallback<void(ServiceWorkerDatabase::Status,
                              std::vector<mojom::ServiceWorkerUserDataPtr>)>;

  ServiceWorkerStorage(const ServiceWorkerStorage&) = delete;
  ServiceWorkerStorage& operator=(const ServiceWorkerStorage&) = delete;

  ~ServiceWorkerStorage();

  static std::unique_ptr<ServiceWorkerStorage> Create(
      const base::FilePath& user_data_directory,
      scoped_refptr<StorageSharedBuffer> storage_shared_buffer);

  // Returns all StorageKeys which have service worker registrations.
  void GetRegisteredStorageKeys(GetRegisteredStorageKeysCallback callback);

  // Reads stored registrations for `client_url`, `scope`, or
  // `registration_id` with the associated `key`. Returns
  // ServiceWorkerDatabase::Status::kOk with non-null RegistrationData and
  // ResourceList if registration is found, or returns
  // ServiceWorkerDatabase::Status::kErrorNotFound if no matching registration
  // is found.
  void FindRegistrationForClientUrl(
      const GURL& client_url,
      const blink::StorageKey& key,
      FindRegistrationForClientUrlDataCallback callback);
  void FindRegistrationForScope(const GURL& scope,
                                const blink::StorageKey& key,
                                FindRegistrationDataCallback callback);
  void FindRegistrationForId(int64_t registration_id,
                             const blink::StorageKey& key,
                             FindRegistrationDataCallback callback);
  void FindRegistrationForIdOnly(int64_t registration_id,
                                 FindRegistrationDataCallback callback);

  // Returns all stored registrations for a given `key`.
  void GetRegistrationsForStorageKey(const blink::StorageKey& key,
                                     GetRegistrationsDataCallback callback);

  // Reads the total resource size stored in the storage for a given `key`.
  void GetUsageForStorageKey(const blink::StorageKey& key,
                             GetUsageForStorageKeyCallback callback);

  // Returns all stored registrations.
  void GetAllRegistrations(GetAllRegistrationsCallback callback);

  // Stores |registration_data| and |resources| on persistent storage.
  void StoreRegistrationData(
      mojom::ServiceWorkerRegistrationDataPtr registration_data,
      ResourceList resources,
      StoreRegistrationDataCallback callback);

  // Updates the state of the registration's stored version to active.
  void UpdateToActiveState(int64_t registration_id,
                           const blink::StorageKey& key,
                           DatabaseStatusCallback callback);

  // Updates the stored time to match the value of
  // registration->last_update_check().
  void UpdateLastUpdateCheckTime(int64_t registration_id,
                                 const blink::StorageKey& key,
                                 base::Time last_update_check_time,
                                 DatabaseStatusCallback callback);

  // Updates the specified registration's navigation preload state in storage.
  // The caller is responsible for mutating the live registration's state.
  void UpdateNavigationPreloadEnabled(int64_t registration_id,
                                      const blink::StorageKey& key,
                                      bool enable,
                                      DatabaseStatusCallback callback);
  void UpdateNavigationPreloadHeader(int64_t registration_id,
                                     const blink::StorageKey& key,
                                     const std::string& value,
                                     DatabaseStatusCallback callback);
  // Updates the stored fetch handler type to match the value of
  // the active service worker version's.
  void UpdateFetchHandlerType(int64_t registration_id,
                              const blink::StorageKey& key,
                              blink::mojom::ServiceWorkerFetchHandlerType type,
                              DatabaseStatusCallback callback);

  // Updates sha256 scripts in resource lists of the active service worker
  // version's.
  void UpdateResourceSha256Checksums(
      int64_t registration_id,
      const blink::StorageKey& key,
      const base::flat_map<int64_t, std::string>& updated_sha256_checksums,
      DatabaseStatusCallback callback);

  // Deletes the registration specified by |registration_id|. This should be
  // called only from ServiceWorkerRegistry.
  void DeleteRegistration(int64_t registration_id,
                          const blink::StorageKey& key,
                          DeleteRegistrationCallback callback);

  // Removes traces of deleted data on disk.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Creates a resource accessor. Never returns nullptr but an accessor may be
  // associated with the disabled disk cache if the storage is disabled.
  void CreateResourceReader(
      int64_t resource_id,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceReader> receiver);
  void CreateResourceWriter(
      int64_t resource_id,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceWriter> receiver);
  void CreateResourceMetadataWriter(
      int64_t resource_id,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceMetadataWriter>
          receiver);

  // Adds |resource_id| to the set of resources that are in the disk cache
  // but not yet stored with a registration.
  void StoreUncommittedResourceId(int64_t resource_id,
                                  DatabaseStatusCallback callback);

  // Removes resource ids from uncommitted list, adds them to the purgeable list
  // and purges them.
  void DoomUncommittedResources(const std::vector<int64_t>& resource_ids,
                                DatabaseStatusCallback callback);

  // Provide a storage mechanism to read/write arbitrary data associated with
  // a registration. Each registration has its own key namespace.
  // GetUserData responds OK only if all keys are found; otherwise NOT_FOUND,
  // and the callback's data will be empty.
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataInDBCallback callback);
  // GetUserDataByKeyPrefix responds OK with a vector containing data rows that
  // had matching keys assuming the database was read successfully.
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataInDBCallback callback);
  // GetUserKeysAndDataByKeyPrefix responds OK with a flat_map containing
  // matching keys and their data assuming the database was read successfully.
  // The map keys have |key_prefix| stripped from them.
  void GetUserKeysAndDataByKeyPrefix(int64_t registration_id,
                                     const std::string& key_prefix,
                                     GetUserKeysAndDataInDBCallback callback);

  // Stored data is deleted when the associated registraton is deleted.
  void StoreUserData(int64_t registration_id,
                     const blink::StorageKey& key,
                     std::vector<mojom::ServiceWorkerUserDataPtr> user_data,
                     DatabaseStatusCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     DatabaseStatusCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  // Neither |key_prefixes| nor the prefixes within can be empty.
  void ClearUserDataByKeyPrefixes(int64_t registration_id,
                                  const std::vector<std::string>& key_prefixes,
                                  DatabaseStatusCallback callback);
  // Responds with all registrations that have user data with a particular key,
  // as well as that user data.
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsInDBCallback callback);
  // Responds with all registrations that have user data with a particular key,
  // as well as that user data.
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsInDBCallback callback);
  // Responds OK if all are successfully deleted or not found in the database.
  // |key_prefix| cannot be empty.
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      DatabaseStatusCallback callback);

  // Deletes the storage and starts over. This should be called only from
  // ServiceWorkerRegistry other than tests.
  void DeleteAndStartOver(DatabaseStatusCallback callback);

  // Returns a new registration id which is guaranteed to be unique in the
  // storage. Returns blink::mojom::kInvalidServiceWorkerRegistrationId if the
  // storage is disabled.
  void GetNewRegistrationId(
      base::OnceCallback<void(int64_t registration_id)> callback);

  // Returns a new version id which is guaranteed to be unique in the storage.
  // Returns kInvalidServiceWorkerVersionId if the storage is disabled.
  void GetNewVersionId(base::OnceCallback<void(int64_t version_id)> callback);

  // Returns a new resource id which is guaranteed to be unique in the storage.
  // Returns blink::mojom::kInvalidServiceWorkerResourceId if the storage
  // is disabled.
  void GetNewResourceId(base::OnceCallback<void(int64_t resource_id)> callback);

  void Disable();

  // Schedules deleting `resource_ids` from the disk cache and removing their
  // keys as purgeable resources from the service worker database. It's OK to
  // call this for resources that don't have purgeable resource keys, like
  // uncommitted resources, as long as the caller does its own cleanup to remove
  // the uncommitted resource keys.
  void PurgeResources(const std::vector<int64_t>& resource_ids);

  // Applies |policy_updates|.
  void ApplyPolicyUpdates(
      const std::vector<mojom::StoragePolicyUpdatePtr>& policy_updates,
      DatabaseStatusCallback callback);

  void LazyInitializeForTest();

  void SetPurgingCompleteCallbackForTest(base::OnceClosure callback);

  void GetPurgingResourceIdsForTest(ResourceIdsCallback callback);
  void GetPurgeableResourceIdsForTest(ResourceIdsCallback callback);
  void GetUncommittedResourceIdsForTest(ResourceIdsCallback callback);
  StorageSharedBuffer& storage_shared_buffer() {
    // storage_shared_buffer_  always exists.
    return *storage_shared_buffer_;
  }

 private:
  friend class ServiceWorkerStorageControlImplTest;
  friend class service_worker_storage_unittest::ServiceWorkerStorageTest;
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerStorageDiskTest,
      DeleteAndStartOver);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerStorageDiskTest,
      DeleteAndStartOver_UnrelatedFileExists);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerStorageDiskTest,
      DeleteAndStartOver_OpenedFileExists);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_storage_unittest::ServiceWorkerStorageTest,
      DisabledStorage);

  struct InitialData {
    int64_t next_registration_id;
    int64_t next_version_id;
    int64_t next_resource_id;
    std::set<blink::StorageKey> keys;

    InitialData();
    ~InitialData();
  };

  // Because there are too many params for base::Bind to wrap a closure around.
  struct DidDeleteRegistrationParams {
    int64_t registration_id;
    blink::StorageKey key;
    DeleteRegistrationCallback callback;

    DidDeleteRegistrationParams(int64_t registration_id,
                                const blink::StorageKey& key,
                                DeleteRegistrationCallback callback);
    ~DidDeleteRegistrationParams();
  };

  ServiceWorkerStorage(
      const base::FilePath& user_data_directory,
      scoped_refptr<StorageSharedBuffer> storage_shared_buffer);

  base::FilePath GetDatabasePath();
  base::FilePath GetDiskCachePath();

  void LazyInitialize(base::OnceClosure callback);
  void DidReadInitialData(std::unique_ptr<InitialData> data,
                          ServiceWorkerDatabase::Status status);
  void DidStoreRegistrationData(
      StoreRegistrationDataCallback callback,
      const blink::StorageKey& key,
      const ServiceWorkerDatabase::DeletedVersion& deleted_version,
      ServiceWorkerDatabase::Status status);
  void DidDeleteRegistration(
      std::unique_ptr<DidDeleteRegistrationParams> params,
      StorageKeyState storage_key_state,
      const ServiceWorkerDatabase::DeletedVersion& deleted_version,
      ServiceWorkerDatabase::Status status);

  // Lazy disk_cache getter.
  ServiceWorkerDiskCache* disk_cache();
  void InitializeDiskCache();
  void OnDiskCacheInitialized(int rv);

  void StartPurgingResources(const std::vector<int64_t>& resource_ids);
  void StartPurgingResources(const ResourceList& resources);
  void ContinuePurgingResources();
  void PurgeResource(int64_t id);
  void OnResourcePurged(int64_t id, int rv);

  // Deletes purgeable and uncommitted resources left over from the previous
  // browser session. This must be called once per session before any database
  // operation that may mutate the purgeable or uncommitted resource lists.
  void DeleteStaleResources();
  void DidCollectStaleResources(const std::vector<int64_t>& stale_resource_ids,
                                ServiceWorkerDatabase::Status status);

  void ClearSessionOnlyOrigins();

  bool IsDisabled() const { return state_ == STORAGE_STATE_DISABLED; }

  uint64_t GetNextResourceOperationId() {
    return next_resource_operation_id_++;
  }
  void OnResourceReaderDisconnected(uint64_t resource_operation_id);
  void OnResourceWriterDisconnected(uint64_t resource_operation_id);
  void OnResourceMetadataWriterDisconnected(uint64_t resource_operation_id);

  void CollectStaleResourcesFromDB();
  void ReadInitialDataFromDB();
  void DeleteRegistrationFromDB(
      int64_t registration_id,
      const blink::StorageKey& key,
      std::unique_ptr<DidDeleteRegistrationParams> params);
  void FindForClientUrlInDB(const GURL& client_url,
                            const blink::StorageKey& key,
                            FindRegistrationForClientUrlDataCallback callback);
  void FindForScopeInDB(const GURL& scope,
                        const blink::StorageKey& key,
                        FindRegistrationDataCallback callback);

  // Posted by the underlying cache implementation after it finishes making
  // disk changes upon its destruction.
  void DiskCacheImplDoneWithDisk();
  void DidDeleteDatabase(DatabaseStatusCallback callback,
                         ServiceWorkerDatabase::Status status);
  // Posted when we finish deleting the cache directory.
  void DidDeleteDiskCache(DatabaseStatusCallback callback, bool result);

  // StorageKeys having registations.
  std::set<blink::StorageKey> registered_keys_;
  // The set of StorageKeys whose storage should be cleaned on shutdown.
  std::set<url::Origin> origins_to_purge_on_shutdown_;

  // Pending database tasks waiting for initialization.
  std::vector<base::OnceClosure> pending_tasks_;

  int64_t next_registration_id_;
  int64_t next_version_id_;
  int64_t next_resource_id_;

  enum State {
    STORAGE_STATE_UNINITIALIZED,
    STORAGE_STATE_INITIALIZING,
    STORAGE_STATE_INITIALIZED,
    STORAGE_STATE_DISABLED,
  };
  State state_;

  // non-null between when DeleteAndStartOver() is called and when the
  // underlying disk cache stops using the disk.
  DatabaseStatusCallback delete_and_start_over_callback_;

  // This is set when we know that a call to Disable() will result in
  // DiskCacheImplDoneWithDisk() eventually called. This might not happen
  // for many reasons:
  // 1) A previous call to Disable() may have already triggered that.
  // 2) We may be using a memory backend.
  // 3) |disk_cache_| might not have been created yet.
  // ... so it's easier to keep track of the case when it will happen.
  bool expecting_done_with_disk_on_disable_;

  base::FilePath user_data_directory_;

  scoped_refptr<StorageSharedBuffer> storage_shared_buffer_;

  std::unique_ptr<ServiceWorkerDatabase> database_;

  std::unique_ptr<ServiceWorkerDiskCache> disk_cache_;

  base::circular_deque<int64_t> purgeable_resource_ids_;
  bool is_purge_pending_;
  bool has_checked_for_stale_resources_;
  base::OnceClosure purging_complete_callback_for_test_;

  uint64_t next_resource_operation_id_ = 0;
  base::flat_map<uint64_t, std::unique_ptr<ServiceWorkerResourceReaderImpl>>
      resource_readers_;
  base::flat_map<uint64_t, std::unique_ptr<ServiceWorkerResourceWriterImpl>>
      resource_writers_;
  base::flat_map<uint64_t,
                 std::unique_ptr<ServiceWorkerResourceMetadataWriterImpl>>
      resource_metadata_writers_;

  base::WeakPtrFactory<ServiceWorkerStorage> weak_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
