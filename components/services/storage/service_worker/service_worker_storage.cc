// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_storage.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/service_worker/service_worker_disk_cache.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace storage {

namespace {

void RunSoon(const base::Location& from_here, base::OnceClosure closure) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(from_here,
                                                           std::move(closure));
}

const base::FilePath::CharType kDatabaseName[] = FILE_PATH_LITERAL("Database");
const base::FilePath::CharType kDiskCacheName[] =
    FILE_PATH_LITERAL("ScriptCache");

std::optional<size_t> g_override_max_service_worker_scope_url_count_for_testing;

size_t GetMaxServiceWorkerScopeUrlCountPerStorageKey() {
  if (g_override_max_service_worker_scope_url_count_for_testing) {
    return *g_override_max_service_worker_scope_url_count_for_testing;
  }
  return kMaxServiceWorkerScopeUrlCountPerStorageKey;
}

// Used for UMA. Append-only.
enum class DeleteAndStartOverResult {
  kDeleteOk = 0,
  kDeleteDatabaseError = 1,
  kDeleteDiskCacheError = 2,
  kMaxValue = kDeleteDiskCacheError,
};

void RecordDeleteAndStartOverResult(DeleteAndStartOverResult result) {
  base::UmaHistogramEnumeration(
      "ServiceWorker.Storage.DeleteAndStartOverResult", result);
}

bool IsServiceWorkerStorageSuppressPostTaskEnabled() {
  static const bool enabled = base::FeatureList::IsEnabled(
      blink::features::kServiceWorkerStorageSuppressPostTask);
  return enabled;
}

void MaybePostTask(scoped_refptr<base::SequencedTaskRunner> task_runner,
                   const base::Location& from_here,
                   base::OnceClosure task) {
  CHECK_EQ(task_runner.get(),
           base::SequencedTaskRunner::GetCurrentDefault().get());
  if (IsServiceWorkerStorageSuppressPostTaskEnabled()) {
    std::move(task).Run();
  } else {
    task_runner->PostTask(from_here, std::move(task));
  }
}

template <class Task, class Reply>
void MaybePostTaskAndReplyWithResult(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    Task task,
    Reply reply) {
  CHECK_EQ(task_runner.get(),
           base::SequencedTaskRunner::GetCurrentDefault().get());
  if (IsServiceWorkerStorageSuppressPostTaskEnabled()) {
    std::move(reply).Run(std::move(task).Run());
  } else {
    task_runner->PostTaskAndReplyWithResult(from_here, std::move(task),
                                            std::move(reply));
  }
}

void MaybePostTaskAndReply(scoped_refptr<base::SequencedTaskRunner> task_runner,
                           const base::Location& from_here,
                           base::OnceClosure task,
                           base::OnceClosure reply) {
  CHECK_EQ(task_runner.get(),
           base::SequencedTaskRunner::GetCurrentDefault().get());
  if (IsServiceWorkerStorageSuppressPostTaskEnabled()) {
    std::move(task).Run();
    std::move(reply).Run();
  } else {
    task_runner->PostTaskAndReply(from_here, std::move(task), std::move(reply));
  }
}

}  // namespace

void OverrideMaxServiceWorkerScopeUrlCountForTesting(  // IN-TEST
    std::optional<size_t> max_count) {
  g_override_max_service_worker_scope_url_count_for_testing =
      std::move(max_count);
}

ServiceWorkerStorage::InitialData::InitialData()
    : next_registration_id(blink::mojom::kInvalidServiceWorkerRegistrationId),
      next_version_id(blink::mojom::kInvalidServiceWorkerVersionId),
      next_resource_id(blink::mojom::kInvalidServiceWorkerResourceId) {}

ServiceWorkerStorage::InitialData::~InitialData() = default;

ServiceWorkerStorage::DidDeleteRegistrationParams::DidDeleteRegistrationParams(
    int64_t registration_id,
    const blink::StorageKey& key,
    DeleteRegistrationCallback callback)
    : registration_id(registration_id),
      key(key),
      callback(std::move(callback)) {}

ServiceWorkerStorage::DidDeleteRegistrationParams::
    ~DidDeleteRegistrationParams() = default;

ServiceWorkerStorage::~ServiceWorkerStorage() {
  ClearSessionOnlyOrigins();
  weak_factory_.InvalidateWeakPtrs();
  database_task_runner_->DeleteSoon(FROM_HERE, std::move(database_));
}

// static
std::unique_ptr<ServiceWorkerStorage> ServiceWorkerStorage::Create(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner) {
  return base::WrapUnique(new ServiceWorkerStorage(
      user_data_directory, std::move(database_task_runner)));
}

void ServiceWorkerStorage::GetRegisteredStorageKeys(
    GetRegisteredStorageKeysCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(/*keys=*/std::vector<blink::StorageKey>());
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetRegisteredStorageKeys,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  std::vector<blink::StorageKey> keys(registered_keys_.begin(),
                                      registered_keys_.end());
  std::move(callback).Run(std::move(keys));
}

void ServiceWorkerStorage::FindRegistrationForClientUrl(
    const GURL& client_url,
    const blink::StorageKey& key,
    FindRegistrationForClientUrlDataCallback callback) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerStorage::FindRegistrationForClientUrl");
  DCHECK(!client_url.has_ref());
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(
          /*data=*/nullptr, /*resources=*/nullptr,
          /*scopes=*/std::nullopt,
          ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::FindRegistrationForClientUrl,
          weak_factory_.GetWeakPtr(), client_url, key, std::move(callback)));
      TRACE_EVENT_INSTANT1(
          "ServiceWorker",
          "ServiceWorkerStorage::FindRegistrationForClientUrl:LazyInitialize",
          TRACE_EVENT_SCOPE_THREAD, "URL", client_url.spec());
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  // Bypass database lookup when there is no stored registration.
  if (!base::Contains(registered_keys_, key)) {
    std::optional<std::vector<GURL>> scopes = std::vector<GURL>();
    std::move(callback).Run(
        /*data=*/nullptr, /*resources=*/nullptr, /*scopes=*/scopes,
        ServiceWorkerDatabase::Status::kErrorNotFound);
    return;
  }

  MaybePostTask(database_task_runner_, FROM_HERE,
                base::BindOnce(&FindForClientUrlInDB, database_.get(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               client_url, key, std::move(callback)));
}

