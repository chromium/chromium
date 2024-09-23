// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_storage_control_impl.h"

#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/service_worker/service_worker_resource_ops.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {

void DidGetAllRegistrations(
    ServiceWorkerStorageControlImpl::GetAllRegistrationsDeprecatedCallback
        callback,
    mojom::ServiceWorkerDatabaseStatus status,
    std::unique_ptr<ServiceWorkerStorage::RegistrationList> registrations) {
  if (status != mojom::ServiceWorkerDatabaseStatus::kOk) {
    std::move(callback).Run(status, ServiceWorkerStorage::RegistrationList());
    return;
  }
  DCHECK(registrations);
  std::move(callback).Run(status, std::move(*registrations));
}

}  // namespace

class ServiceWorkerLiveVersionRefImpl
    : public mojom::ServiceWorkerLiveVersionRef {
 public:
  ServiceWorkerLiveVersionRefImpl(
      base::WeakPtr<ServiceWorkerStorageControlImpl> storage,
      int64_t version_id)
      : storage_(std::move(storage)), version_id_(version_id) {
    DCHECK_NE(version_id_, blink::mojom::kInvalidServiceWorkerVersionId);
    receivers_.set_disconnect_handler(
        base::BindRepeating(&ServiceWorkerLiveVersionRefImpl::OnDisconnect,
                            base::Unretained(this)));
  }
  ~ServiceWorkerLiveVersionRefImpl() override = default;

  void Add(mojo::PendingReceiver<mojom::ServiceWorkerLiveVersionRef> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void set_purgeable_resources(
      const std::vector<int64_t>& purgeable_resources) {
    if (!purgeable_resources_.empty()) {
      // This setter method can be called multiple times but the resource ids
      // should be the same.
      DCHECK(std::set<int64_t>(purgeable_resources_.begin(),
                               purgeable_resources_.end()) ==
             std::set<int64_t>(purgeable_resources.begin(),
                               purgeable_resources.end()));
      return;
    }
    purgeable_resources_ = purgeable_resources;
  }

  void clear_purgeable_resources() { purgeable_resources_.clear(); }

  const std::vector<int64_t>& purgeable_resources() const {
    return purgeable_resources_;
  }

 private:
  void OnDisconnect() {
    if (storage_ && receivers_.empty())
      storage_->OnNoLiveVersion(version_id_);
  }

  base::WeakPtr<ServiceWorkerStorageControlImpl> storage_;
  const int64_t version_id_;
  std::vector<int64_t /*resource_id*/> purgeable_resources_;
  mojo::ReceiverSet<mojom::ServiceWorkerLiveVersionRef> receivers_;
};

// static
mojo::SelfOwnedReceiverRef<mojom::ServiceWorkerStorageControl>
ServiceWorkerStorageControlImpl::Create(
    mojo::PendingReceiver<mojom::ServiceWorkerStorageControl> receiver,
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner) {
  return mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ServiceWorkerStorageControlImpl(
          user_data_directory, std::move(database_task_runner))),
      std::move(receiver));
}

ServiceWorkerStorageControlImpl::ServiceWorkerStorageControlImpl(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner)
    : storage_(ServiceWorkerStorage::Create(user_data_directory,
                                            std::move(database_task_runner))),
      receiver_(this) {}

ServiceWorkerStorageControlImpl::ServiceWorkerStorageControlImpl(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    mojo::PendingReceiver<mojom::ServiceWorkerStorageControl> receiver)
    : storage_(ServiceWorkerStorage::Create(user_data_directory,
                                            std::move(database_task_runner))),
      receiver_(this, std::move(receiver)) {}

ServiceWorkerStorageControlImpl::~ServiceWorkerStorageControlImpl() = default;

void ServiceWorkerStorageControlImpl::OnNoLiveVersion(int64_t version_id) {
  auto it = live_versions_.find(version_id);
  CHECK(it != live_versions_.end(), base::NotFatalUntil::M130);
  if (it->second->purgeable_resources().size() > 0) {
    storage_->PurgeResources(it->second->purgeable_resources());
  }
  live_versions_.erase(it);
}

