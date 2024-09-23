// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "components/services/storage/service_worker/service_worker_storage.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class ServiceWorkerLiveVersionRefImpl;

// This class wraps ServiceWorkerStorage to implement mojo interface defined by
// the storage service, i.e., ServiceWorkerStorageControl.
// ServiceWorkerStorageControlImpl is created on database_task_runner (thread
// pool) and retained by mojo::SelfOwnedReceiver.
// TODO(crbug.com/40120038): Merge this implementation into ServiceWorkerStorage
// and move the merged class to components/services/storage.
class ServiceWorkerStorageControlImpl
    : public mojom::ServiceWorkerStorageControl {
 public:
  static mojo::SelfOwnedReceiverRef<mojom::ServiceWorkerStorageControl> Create(
      mojo::PendingReceiver<mojom::ServiceWorkerStorageControl> receiver,
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner);
  ServiceWorkerStorageControlImpl(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      mojo::PendingReceiver<mojom::ServiceWorkerStorageControl> receiver);

  ServiceWorkerStorageControlImpl(const ServiceWorkerStorageControlImpl&) =
      delete;
  ServiceWorkerStorageControlImpl& operator=(
      const ServiceWorkerStorageControlImpl&) = delete;

  ~ServiceWorkerStorageControlImpl() override;

  void OnNoLiveVersion(int64_t version_id);

  void LazyInitializeForTest();

 private:
  ServiceWorkerStorageControlImpl(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner);
  // mojom::ServiceWorkerStorageControl implementations:
  void Disable(DisableCallback callback) override;
  void Delete(DeleteCallback callback) override;
  void Recover(std::vector<mojom::ServiceWorkerLiveVersionInfoPtr> versions,
               RecoverCallback callback) override;
  void GetRegisteredStorageKeys(
      GetRegisteredStorageKeysCallback callback) override;
  void FindRegistrationForClientUrl(
      const GURL& client_url,
      const blink::StorageKey& key,
      FindRegistrationForClientUrlCallback callback) override;
  void FindRegistrationForScope(
      const GURL& scope,
      const blink::StorageKey& key,
      FindRegistrationForScopeCallback callback) override;
  void FindRegistrationForId(int64_t registration_id,
                             const std::optional<blink::StorageKey>& key,
                             FindRegistrationForIdCallback callback) override;
  void GetRegistrationsForStorageKey(
      const blink::StorageKey& key,
      GetRegistrationsForStorageKeyCallback callback) override;
  void GetUsageForStorageKey(const blink::StorageKey& key,
                             GetUsageForStorageKeyCallback callback) override;
  void GetAllRegistrationsDeprecated(
      GetAllRegistrationsDeprecatedCallback calback) override;
  void StoreRegistration(
      mojom::ServiceWorkerRegistrationDataPtr registration,
      std::vector<mojom::ServiceWorkerResourceRecordPtr> resources,
      StoreRegistrationCallback callback) override;
  void DeleteRegistration(int64_t registration_id,
                          const blink::StorageKey& key,
                          DeleteRegistrationCallback callback) override;
  void UpdateToActiveState(int64_t registration_id,
                           const blink::StorageKey& key,
                           UpdateToActiveStateCallback callback) override;
  void UpdateLastUpdateCheckTime(
      int64_t registration_id,
      const blink::StorageKey& key,
      base::Time last_update_check_time,
      UpdateLastUpdateCheckTimeCallback callback) override;
  void UpdateNavigationPreloadEnabled(
      int64_t registration_id,
      const blink::StorageKey& key,
      bool enable,
      UpdateNavigationPreloadEnabledCallback callback) override;
  void UpdateNavigationPreloadHeader(
      int64_t registration_id,
      const blink::StorageKey& key,
      const std::string& value,
      UpdateNavigationPreloadHeaderCallback callback) override;
  void UpdateFetchHandlerType(
      int64_t registration_id,
      const blink::StorageKey& key,
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type,
      UpdateFetchHandlerTypeCallback callback) override;
  void UpdateResourceSha256Checksums(
      int64_t registration_id,
      const blink::StorageKey& key,
      const base::flat_map<int64_t, std::string>& updated_sha256_checksums,
      UpdateResourceSha256ChecksumsCallback callback) override;
  void GetNewRegistrationId(GetNewRegistrationIdCallback callback) override;
  void GetNewVersionId(GetNewVersionIdCallback callback) override;
  void GetNewResourceId(GetNewResourceIdCallback callback) override;
  void CreateResourceReader(
      int64_t resource_id,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceReader> reader)
      override;
  void CreateResourceWriter(
      int64_t resource_id,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceWriter> writer)
      override;
  void CreateResourceMetadataWriter(
      int64_t resource_id,
      mojo::PendingReceiver<mojom::ServiceWorkerResourceMetadataWriter> writer)
      override;
  void StoreUncommittedResourceId(
      int64_t resource_id,
      StoreUncommittedResourceIdCallback callback) override;
  void DoomUncommittedResources(
      const std::vector<int64_t>& resource_ids,
      DoomUncommittedResourcesCallback callback) override;
  void GetUserData(int64_t registration_id,
                   const std::vector<std::string>& keys,
                   GetUserDataCallback callback) override;
  void StoreUserData(int64_t registration_id,
                     const blink::StorageKey& key,
                     std::vector<mojom::ServiceWorkerUserDataPtr> user_data,
                     StoreUserDataCallback callback) override;
  void ClearUserData(int64_t registration_id,
                     const std::vector<std::string>& keys,
                     ClearUserDataCallback callback) override;
  void GetUserDataByKeyPrefix(int64_t registration_id,
                              const std::string& key_prefix,
                              GetUserDataByKeyPrefixCallback callback) override;
  void GetUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix,
      GetUserKeysAndDataByKeyPrefixCallback callback) override;
  void ClearUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes,
      ClearUserDataByKeyPrefixesCallback callback) override;
  void GetUserDataForAllRegistrations(
      const std::string& key,
      GetUserDataForAllRegistrationsCallback callback) override;
  void GetUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      GetUserDataForAllRegistrationsByKeyPrefixCallback callback) override;
  void ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix,
      ClearUserDataForAllRegistrationsByKeyPrefixCallback callback) override;
  void PerformStorageCleanup(PerformStorageCleanupCallback callback) override;
  void ApplyPolicyUpdates(
      const std::vector<mojom::StoragePolicyUpdatePtr> policy_updates,
      ApplyPolicyUpdatesCallback callback) override;
  void GetPurgingResourceIdsForTest(
      GetPurgingResourceIdsForTestCallback callback) override;
  void GetPurgingResourceIdsForLiveVersionForTest(
      int64_t version_id,
      GetPurgingResourceIdsForTestCallback callback) override;
  void GetPurgeableResourceIdsForTest(
      GetPurgeableResourceIdsForTestCallback callback) override;
  void GetUncommittedResourceIdsForTest(
      GetUncommittedResourceIdsForTestCallback callback) override;
  void SetPurgingCompleteCallbackForTest(
      SetPurgingCompleteCallbackForTestCallback callback) override;

  using ResourceList = std::vector<mojom::ServiceWorkerResourceRecordPtr>;

  // Callbacks for ServiceWorkerStorage methods.
  void DidFindRegistrationForClientUrl(
      FindRegistrationForClientUrlCallback callback,
      mojom::ServiceWorkerRegistrationDataPtr data,
      std::unique_ptr<ResourceList> resources,
      const std::optional<std::vector<GURL>>& scopes,
      mojom::ServiceWorkerDatabaseStatus status);
  void DidFindRegistration(
      base::OnceCallback<void(mojom::ServiceWorkerDatabaseStatus status,
                              mojom::ServiceWorkerFindRegistrationResultPtr)>
          callback,
      mojom::ServiceWorkerRegistrationDataPtr data,
      std::unique_ptr<ResourceList> resources,
      mojom::ServiceWorkerDatabaseStatus status);
  void DidGetRegistrationsForStorageKey(
      GetRegistrationsForStorageKeyCallback callback,
      mojom::ServiceWorkerDatabaseStatus status,
      std::unique_ptr<ServiceWorkerStorage::RegistrationList>
          registration_data_list,
      std::unique_ptr<std::vector<ResourceList>> resources_list);
  void DidStoreRegistration(
      StoreRegistrationCallback callback,
      int64_t stored_version_id,
      mojom::ServiceWorkerDatabaseStatus status,
      int64_t deleted_version_id,
      uint64_t deleted_resources_size,
      const std::vector<int64_t>& newly_purgeable_resources);
  void DidDeleteRegistration(
      DeleteRegistrationCallback callback,
      mojom::ServiceWorkerDatabaseStatus status,
      ServiceWorkerStorage::StorageKeyState storage_key_state,
      int64_t deleted_version_id,
      uint64_t deleted_resources_size,
      const std::vector<int64_t>& newly_purgeable_resources);
  void DidGetNewVersionId(GetNewVersionIdCallback callback, int64_t version_id);

  mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef>
  CreateLiveVersionReferenceRemote(int64_t version_id);

  void MaybePurgeResources(int64_t version_id,
                           const std::vector<int64_t>& purgeable_resources);

  // Cancels resource purging on successfull registration.
  // This is necessary when resurrecting an uninstalling registration
  // in the unregistration + registration case because unregistration could've
  // scheduled resources purging yet registration will try to reuse them which
  // leads to potential use of doomed resources once the current version is
  // marked as no longer alive.
  void MaybeCancelPurgeResources(int64_t version_id);

  const std::unique_ptr<ServiceWorkerStorage> storage_;

  mojo::Receiver<mojom::ServiceWorkerStorageControl> receiver_;

  base::flat_map<int64_t /*version_id*/,
                 std::unique_ptr<ServiceWorkerLiveVersionRefImpl>>
      live_versions_;

  base::WeakPtrFactory<ServiceWorkerStorageControlImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_CONTROL_IMPL_H_