void ServiceWorkerStorage::FindRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    FindRegistrationDataCallback callback) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerStorage::FindRegistrationForScope");
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             /*data=*/nullptr, /*resources=*/nullptr,
                             ServiceWorkerDatabase::Status::kErrorDisabled));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::FindRegistrationForScope,
          weak_factory_.GetWeakPtr(), scope, key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  // Bypass database lookup when there is no stored registration.
  if (!base::Contains(registered_keys_, key)) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           /*data=*/nullptr, /*resources=*/nullptr,
                           ServiceWorkerDatabase::Status::kErrorNotFound));
    return;
  }

  MaybePostTask(database_task_runner_, FROM_HERE,
                base::BindOnce(&FindForScopeInDB, database_.get(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               scope, key, std::move(callback)));
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64_t registration_id,
    const blink::StorageKey& key,
    FindRegistrationDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(
          /*data=*/nullptr, /*resources=*/nullptr,
          ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::FindRegistrationForId,
                         weak_factory_.GetWeakPtr(), registration_id, key,
                         std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  // Bypass database lookup when there is no stored registration.
  if (!base::Contains(registered_keys_, key)) {
    std::move(callback).Run(
        /*data=*/nullptr, /*resources=*/nullptr,
        ServiceWorkerDatabase::Status::kErrorNotFound);
    return;
  }

  MaybePostTask(database_task_runner_, FROM_HERE,
                base::BindOnce(&FindForIdInDB, database_.get(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               registration_id, key, std::move(callback)));
}

void ServiceWorkerStorage::FindRegistrationForIdOnly(
    int64_t registration_id,
    FindRegistrationDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(
          /*data=*/nullptr, /*resources=*/nullptr,
          ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::FindRegistrationForIdOnly,
          weak_factory_.GetWeakPtr(), registration_id, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTask(database_task_runner_, FROM_HERE,
                base::BindOnce(&FindForIdOnlyInDB, database_.get(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               registration_id, std::move(callback)));
}

void ServiceWorkerStorage::GetRegistrationsForStorageKey(
    const blink::StorageKey& key,
    GetRegistrationsDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             /*registrations=*/nullptr,
                             /*resource_lists=*/nullptr));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetRegistrationsForStorageKey,
                         weak_factory_.GetWeakPtr(), key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  auto registrations = std::make_unique<RegistrationList>();
  auto resource_lists = std::make_unique<std::vector<ResourceList>>();
  RegistrationList* registrations_ptr = registrations.get();
  std::vector<ResourceList>* resource_lists_ptr = resource_lists.get();

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::GetRegistrationsForStorageKey,
                     base::Unretained(database_.get()), key, registrations_ptr,
                     resource_lists_ptr),
      base::BindOnce(&ServiceWorkerStorage::DidGetRegistrationsForStorageKey,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(registrations), std::move(resource_lists)));
}

void ServiceWorkerStorage::GetUsageForStorageKey(
    const blink::StorageKey& key,
    GetUsageForStorageKeyCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             /*usage=*/0));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUsageForStorageKey,
                         weak_factory_.GetWeakPtr(), key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTask(database_task_runner_, FROM_HERE,
                base::BindOnce(&ServiceWorkerStorage::GetUsageForStorageKeyInDB,
                               database_.get(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               key, std::move(callback)));
}

void ServiceWorkerStorage::GetAllRegistrations(
    GetAllRegistrationsCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             /*registrations=*/nullptr));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::GetAllRegistrations,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  auto registrations = std::make_unique<RegistrationList>();
  RegistrationList* registrations_ptr = registrations.get();

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::GetAllRegistrations,
                     base::Unretained(database_.get()), registrations_ptr),
      base::BindOnce(&ServiceWorkerStorage::DidGetAllRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(registrations)));
}

void ServiceWorkerStorage::StoreRegistrationData(
    mojom::ServiceWorkerRegistrationDataPtr registration_data,
    ResourceList resources,
    StoreRegistrationDataCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(
          ServiceWorkerDatabase::Status::kErrorDisabled,
          /*deleted_version=*/blink::mojom::kInvalidServiceWorkerVersionId,
          /*deleted_resources_size=*/0,
          /*newly_purgeable_resources=*/{});
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::StoreRegistrationData,
          weak_factory_.GetWeakPtr(), std::move(registration_data),
          std::move(resources), std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  uint64_t resources_total_size_bytes =
      registration_data->resources_total_size_bytes;
  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(
          &WriteRegistrationInDB, database_.get(),
          base::SequencedTaskRunner::GetCurrentDefault(),
          std::move(registration_data), std::move(resources),
          base::BindOnce(&ServiceWorkerStorage::DidStoreRegistrationData,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         resources_total_size_bytes)));
}

void ServiceWorkerStorage::UpdateToActiveState(
    int64_t registration_id,
    const blink::StorageKey& key,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::UpdateToActiveState,
                                    weak_factory_.GetWeakPtr(), registration_id,
                                    key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateVersionToActive,
                     base::Unretained(database_.get()), registration_id, key),
      std::move(callback));
}

void ServiceWorkerStorage::UpdateLastUpdateCheckTime(
    int64_t registration_id,
    const blink::StorageKey& key,
    base::Time last_update_check_time,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::UpdateLastUpdateCheckTime,
                         weak_factory_.GetWeakPtr(), registration_id, key,
                         last_update_check_time, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateLastCheckTime,
                     base::Unretained(database_.get()), registration_id, key,
                     last_update_check_time),
      std::move(callback));
}

void ServiceWorkerStorage::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const blink::StorageKey& key,
    bool enable,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::UpdateNavigationPreloadEnabled,
                         weak_factory_.GetWeakPtr(), registration_id, key,
                         enable, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateNavigationPreloadEnabled,
                     base::Unretained(database_.get()), registration_id, key,
                     enable),
      std::move(callback));
}

void ServiceWorkerStorage::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::string& value,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::UpdateNavigationPreloadHeader,
                         weak_factory_.GetWeakPtr(), registration_id, key,
                         value, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateNavigationPreloadHeader,
                     base::Unretained(database_.get()), registration_id, key,
                     value),
      std::move(callback));
}

void ServiceWorkerStorage::UpdateFetchHandlerType(
    int64_t registration_id,
    const blink::StorageKey& key,
    blink::mojom::ServiceWorkerFetchHandlerType type,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::UpdateFetchHandlerType,
                         weak_factory_.GetWeakPtr(), registration_id, key, type,
                         std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateFetchHandlerType,
                     base::Unretained(database_.get()), registration_id, key,
                     type),
      std::move(callback));
}