void ServiceWorkerStorageControlImpl::LazyInitializeForTest() {
  storage_->LazyInitializeForTest();  // IN-TEST
}

void ServiceWorkerStorageControlImpl::Disable(DisableCallback callback) {
  storage_->Disable();
  std::move(callback).Run();
}

void ServiceWorkerStorageControlImpl::Delete(DeleteCallback callback) {
  storage_->DeleteAndStartOver(std::move(callback));
}

void ServiceWorkerStorageControlImpl::Recover(
    std::vector<mojom::ServiceWorkerLiveVersionInfoPtr> versions,
    RecoverCallback callback) {
  for (auto& version : versions) {
    DCHECK(!base::Contains(live_versions_, version->id));
    auto reference = std::make_unique<ServiceWorkerLiveVersionRefImpl>(
        weak_ptr_factory_.GetWeakPtr(), version->id);
    reference->Add(std::move(version->reference));
    reference->set_purgeable_resources(version->purgeable_resources);
    live_versions_[version->id] = std::move(reference);
  }

  std::move(callback).Run();
}

void ServiceWorkerStorageControlImpl::GetRegisteredStorageKeys(
    GetRegisteredStorageKeysCallback callback) {
  storage_->GetRegisteredStorageKeys(std::move(callback));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForClientUrl(
    const GURL& client_url,
    const blink::StorageKey& key,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForClientUrl(
      client_url, key,
      base::BindOnce(
          &ServiceWorkerStorageControlImpl::DidFindRegistrationForClientUrl,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    FindRegistrationForScopeCallback callback) {
  storage_->FindRegistrationForScope(
      scope, key,
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidFindRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForId(
    int64_t registration_id,
    const std::optional<blink::StorageKey>& key,
    FindRegistrationForIdCallback callback) {
  if (key.has_value()) {
    storage_->FindRegistrationForId(
        registration_id, *key,
        base::BindOnce(&ServiceWorkerStorageControlImpl::DidFindRegistration,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    storage_->FindRegistrationForIdOnly(
        registration_id,
        base::BindOnce(&ServiceWorkerStorageControlImpl::DidFindRegistration,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void ServiceWorkerStorageControlImpl::GetRegistrationsForStorageKey(
    const blink::StorageKey& key,
    GetRegistrationsForStorageKeyCallback callback) {
  storage_->GetRegistrationsForStorageKey(
      key,
      base::BindOnce(
          &ServiceWorkerStorageControlImpl::DidGetRegistrationsForStorageKey,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetUsageForStorageKey(
    const blink::StorageKey& key,
    GetUsageForStorageKeyCallback callback) {
  storage_->GetUsageForStorageKey(key, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetAllRegistrationsDeprecated(
    GetAllRegistrationsDeprecatedCallback callback) {
  storage_->GetAllRegistrations(
      base::BindOnce(&DidGetAllRegistrations, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::StoreRegistration(
    mojom::ServiceWorkerRegistrationDataPtr registration,
    std::vector<mojom::ServiceWorkerResourceRecordPtr> resources,
    StoreRegistrationCallback callback) {
  int64_t version_id = registration->version_id;
  storage_->StoreRegistrationData(
      std::move(registration), std::move(resources),
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidStoreRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     version_id));
}

void ServiceWorkerStorageControlImpl::DeleteRegistration(
    int64_t registration_id,
    const blink::StorageKey& key,
    DeleteRegistrationCallback callback) {
  storage_->DeleteRegistration(
      registration_id, key,
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidDeleteRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::UpdateToActiveState(
    int64_t registration_id,
    const blink::StorageKey& key,
    UpdateToActiveStateCallback callback) {
  storage_->UpdateToActiveState(registration_id, key, std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateLastUpdateCheckTime(
    int64_t registration_id,
    const blink::StorageKey& key,
    base::Time last_update_check_time,
    UpdateLastUpdateCheckTimeCallback callback) {
  storage_->UpdateLastUpdateCheckTime(
      registration_id, key, last_update_check_time, std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const blink::StorageKey& key,
    bool enable,
    UpdateNavigationPreloadEnabledCallback callback) {
  storage_->UpdateNavigationPreloadEnabled(registration_id, key, enable,
                                           std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::string& value,
    UpdateNavigationPreloadHeaderCallback callback) {
  storage_->UpdateNavigationPreloadHeader(registration_id, key, value,
                                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateFetchHandlerType(
    int64_t registration_id,
    const blink::StorageKey& key,
    blink::mojom::ServiceWorkerFetchHandlerType type,
    UpdateFetchHandlerTypeCallback callback) {
  storage_->UpdateFetchHandlerType(registration_id, key, type,
                                   std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateResourceSha256Checksums(
    int64_t registration_id,
    const blink::StorageKey& key,
    const base::flat_map<int64_t, std::string>& updated_sha256_checksums,
    UpdateResourceSha256ChecksumsCallback callback) {
  storage_->UpdateResourceSha256Checksums(
      registration_id, key, updated_sha256_checksums, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewRegistrationId(
    GetNewRegistrationIdCallback callback) {
  storage_->GetNewRegistrationId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewVersionId(
    GetNewVersionIdCallback callback) {
  storage_->GetNewVersionId(
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidGetNewVersionId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetNewResourceId(
    GetNewResourceIdCallback callback) {
  storage_->GetNewResourceId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::CreateResourceReader(
    int64_t resource_id,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceReader> reader) {
  storage_->CreateResourceReader(resource_id, std::move(reader));
}

void ServiceWorkerStorageControlImpl::CreateResourceWriter(
    int64_t resource_id,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceWriter> writer) {
  storage_->CreateResourceWriter(resource_id, std::move(writer));
}

void ServiceWorkerStorageControlImpl::CreateResourceMetadataWriter(
    int64_t resource_id,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceMetadataWriter> writer) {
  storage_->CreateResourceMetadataWriter(resource_id, std::move(writer));
}

void ServiceWorkerStorageControlImpl::StoreUncommittedResourceId(
    int64_t resource_id,
    StoreUncommittedResourceIdCallback callback) {
  storage_->StoreUncommittedResourceId(resource_id, std::move(callback));
}

void ServiceWorkerStorageControlImpl::DoomUncommittedResources(
    const std::vector<int64_t>& resource_ids,
    DoomUncommittedResourcesCallback callback) {
  storage_->DoomUncommittedResources(resource_ids, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataCallback callback) {
  storage_->GetUserData(registration_id, keys, std::move(callback));
}

void ServiceWorkerStorageControlImpl::StoreUserData(
    int64_t registration_id,
    const blink::StorageKey& key,
    std::vector<mojom::ServiceWorkerUserDataPtr> user_data,
    StoreUserDataCallback callback) {
  storage_->StoreUserData(registration_id, key, std::move(user_data),
                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::ClearUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    ClearUserDataCallback callback) {
  storage_->ClearUserData(registration_id, keys, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataByKeyPrefixCallback callback) {
  storage_->GetUserDataByKeyPrefix(registration_id, key_prefix,
                                   std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataByKeyPrefixCallback callback) {
  storage_->GetUserKeysAndDataByKeyPrefix(registration_id, key_prefix,
                                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    ClearUserDataByKeyPrefixesCallback callback) {
  storage_->ClearUserDataByKeyPrefixes(registration_id, key_prefixes,
                                       std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  storage_->GetUserDataForAllRegistrations(key, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsByKeyPrefixCallback callback) {
  storage_->GetUserDataForAllRegistrationsByKeyPrefix(key_prefix,
                                                      std::move(callback));
}

void ServiceWorkerStorageControlImpl::
    ClearUserDataForAllRegistrationsByKeyPrefix(
        const std::string& key_prefix,
        ClearUserDataForAllRegistrationsByKeyPrefixCallback callback) {
  storage_->ClearUserDataForAllRegistrationsByKeyPrefix(key_prefix,
                                                        std::move(callback));
}

void ServiceWorkerStorageControlImpl::PerformStorageCleanup(
    PerformStorageCleanupCallback callback) {
  storage_->PerformStorageCleanup(std::move(callback));
}

void ServiceWorkerStorageControlImpl::ApplyPolicyUpdates(
    const std::vector<mojom::StoragePolicyUpdatePtr> policy_updates,
    ApplyPolicyUpdatesCallback callback) {
  storage_->ApplyPolicyUpdates(std::move(policy_updates), std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetPurgingResourceIdsForTest(
    GetPurgeableResourceIdsForTestCallback callback) {
  storage_->GetPurgingResourceIdsForTest(std::move(callback));  // IN-TEST
}

void ServiceWorkerStorageControlImpl::
    GetPurgingResourceIdsForLiveVersionForTest(
        int64_t version_id,
        GetPurgeableResourceIdsForTestCallback callback) {
  auto it = live_versions_.find(version_id);
  if (it == live_versions_.end()) {
    std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorNotFound,
                            std::vector<int64_t>{});
    return;
  }

  std::move(callback).Run(ServiceWorkerDatabase::Status::kOk,
                          it->second->purgeable_resources());
}

void ServiceWorkerStorageControlImpl::GetPurgeableResourceIdsForTest(
    GetPurgeableResourceIdsForTestCallback callback) {
  storage_->GetPurgeableResourceIdsForTest(std::move(callback));  // IN-TEST
}

void ServiceWorkerStorageControlImpl::GetUncommittedResourceIdsForTest(
    GetPurgeableResourceIdsForTestCallback callback) {
  storage_->GetUncommittedResourceIdsForTest(std::move(callback));  // IN-TEST
}

void ServiceWorkerStorageControlImpl::SetPurgingCompleteCallbackForTest(
    SetPurgingCompleteCallbackForTestCallback callback) {
  storage_->SetPurgingCompleteCallbackForTest(std::move(callback));  // IN-TEST
}

void ServiceWorkerStorageControlImpl::DidFindRegistrationForClientUrl(
    FindRegistrationForClientUrlCallback callback,
    mojom::ServiceWorkerRegistrationDataPtr data,
    std::unique_ptr<ResourceList> resources,
    const std::optional<std::vector<GURL>>& scopes,
    mojom::ServiceWorkerDatabaseStatus status) {
  if (status != mojom::ServiceWorkerDatabaseStatus::kOk) {
    std::move(callback).Run(status, /*result=*/nullptr, scopes);
    return;
  }

  DCHECK(resources);
  DCHECK(data);

  mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef> remote_reference =
      CreateLiveVersionReferenceRemote(data->version_id);

  std::move(callback).Run(
      status,
      mojom::ServiceWorkerFindRegistrationResult::New(
          std::move(remote_reference), std::move(data), std::move(*resources)),
      scopes);
}

void ServiceWorkerStorageControlImpl::DidFindRegistration(
    base::OnceCallback<void(mojom::ServiceWorkerDatabaseStatus status,
                            mojom::ServiceWorkerFindRegistrationResultPtr)>
        callback,
    mojom::ServiceWorkerRegistrationDataPtr data,
    std::unique_ptr<ResourceList> resources,
    mojom::ServiceWorkerDatabaseStatus status) {
  if (status != mojom::ServiceWorkerDatabaseStatus::kOk) {
    std::move(callback).Run(status, /*result=*/nullptr);
    return;
  }

  DCHECK(resources);
  DCHECK(data);

  mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef> remote_reference =
      CreateLiveVersionReferenceRemote(data->version_id);

  std::move(callback).Run(
      status,
      mojom::ServiceWorkerFindRegistrationResult::New(
          std::move(remote_reference), std::move(data), std::move(*resources)));
}

void ServiceWorkerStorageControlImpl::DidGetRegistrationsForStorageKey(
    GetRegistrationsForStorageKeyCallback callback,
    mojom::ServiceWorkerDatabaseStatus status,
    std::unique_ptr<ServiceWorkerStorage::RegistrationList>
        registration_data_list,
    std::unique_ptr<std::vector<ResourceList>> resources_list) {
  if (status != mojom::ServiceWorkerDatabaseStatus::kOk) {
    std::move(callback).Run(
        status, std::vector<mojom::ServiceWorkerFindRegistrationResultPtr>());
    return;
  }

  DCHECK_EQ(registration_data_list->size(), resources_list->size());

  std::vector<mojom::ServiceWorkerFindRegistrationResultPtr> registrations;
  for (size_t i = 0; i < registration_data_list->size(); ++i) {
    int64_t version_id = (*registration_data_list)[i]->version_id;
    registrations.push_back(mojom::ServiceWorkerFindRegistrationResult::New(
        CreateLiveVersionReferenceRemote(version_id),
        std::move((*registration_data_list)[i]),
        std::move((*resources_list)[i])));
  }

  std::move(callback).Run(status, std::move(registrations));
}

void ServiceWorkerStorageControlImpl::DidStoreRegistration(
    StoreRegistrationCallback callback,
    int64_t stored_version_id,
    mojom::ServiceWorkerDatabaseStatus status,
    int64_t deleted_version_id,
    uint64_t deleted_resources_size,
    const std::vector<int64_t>& newly_purgeable_resources) {
  MaybePurgeResources(deleted_version_id, newly_purgeable_resources);
  MaybeCancelPurgeResources(stored_version_id);
  std::move(callback).Run(status, deleted_resources_size);
}

void ServiceWorkerStorageControlImpl::DidDeleteRegistration(
    DeleteRegistrationCallback callback,
    mojom::ServiceWorkerDatabaseStatus status,
    ServiceWorkerStorage::StorageKeyState storage_key_state,
    int64_t deleted_version_id,
    uint64_t deleted_resources_size,
    const std::vector<int64_t>& newly_purgeable_resources) {
  MaybePurgeResources(deleted_version_id, newly_purgeable_resources);
  std::move(callback).Run(status, deleted_resources_size, storage_key_state);
}

void ServiceWorkerStorageControlImpl::DidGetNewVersionId(
    GetNewVersionIdCallback callback,
    int64_t version_id) {
  mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef> remote_reference;
  if (version_id != blink::mojom::kInvalidServiceWorkerVersionId) {
    remote_reference = CreateLiveVersionReferenceRemote(version_id);
  }
  std::move(callback).Run(version_id, std::move(remote_reference));
}

mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef>
ServiceWorkerStorageControlImpl::CreateLiveVersionReferenceRemote(
    int64_t version_id) {
  DCHECK_NE(version_id, blink::mojom::kInvalidServiceWorkerVersionId);

  mojo::PendingRemote<mojom::ServiceWorkerLiveVersionRef> remote_reference;
  auto it = live_versions_.find(version_id);
  if (it == live_versions_.end()) {
    auto reference = std::make_unique<ServiceWorkerLiveVersionRefImpl>(
        weak_ptr_factory_.GetWeakPtr(), version_id);
    reference->Add(remote_reference.InitWithNewPipeAndPassReceiver());
    live_versions_[version_id] = std::move(reference);
  } else {
    // TODO(crbug.com/40207717): Remove the following CHECK() once the
    // cause is identified.
    base::debug::Alias(&version_id);
    CHECK(it->second.get()) << "Invalid version id: " << version_id;
    it->second->Add(remote_reference.InitWithNewPipeAndPassReceiver());
  }
  return remote_reference;
}

void ServiceWorkerStorageControlImpl::MaybePurgeResources(
    int64_t version_id,
    const std::vector<int64_t>& purgeable_resources) {
  if (version_id == blink::mojom::kInvalidServiceWorkerVersionId ||
      purgeable_resources.size() == 0) {
    return;
  }

  if (base::Contains(live_versions_, version_id)) {
    live_versions_[version_id]->set_purgeable_resources(
        std::move(purgeable_resources));
  } else {
    storage_->PurgeResources(std::move(purgeable_resources));
  }
}

void ServiceWorkerStorageControlImpl::MaybeCancelPurgeResources(
    int64_t version_id) {
  auto it = live_versions_.find(version_id);
  if (it == live_versions_.end())
    return;

  it->second->clear_purgeable_resources();
}

}  // namespace storage