void ServiceWorkerStorage::UpdateResourceSha256Checksums(
    int64_t registration_id,
    const blink::StorageKey& key,
    const base::flat_map<int64_t, std::string>& updated_sha256_checksums,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::UpdateResourceSha256Checksums,
                         weak_factory_.GetWeakPtr(), registration_id, key,
                         updated_sha256_checksums, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }
  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::UpdateResourceSha256Checksums,
                     base::Unretained(database_.get()), registration_id, key,
                     updated_sha256_checksums),
      std::move(callback));
}

void ServiceWorkerStorage::DeleteRegistration(
    int64_t registration_id,
    const blink::StorageKey& key,
    DeleteRegistrationCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(
          ServiceWorkerDatabase::Status::kErrorDisabled,
          mojom::ServiceWorkerStorageStorageKeyState::kKeep,
          /*deleted_version_id=*/blink::mojom::kInvalidServiceWorkerVersionId,
          /*deleted_resources_size=*/0,
          /*newly_purgeable_resources=*/std::vector<int64_t>());
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::DeleteRegistration,
                                    weak_factory_.GetWeakPtr(), registration_id,
                                    key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  auto params = std::make_unique<DidDeleteRegistrationParams>(
      registration_id, key, std::move(callback));

  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(
          &DeleteRegistrationFromDB, database_.get(),
          base::SequencedTaskRunner::GetCurrentDefault(), registration_id, key,
          base::BindOnce(&ServiceWorkerStorage::DidDeleteRegistration,
                         weak_factory_.GetWeakPtr(), std::move(params))));
}

void ServiceWorkerStorage::PerformStorageCleanup(base::OnceClosure callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run();
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::PerformStorageCleanup,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  MaybePostTaskAndReply(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&PerformStorageCleanupInDB, database_.get()),
      std::move(callback));
}

void ServiceWorkerStorage::CreateResourceReader(
    int64_t resource_id,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceReader> receiver) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      return;
    case STORAGE_STATE_INITIALIZING:
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::CreateResourceReader,
                                    weak_factory_.GetWeakPtr(), resource_id,
                                    std::move(receiver)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  uint64_t resource_operation_id = GetNextResourceOperationId();
  DCHECK(!base::Contains(resource_readers_, resource_operation_id));
  resource_readers_[resource_operation_id] =
      std::make_unique<ServiceWorkerResourceReaderImpl>(
          resource_id, disk_cache()->GetWeakPtr(), std::move(receiver),
          base::BindOnce(&ServiceWorkerStorage::OnResourceReaderDisconnected,
                         weak_factory_.GetWeakPtr(), resource_operation_id));
}

void ServiceWorkerStorage::CreateResourceWriter(
    int64_t resource_id,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceWriter> receiver) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      return;
    case STORAGE_STATE_INITIALIZING:
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::CreateResourceWriter,
                                    weak_factory_.GetWeakPtr(), resource_id,
                                    std::move(receiver)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  uint64_t resource_operation_id = GetNextResourceOperationId();
  DCHECK(!base::Contains(resource_writers_, resource_operation_id));
  resource_writers_[resource_operation_id] =
      std::make_unique<ServiceWorkerResourceWriterImpl>(
          resource_id, disk_cache()->GetWeakPtr(), std::move(receiver),
          base::BindOnce(&ServiceWorkerStorage::OnResourceWriterDisconnected,
                         weak_factory_.GetWeakPtr(), resource_operation_id));
}

void ServiceWorkerStorage::CreateResourceMetadataWriter(
    int64_t resource_id,
    mojo::PendingReceiver<mojom::ServiceWorkerResourceMetadataWriter>
        receiver) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      return;
    case STORAGE_STATE_INITIALIZING:
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::CreateResourceMetadataWriter,
          weak_factory_.GetWeakPtr(), resource_id, std::move(receiver)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  uint64_t resource_operation_id = GetNextResourceOperationId();
  DCHECK(!base::Contains(resource_metadata_writers_, resource_operation_id));
  resource_metadata_writers_[resource_operation_id] =
      std::make_unique<ServiceWorkerResourceMetadataWriterImpl>(
          resource_id, disk_cache()->GetWeakPtr(), std::move(receiver),
          base::BindOnce(
              &ServiceWorkerStorage::OnResourceMetadataWriterDisconnected,
              weak_factory_.GetWeakPtr(), resource_operation_id));
}

void ServiceWorkerStorage::StoreUncommittedResourceId(
    int64_t resource_id,
    DatabaseStatusCallback callback) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId, resource_id);
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::StoreUncommittedResourceId,
          weak_factory_.GetWeakPtr(), resource_id, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();

  std::vector<int64_t> resource_ids = {resource_id};
  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::WriteUncommittedResourceIds,
                     base::Unretained(database_.get()), resource_ids),
      std::move(callback));
}

void ServiceWorkerStorage::DoomUncommittedResources(
    const std::vector<int64_t>& resource_ids,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::DoomUncommittedResources,
          weak_factory_.GetWeakPtr(), resource_ids, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::PurgeUncommittedResourceIds,
                     base::Unretained(database_.get()), resource_ids),
      base::BindOnce(&ServiceWorkerStorage::DidDoomUncommittedResourceIds,
                     weak_factory_.GetWeakPtr(), resource_ids,
                     std::move(callback)));
}

void ServiceWorkerStorage::StoreUserData(
    int64_t registration_id,
    const blink::StorageKey& key,
    std::vector<mojom::ServiceWorkerUserDataPtr> user_data,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::StoreUserData, weak_factory_.GetWeakPtr(),
          registration_id, key, std::move(user_data), std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      user_data.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed));
    return;
  }

  for (const auto& entry : user_data) {
    if (entry->key.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorFailed));
      return;
    }
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::WriteUserData,
                     base::Unretained(database_.get()), registration_id, key,
                     std::move(user_data)),
      std::move(callback));
}

void ServiceWorkerStorage::GetUserData(int64_t registration_id,
                                       const std::vector<std::string>& keys,
                                       GetUserDataInDBCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             std::vector<std::string>()));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::GetUserData,
                                    weak_factory_.GetWeakPtr(), registration_id,
                                    keys, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      keys.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed,
                           std::vector<std::string>()));
    return;
  }

  for (const std::string& key : keys) {
    if (key.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorFailed,
                             std::vector<std::string>()));
      return;
    }
  }

  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerStorage::GetUserDataInDB, database_.get(),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     registration_id, keys, std::move(callback)));
}

void ServiceWorkerStorage::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataInDBCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             std::vector<std::string>()));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUserDataByKeyPrefix,
                         weak_factory_.GetWeakPtr(), registration_id,
                         key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed,
                           std::vector<std::string>()));
    return;
  }

  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerStorage::GetUserDataByKeyPrefixInDB,
                     database_.get(),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     registration_id, key_prefix, std::move(callback)));
}

void ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataInDBCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             base::flat_map<std::string, std::string>()));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefix,
                         weak_factory_.GetWeakPtr(), registration_id,
                         key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed,
                           base::flat_map<std::string, std::string>()));
    return;
  }

  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefixInDB,
                     database_.get(),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     registration_id, key_prefix, std::move(callback)));
}

void ServiceWorkerStorage::ClearUserData(int64_t registration_id,
                                         const std::vector<std::string>& keys,
                                         DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::ClearUserData,
                                    weak_factory_.GetWeakPtr(), registration_id,
                                    keys, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      keys.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed));
    return;
  }

  for (const std::string& key : keys) {
    if (key.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorFailed));
      return;
    }
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::DeleteUserData,
                     base::Unretained(database_.get()), registration_id, keys),
      std::move(callback));
}

void ServiceWorkerStorage::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::ClearUserDataByKeyPrefixes,
                         weak_factory_.GetWeakPtr(), registration_id,
                         key_prefixes, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (registration_id == blink::mojom::kInvalidServiceWorkerRegistrationId ||
      key_prefixes.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed));
    return;
  }

  for (const std::string& key_prefix : key_prefixes) {
    if (key_prefix.empty()) {
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorFailed));
      return;
    }
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerDatabase::DeleteUserDataByKeyPrefixes,
                     base::Unretained(database_.get()), registration_id,
                     key_prefixes),
      std::move(callback));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsInDBCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             std::vector<mojom::ServiceWorkerUserDataPtr>()));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(
          base::BindOnce(&ServiceWorkerStorage::GetUserDataForAllRegistrations,
                         weak_factory_.GetWeakPtr(), key, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (key.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed,
                           std::vector<mojom::ServiceWorkerUserDataPtr>()));
    return;
  }

  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ServiceWorkerStorage::GetUserDataForAllRegistrationsInDB,
                     database_.get(),
                     base::SequencedTaskRunner::GetCurrentDefault(), key,
                     std::move(callback)));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsInDBCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled,
                             std::vector<mojom::ServiceWorkerUserDataPtr>()));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefix,
          weak_factory_.GetWeakPtr(), key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed,
                           std::vector<mojom::ServiceWorkerUserDataPtr>()));
    return;
  }

  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefixInDB,
          database_.get(), base::SequencedTaskRunner::GetCurrentDefault(),
          key_prefix, std::move(callback)));
}

void ServiceWorkerStorage::ClearUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      RunSoon(FROM_HERE,
              base::BindOnce(std::move(callback),
                             ServiceWorkerDatabase::Status::kErrorDisabled));
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::ClearUserDataForAllRegistrationsByKeyPrefix,
          weak_factory_.GetWeakPtr(), key_prefix, std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  if (key_prefix.empty()) {
    RunSoon(FROM_HERE,
            base::BindOnce(std::move(callback),
                           ServiceWorkerDatabase::Status::kErrorFailed));
    return;
  }

  MaybePostTaskAndReplyWithResult(
      database_task_runner_, FROM_HERE,
      base::BindOnce(
          &ServiceWorkerDatabase::DeleteUserDataForAllRegistrationsByKeyPrefix,
          base::Unretained(database_.get()), key_prefix),
      std::move(callback));
}

void ServiceWorkerStorage::DeleteAndStartOver(DatabaseStatusCallback callback) {
  Disable();

  // Will be used in DiskCacheImplDoneWithDisk()
  delete_and_start_over_callback_ = std::move(callback);

  // Won't get a callback about cleanup being done, so call it ourselves.
  if (!expecting_done_with_disk_on_disable_)
    DiskCacheImplDoneWithDisk();
}

void ServiceWorkerStorage::DiskCacheImplDoneWithDisk() {
  expecting_done_with_disk_on_disable_ = false;
  if (!delete_and_start_over_callback_.is_null()) {
    // Delete the database on the database thread.
    MaybePostTaskAndReplyWithResult(
        database_task_runner_, FROM_HERE,
        base::BindOnce(&ServiceWorkerDatabase::DestroyDatabase,
                       base::Unretained(database_.get())),
        base::BindOnce(&ServiceWorkerStorage::DidDeleteDatabase,
                       weak_factory_.GetWeakPtr(),
                       std::move(delete_and_start_over_callback_)));
  }
}

void ServiceWorkerStorage::GetNewRegistrationId(
    base::OnceCallback<void(int64_t registration_id)> callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(
          blink::mojom::kInvalidServiceWorkerRegistrationId);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::GetNewRegistrationId,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }
  int64_t registration_id = next_registration_id_;
  ++next_registration_id_;
  std::move(callback).Run(registration_id);
}

void ServiceWorkerStorage::GetNewVersionId(
    base::OnceCallback<void(int64_t version_id)> callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(blink::mojom::kInvalidServiceWorkerVersionId);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::GetNewVersionId,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }
  int64_t version_id = next_version_id_;
  ++next_version_id_;
  std::move(callback).Run(version_id);
}

void ServiceWorkerStorage::GetNewResourceId(
    base::OnceCallback<void(int64_t resource_id)> callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(blink::mojom::kInvalidServiceWorkerResourceId);
      return;
    case STORAGE_STATE_INITIALIZING:
      // Fall-through.
    case STORAGE_STATE_UNINITIALIZED:
      LazyInitialize(base::BindOnce(&ServiceWorkerStorage::GetNewResourceId,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
      return;
    case STORAGE_STATE_INITIALIZED:
      break;
  }
  int64_t resource_id = next_resource_id_;
  ++next_resource_id_;
  std::move(callback).Run(resource_id);
}

void ServiceWorkerStorage::Disable() {
  state_ = STORAGE_STATE_DISABLED;
  if (disk_cache_)
    disk_cache_->Disable();
}

void ServiceWorkerStorage::PurgeResources(
    const std::vector<int64_t>& resource_ids) {
  if (!has_checked_for_stale_resources_)
    DeleteStaleResources();
  StartPurgingResources(resource_ids);
}

void ServiceWorkerStorage::ApplyPolicyUpdates(
    const std::vector<mojom::StoragePolicyUpdatePtr>& policy_updates,
    DatabaseStatusCallback callback) {
  switch (state_) {
    case STORAGE_STATE_DISABLED:
      std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorDisabled);
      return;
    case STORAGE_STATE_INITIALIZING:
    case STORAGE_STATE_UNINITIALIZED: {
      // An explicit clone is needed to pass `policy_updates` to LazyInitialize.
      std::vector<mojom::StoragePolicyUpdatePtr> cloned_policy_updates;
      for (const auto& entry : policy_updates)
        cloned_policy_updates.push_back(entry.Clone());

      LazyInitialize(base::BindOnce(
          &ServiceWorkerStorage::ApplyPolicyUpdates, weak_factory_.GetWeakPtr(),
          std::move(cloned_policy_updates), std::move(callback)));
      return;
    }
    case STORAGE_STATE_INITIALIZED:
      break;
  }

  for (const auto& update : policy_updates) {
    const url::Origin origin = update->origin;
    if (!update->purge_on_shutdown)
      origins_to_purge_on_shutdown_.erase(origin);
    else
      origins_to_purge_on_shutdown_.insert(std::move(origin));
  }

  std::move(callback).Run(ServiceWorkerDatabase::Status::kOk);
}

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner)
    : next_registration_id_(blink::mojom::kInvalidServiceWorkerRegistrationId),
      next_version_id_(blink::mojom::kInvalidServiceWorkerVersionId),
      next_resource_id_(blink::mojom::kInvalidServiceWorkerResourceId),
      state_(STORAGE_STATE_UNINITIALIZED),
      expecting_done_with_disk_on_disable_(false),
      user_data_directory_(user_data_directory),
      database_task_runner_(std::move(database_task_runner)),
      is_purge_pending_(false),
      has_checked_for_stale_resources_(false) {
  database_ = std::make_unique<ServiceWorkerDatabase>(GetDatabasePath());
  // Confirms that this is running on `database_task_runner_`.
  CHECK_EQ(database_task_runner_.get(),
           base::SequencedTaskRunner::GetCurrentDefault().get());
}

base::FilePath ServiceWorkerStorage::GetDatabasePath() {
  if (user_data_directory_.empty())
    return base::FilePath();
  return user_data_directory_.Append(storage::kServiceWorkerDirectory)
      .Append(kDatabaseName);
}

base::FilePath ServiceWorkerStorage::GetDiskCachePath() {
  if (user_data_directory_.empty())
    return base::FilePath();
  return user_data_directory_.Append(storage::kServiceWorkerDirectory)
      .Append(kDiskCacheName);
}

void ServiceWorkerStorage::LazyInitializeForTest() {
  DCHECK_NE(state_, STORAGE_STATE_DISABLED);

  if (state_ == STORAGE_STATE_INITIALIZED)
    return;
  base::RunLoop loop;
  LazyInitialize(loop.QuitClosure());
  loop.Run();
}

void ServiceWorkerStorage::SetPurgingCompleteCallbackForTest(
    base::OnceClosure callback) {
  DCHECK(!purging_complete_callback_for_test_);
  purging_complete_callback_for_test_ = std::move(callback);
}

void ServiceWorkerStorage::GetPurgingResourceIdsForTest(
    ResourceIdsCallback callback) {
  std::move(callback).Run(ServiceWorkerDatabase::Status::kOk,
                          std::vector<int64_t>(purgeable_resource_ids_.begin(),
                                               purgeable_resource_ids_.end()));
}

void ServiceWorkerStorage::GetPurgeableResourceIdsForTest(
    ResourceIdsCallback callback) {
  MaybePostTask(database_task_runner_, FROM_HERE,
                base::BindOnce(&GetPurgeableResourceIdsFromDB, database_.get(),
                               base::SequencedTaskRunner::GetCurrentDefault(),
                               std::move(callback)));
}

void ServiceWorkerStorage::GetUncommittedResourceIdsForTest(
    ResourceIdsCallback callback) {
  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&GetUncommittedResourceIdsFromDB, database_.get(),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(callback)));
}

void ServiceWorkerStorage::LazyInitialize(base::OnceClosure callback) {
  TRACE_EVENT("ServiceWorker", "ServiceWorkerStorage::LazyInitialize");
  DCHECK(state_ == STORAGE_STATE_UNINITIALIZED ||
         state_ == STORAGE_STATE_INITIALIZING)
      << state_;
  pending_tasks_.push_back(std::move(callback));
  if (state_ == STORAGE_STATE_INITIALIZING) {
    return;
  }

  state_ = STORAGE_STATE_INITIALIZING;
  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(&ReadInitialDataFromDB, database_.get(),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&ServiceWorkerStorage::DidReadInitialData,
                                    weak_factory_.GetWeakPtr())));
}

void ServiceWorkerStorage::DidReadInitialData(
    std::unique_ptr<InitialData> data,
    ServiceWorkerDatabase::Status status) {
  DCHECK(data);
  DCHECK_EQ(STORAGE_STATE_INITIALIZING, state_);

  if (status == ServiceWorkerDatabase::Status::kOk) {
    next_registration_id_ = data->next_registration_id;
    next_version_id_ = data->next_version_id;
    next_resource_id_ = data->next_resource_id;
    registered_keys_.swap(data->keys);
    state_ = STORAGE_STATE_INITIALIZED;
    base::UmaHistogramCounts1M("ServiceWorker.RegisteredStorageKeyCount",
                               registered_keys_.size());
  } else {
    DVLOG(2) << "Failed to initialize: "
             << ServiceWorkerDatabase::StatusToString(status);
    Disable();
  }

  for (base::OnceClosure& task : pending_tasks_)
    RunSoon(FROM_HERE, std::move(task));
  pending_tasks_.clear();
}

void ServiceWorkerStorage::DidGetRegistrationsForStorageKey(
    GetRegistrationsDataCallback callback,
    std::unique_ptr<RegistrationList> registration_data_list,
    std::unique_ptr<std::vector<ResourceList>> resource_lists,
    ServiceWorkerDatabase::Status status) {
  std::move(callback).Run(status, std::move(registration_data_list),
                          std::move(resource_lists));
}

void ServiceWorkerStorage::DidGetAllRegistrations(
    GetAllRegistrationsCallback callback,
    std::unique_ptr<RegistrationList> registration_data_list,
    ServiceWorkerDatabase::Status status) {
  std::move(callback).Run(status, std::move(registration_data_list));
}

void ServiceWorkerStorage::DidStoreRegistrationData(
    StoreRegistrationDataCallback callback,
    uint64_t new_resources_total_size_bytes,
    const blink::StorageKey& key,
    const ServiceWorkerDatabase::DeletedVersion& deleted_version,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::Status::kOk) {
    std::move(callback).Run(status, deleted_version.version_id,
                            deleted_version.resources_total_size_bytes,
                            deleted_version.newly_purgeable_resources);
    return;
  }
  registered_keys_.insert(key);

  std::move(callback).Run(ServiceWorkerDatabase::Status::kOk,
                          deleted_version.version_id,
                          deleted_version.resources_total_size_bytes,
                          deleted_version.newly_purgeable_resources);
}

void ServiceWorkerStorage::DidDeleteRegistration(
    std::unique_ptr<DidDeleteRegistrationParams> params,
    StorageKeyState storage_key_state,
    const ServiceWorkerDatabase::DeletedVersion& deleted_version,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::Status::kOk) {
    std::move(params->callback)
        .Run(status, storage_key_state, deleted_version.version_id,
             deleted_version.resources_total_size_bytes,
             deleted_version.newly_purgeable_resources);
    return;
  }

  if (storage_key_state == StorageKeyState::kDelete)
    registered_keys_.erase(params->key);

  std::move(params->callback)
      .Run(ServiceWorkerDatabase::Status::kOk, storage_key_state,
           deleted_version.version_id,
           deleted_version.resources_total_size_bytes,
           deleted_version.newly_purgeable_resources);
}

void ServiceWorkerStorage::DidDoomUncommittedResourceIds(
    const std::vector<int64_t>& resource_ids,
    DatabaseStatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  if (status == ServiceWorkerDatabase::Status::kOk)
    PurgeResources(resource_ids);
  std::move(callback).Run(status);
}

ServiceWorkerDiskCache* ServiceWorkerStorage::disk_cache() {
  DCHECK(STORAGE_STATE_INITIALIZED == state_ ||
         STORAGE_STATE_DISABLED == state_)
      << state_;
  if (disk_cache_)
    return disk_cache_.get();
  disk_cache_ = std::make_unique<ServiceWorkerDiskCache>();

  if (IsDisabled()) {
    disk_cache_->Disable();
    return disk_cache_.get();
  }

  base::FilePath path = GetDiskCachePath();
  if (path.empty()) {
    int rv = disk_cache_->InitWithMemBackend(0, net::CompletionOnceCallback());
    DCHECK_EQ(net::OK, rv);
    return disk_cache_.get();
  }

  InitializeDiskCache();
  return disk_cache_.get();
}

void ServiceWorkerStorage::InitializeDiskCache() {
  disk_cache_->set_is_waiting_to_initialize(false);
  expecting_done_with_disk_on_disable_ = true;
  int rv = disk_cache_->InitWithDiskBackend(
      GetDiskCachePath(),
      base::BindOnce(&ServiceWorkerStorage::DiskCacheImplDoneWithDisk,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&ServiceWorkerStorage::OnDiskCacheInitialized,
                     weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    OnDiskCacheInitialized(rv);
}

void ServiceWorkerStorage::OnDiskCacheInitialized(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open the serviceworker diskcache: "
               << net::ErrorToString(rv);
    Disable();
  }
  base::UmaHistogramBoolean("ServiceWorker.DiskCache.InitResult",
                            rv == net::OK);
}

void ServiceWorkerStorage::StartPurgingResources(
    const std::vector<int64_t>& resource_ids) {
  DCHECK(has_checked_for_stale_resources_);
  for (int64_t resource_id : resource_ids)
    purgeable_resource_ids_.push_back(resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::StartPurgingResources(
    const ResourceList& resources) {
  DCHECK(has_checked_for_stale_resources_);
  for (const auto& resource : resources)
    purgeable_resource_ids_.push_back(resource->resource_id);
  ContinuePurgingResources();
}

void ServiceWorkerStorage::ContinuePurgingResources() {
  if (is_purge_pending_)
    return;
  if (purgeable_resource_ids_.empty()) {
    if (purging_complete_callback_for_test_)
      std::move(purging_complete_callback_for_test_).Run();
    return;
  }

  // Do one at a time until we're done, use RunSoon to avoid recursion when
  // DoomEntry returns immediately.
  is_purge_pending_ = true;
  int64_t id = purgeable_resource_ids_.front();
  purgeable_resource_ids_.pop_front();
  RunSoon(FROM_HERE, base::BindOnce(&ServiceWorkerStorage::PurgeResource,
                                    weak_factory_.GetWeakPtr(), id));
}

void ServiceWorkerStorage::PurgeResource(int64_t id) {
  DCHECK(is_purge_pending_);
  disk_cache()->DoomEntry(
      id, base::BindOnce(&ServiceWorkerStorage::OnResourcePurged,
                         weak_factory_.GetWeakPtr(), id));
}

void ServiceWorkerStorage::OnResourcePurged(int64_t id, int rv) {
  DCHECK(is_purge_pending_);
  is_purge_pending_ = false;

  base::UmaHistogramSparse("ServiceWorker.Storage.PurgeResourceResult",
                           std::abs(rv));

  // TODO(falken): Is it always OK to ClearPurgeableResourceIds if |rv| is
  // failure? The disk cache entry might still remain and once we remove its
  // purgeable id, we will never retry deleting it.
  std::vector<int64_t> ids = {id};
  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&ServiceWorkerDatabase::ClearPurgeableResourceIds),
          base::Unretained(database_.get()), ids));

  // Continue purging resources regardless of the previous result.
  ContinuePurgingResources();
}

void ServiceWorkerStorage::DeleteStaleResources() {
  DCHECK(!has_checked_for_stale_resources_);
  has_checked_for_stale_resources_ = true;
  MaybePostTask(
      database_task_runner_, FROM_HERE,
      base::BindOnce(
          &ServiceWorkerStorage::CollectStaleResourcesFromDB, database_.get(),
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&ServiceWorkerStorage::DidCollectStaleResources,
                         weak_factory_.GetWeakPtr())));
}

void ServiceWorkerStorage::DidCollectStaleResources(
    const std::vector<int64_t>& stale_resource_ids,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::Status::kOk) {
    DCHECK_NE(ServiceWorkerDatabase::Status::kErrorNotFound, status);
    Disable();
    return;
  }
  StartPurgingResources(stale_resource_ids);
}

void ServiceWorkerStorage::ClearSessionOnlyOrigins() {
  if (!origins_to_purge_on_shutdown_.empty()) {
    MaybePostTask(
        database_task_runner_, FROM_HERE,
        base::BindOnce(&DeleteAllDataForOriginsFromDB, database_.get(),
                       origins_to_purge_on_shutdown_));
  }
}

void ServiceWorkerStorage::OnResourceReaderDisconnected(
    uint64_t resource_operation_id) {
  DCHECK(base::Contains(resource_readers_, resource_operation_id));
  resource_readers_.erase(resource_operation_id);
}

void ServiceWorkerStorage::OnResourceWriterDisconnected(
    uint64_t resource_operation_id) {
  DCHECK(base::Contains(resource_writers_, resource_operation_id));
  resource_writers_.erase(resource_operation_id);
}

void ServiceWorkerStorage::OnResourceMetadataWriterDisconnected(
    uint64_t resource_operation_id) {
  DCHECK(base::Contains(resource_metadata_writers_, resource_operation_id));
  resource_metadata_writers_.erase(resource_operation_id);
}

// static
void ServiceWorkerStorage::CollectStaleResourcesFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    GetResourcesCallback callback) {
  std::vector<int64_t> ids;
  ServiceWorkerDatabase::Status status =
      database->GetUncommittedResourceIds(&ids);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(
        original_task_runner, FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<int64_t>(ids.begin(), ids.end()), status));
    return;
  }

  status = database->PurgeUncommittedResourceIds(ids);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(
        original_task_runner, FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<int64_t>(ids.begin(), ids.end()), status));
    return;
  }

  ids.clear();
  status = database->GetPurgeableResourceIds(&ids);
  MaybePostTask(
      original_task_runner, FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::vector<int64_t>(ids.begin(), ids.end()), status));
}

// static
void ServiceWorkerStorage::ReadInitialDataFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    InitializeCallback callback) {
  TRACE_EVENT("ServiceWorker", "ServiceWorkerStorage::ReadInitialDataFromDB");
  base::TimeTicks now = base::TimeTicks::Now();

  DCHECK(database);
  std::unique_ptr<ServiceWorkerStorage::InitialData> data(
      new ServiceWorkerStorage::InitialData());

  ServiceWorkerDatabase::Status status = database->GetNextAvailableIds(
      &data->next_registration_id, &data->next_version_id,
      &data->next_resource_id);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback), std::move(data), status));
    return;
  }

  status = database->GetStorageKeysWithRegistrations(&data->keys);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback), std::move(data), status));
    return;
  }

  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), std::move(data), status));

  base::UmaHistogramMediumTimes(
      "ServiceWorker.Storage.ReadInitialDataFromDB.Time",
      base::TimeTicks::Now() - now);
}

void ServiceWorkerStorage::DeleteRegistrationFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const blink::StorageKey& key,
    DeleteRegistrationInDBCallback callback) {
  DCHECK(database);

  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ServiceWorkerDatabase::Status status =
      database->DeleteRegistration(registration_id, key, &deleted_version);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback), StorageKeyState::kKeep,
                                 deleted_version, status));
    return;
  }

  // TODO(nhiroki): Add convenient method to ServiceWorkerDatabase to check the
  // unique origin list.
  RegistrationList registrations;
  status =
      database->GetRegistrationsForStorageKey(key, &registrations, nullptr);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback), StorageKeyState::kKeep,
                                 deleted_version, status));
    return;
  }

  StorageKeyState storage_key_state =
      registrations.empty() ? StorageKeyState::kDelete : StorageKeyState::kKeep;
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), storage_key_state,
                               deleted_version, status));
}

void ServiceWorkerStorage::WriteRegistrationInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    mojom::ServiceWorkerRegistrationDataPtr registration,
    ResourceList resources,
    WriteRegistrationCallback callback) {
  DCHECK(database);
  ServiceWorkerDatabase::DeletedVersion deleted_version;
  ServiceWorkerDatabase::Status status =
      database->WriteRegistration(*registration, resources, &deleted_version);
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), registration->key,
                               deleted_version, status));
}

// static
void ServiceWorkerStorage::FindForClientUrlInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& client_url,
    const blink::StorageKey& key,
    FindForClientUrlInDBCallback callback) {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT1("ServiceWorker", "ServiceWorkerStorage::FindForClientUrlInDB",
               "url", client_url);

  RegistrationList registration_data_list;
  ServiceWorkerDatabase::Status status =
      database->GetRegistrationsForStorageKey(key, &registration_data_list,
                                              nullptr);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 /*data=*/nullptr,
                                 /*resources=*/nullptr,
                                 /*scopes=*/std::nullopt, status));
    return;
  }

  mojom::ServiceWorkerRegistrationDataPtr data;
  auto resources = std::make_unique<ResourceList>();
  status = ServiceWorkerDatabase::Status::kErrorNotFound;

  base::UmaHistogramCounts1000(
      "ServiceWorker.Storage.FindForClientUrlInDB.ScopeCountForStorageKey",
      registration_data_list.size());

  // Find one with a scope match.
  blink::ServiceWorkerLongestScopeMatcher matcher(client_url);
  int64_t match = blink::mojom::kInvalidServiceWorkerRegistrationId;
  // If the count of scope exceeds the maximum limit, we don't want to return
  // them to avoid returning too big data.
  bool return_scopes = (registration_data_list.size() <=
                        GetMaxServiceWorkerScopeUrlCountPerStorageKey());
  // `scopes` should contain all of the service worker's registration
  // scopes that are relevant to the `key` so that we can cache scope
  // URLs in the UI thread. The 'scopes' is valid only when the status
  // is `kOk` or `kErrorNotFound`.
  std::optional<std::vector<GURL>> scopes;
  if (return_scopes) {
    scopes = std::vector<GURL>();
    scopes->reserve(registration_data_list.size());
  }
  for (const auto& registration_data : registration_data_list) {
    if (matcher.MatchLongest(registration_data->scope)) {
      match = registration_data->registration_id;
    }
    if (return_scopes) {
      scopes->push_back(std::move(registration_data->scope));
    }
  }
  if (match != blink::mojom::kInvalidServiceWorkerRegistrationId)
    status = database->ReadRegistration(match, key, &data, resources.get());

  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), std::move(data),
                               std::move(resources), scopes, status));

  base::UmaHistogramMediumTimes(
      "ServiceWorker.Storage.FindForClientUrlInDB.Time",
      base::TimeTicks::Now() - now);
}

// static
void ServiceWorkerStorage::FindForScopeInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const GURL& scope,
    const blink::StorageKey& key,
    FindInDBCallback callback) {
  RegistrationList registration_data_list;
  ServiceWorkerDatabase::Status status =
      database->GetRegistrationsForStorageKey(key, &registration_data_list,
                                              nullptr);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 /*data=*/nullptr,
                                 /*resources=*/nullptr, status));
    return;
  }

  // Find one with an exact matching scope.
  mojom::ServiceWorkerRegistrationDataPtr data;
  auto resources = std::make_unique<ResourceList>();
  status = ServiceWorkerDatabase::Status::kErrorNotFound;
  for (const auto& registration_data : registration_data_list) {
    if (scope != registration_data->scope)
      continue;
    status = database->ReadRegistration(registration_data->registration_id, key,
                                        &data, resources.get());
    break;  // We're done looping.
  }

  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), std::move(data),
                               std::move(resources), status));
}

// static
void ServiceWorkerStorage::FindForIdInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const blink::StorageKey& key,
    FindInDBCallback callback) {
  mojom::ServiceWorkerRegistrationDataPtr data;
  auto resources = std::make_unique<ResourceList>();
  ServiceWorkerDatabase::Status status =
      database->ReadRegistration(registration_id, key, &data, resources.get());
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), std::move(data),
                               std::move(resources), status));
}

// static
void ServiceWorkerStorage::FindForIdOnlyInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    FindInDBCallback callback) {
  blink::StorageKey key;
  ServiceWorkerDatabase::Status status =
      database->ReadRegistrationStorageKey(registration_id, &key);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    MaybePostTask(original_task_runner, FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 /*data=*/nullptr,
                                 /*resources=*/nullptr, status));
    return;
  }
  FindForIdInDB(database, original_task_runner, registration_id, key,
                std::move(callback));
}

// static
void ServiceWorkerStorage::GetUsageForStorageKeyInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const blink::StorageKey& key,
    GetUsageForStorageKeyCallback callback) {
  int64_t usage = 0;
  ServiceWorkerDatabase::Status status =
      database->GetUsageForStorageKey(key, usage);
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), status, usage));
}

void ServiceWorkerStorage::GetUserDataInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataInDBCallback callback) {
  std::vector<std::string> values;
  ServiceWorkerDatabase::Status status =
      database->ReadUserData(registration_id, keys, &values);
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), status, values));
}

void ServiceWorkerStorage::GetUserDataByKeyPrefixInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataInDBCallback callback) {
  std::vector<std::string> values;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataByKeyPrefix(registration_id, key_prefix, &values);
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), status, values));
}

void ServiceWorkerStorage::GetUserKeysAndDataByKeyPrefixInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataInDBCallback callback) {
  base::flat_map<std::string, std::string> data_map;
  ServiceWorkerDatabase::Status status =
      database->ReadUserKeysAndDataByKeyPrefix(registration_id, key_prefix,
                                               &data_map);
  MaybePostTask(original_task_runner, FROM_HERE,
                base::BindOnce(std::move(callback), status, data_map));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const std::string& key,
    GetUserDataForAllRegistrationsInDBCallback callback) {
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataForAllRegistrations(key, &user_data);
  MaybePostTask(
      original_task_runner, FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(user_data)));
}

void ServiceWorkerStorage::GetUserDataForAllRegistrationsByKeyPrefixInDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsInDBCallback callback) {
  std::vector<mojom::ServiceWorkerUserDataPtr> user_data;
  ServiceWorkerDatabase::Status status =
      database->ReadUserDataForAllRegistrationsByKeyPrefix(key_prefix,
                                                           &user_data);
  MaybePostTask(
      original_task_runner, FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(user_data)));
}

void ServiceWorkerStorage::DeleteAllDataForOriginsFromDB(
    ServiceWorkerDatabase* database,
    const std::set<url::Origin>& origins) {
  DCHECK(database);

  std::vector<int64_t> newly_purgeable_resources;
  database->DeleteAllDataForOrigins(origins, &newly_purgeable_resources);
}

void ServiceWorkerStorage::PerformStorageCleanupInDB(
    ServiceWorkerDatabase* database) {
  DCHECK(database);
  database->RewriteDB();
}

// static
void ServiceWorkerStorage::GetPurgeableResourceIdsFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    ServiceWorkerStorage::ResourceIdsCallback callback) {
  std::vector<int64_t> resource_ids;
  ServiceWorkerDatabase::Status status =
      database->GetPurgeableResourceIds(&resource_ids);
  MaybePostTask(
      original_task_runner, FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(resource_ids)));
}

// static
void ServiceWorkerStorage::GetUncommittedResourceIdsFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    ServiceWorkerStorage::ResourceIdsCallback callback) {
  std::vector<int64_t> resource_ids;
  ServiceWorkerDatabase::Status status =
      database->GetUncommittedResourceIds(&resource_ids);
  MaybePostTask(
      original_task_runner, FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(resource_ids)));
}

void ServiceWorkerStorage::DidDeleteDatabase(
    DatabaseStatusCallback callback,
    ServiceWorkerDatabase::Status status) {
  DCHECK_EQ(STORAGE_STATE_DISABLED, state_);
  if (status != ServiceWorkerDatabase::Status::kOk) {
    // Give up the corruption recovery until the browser restarts.
    LOG(ERROR) << "Failed to delete the database: "
               << ServiceWorkerDatabase::StatusToString(status);
    RecordDeleteAndStartOverResult(
        DeleteAndStartOverResult::kDeleteDatabaseError);
    std::move(callback).Run(status);
    return;
  }
  DVLOG(1) << "Deleted ServiceWorkerDatabase successfully.";

  // Delete the disk cache. Use BLOCK_SHUTDOWN to try to avoid things being
  // half-deleted.
  // TODO(falken): Investigate if BLOCK_SHUTDOWN is needed, as the next startup
  // is expected to cleanup the disk cache anyway. Also investigate whether
  // ClearSessionOnlyOrigins() should try to delete relevant entries from the
  // disk cache before shutdown.

  // TODO(nhiroki): What if there is a bunch of files in the cache directory?
  // Deleting the directory could take a long time and restart could be delayed.
  // We should probably rename the directory and delete it later.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&base::DeletePathRecursively, GetDiskCachePath()),
      base::BindOnce(&ServiceWorkerStorage::DidDeleteDiskCache,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorage::DidDeleteDiskCache(DatabaseStatusCallback callback,
                                              bool result) {
  DCHECK_EQ(STORAGE_STATE_DISABLED, state_);
  if (!result) {
    // Give up the corruption recovery until the browser restarts.
    LOG(ERROR) << "Failed to delete the diskcache.";
    RecordDeleteAndStartOverResult(
        DeleteAndStartOverResult::kDeleteDiskCacheError);
    std::move(callback).Run(ServiceWorkerDatabase::Status::kErrorFailed);
    return;
  }
  DVLOG(1) << "Deleted ServiceWorkerDiskCache successfully.";
  RecordDeleteAndStartOverResult(DeleteAndStartOverResult::kDeleteOk);
  std::move(callback).Run(ServiceWorkerDatabase::Status::kOk);
}

}  // namespace storage
