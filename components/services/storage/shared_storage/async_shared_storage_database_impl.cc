// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/async_shared_storage_database_impl.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

namespace storage {

// static
std::unique_ptr<AsyncSharedStorageDatabase>
AsyncSharedStorageDatabaseImpl::Create(
    base::FilePath db_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<SharedStorageDatabaseOptions> options) {
  return absl::WrapUnique(new AsyncSharedStorageDatabaseImpl(
      std::move(db_path), std::move(blocking_task_runner),
      std::move(special_storage_policy), std::move(options)));
}

AsyncSharedStorageDatabaseImpl::AsyncSharedStorageDatabaseImpl(
    base::FilePath db_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<SharedStorageDatabaseOptions> options)
    : database_(base::SequenceBound<SharedStorageDatabase>(
          std::move(blocking_task_runner),
          std::move(db_path),
          std::move(special_storage_policy),
          std::move(options))) {}

AsyncSharedStorageDatabaseImpl::~AsyncSharedStorageDatabaseImpl() = default;

void AsyncSharedStorageDatabaseImpl::Destroy(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Destroy)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::TrimMemory(base::OnceClosure callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::TrimMemory)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Get(
    url::Origin context_origin,
    std::u16string key,
    base::OnceCallback<void(GetResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Get)
      .WithArgs(std::move(context_origin), std::move(key))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Set(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    base::OnceCallback<void(OperationResult)> callback,
    SetBehavior behavior) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Set)
      .WithArgs(std::move(context_origin), std::move(key), std::move(value),
                behavior)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Append(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Append)
      .WithArgs(std::move(context_origin), std::move(key), std::move(value))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Delete(
    url::Origin context_origin,
    std::u16string key,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Delete)
      .WithArgs(std::move(context_origin), std::move(key))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Clear(
    url::Origin context_origin,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Clear)
      .WithArgs(std::move(context_origin))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Length(
    url::Origin context_origin,
    base::OnceCallback<void(int)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Length)
      .WithArgs(std::move(context_origin))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Keys(
    url::Origin context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Keys)
      .WithArgs(std::move(context_origin), std::move(pending_listener))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::Entries(
    url::Origin context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::Entries)
      .WithArgs(std::move(context_origin), std::move(pending_listener))
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::PurgeMatchingOrigins(
    OriginMatcherFunction origin_matcher,
    base::Time begin,
    base::Time end,
    base::OnceCallback<void(OperationResult)> callback,
    bool perform_storage_cleanup) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::PurgeMatchingOrigins)
      .WithArgs(std::move(origin_matcher), begin, end, perform_storage_cleanup)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::PurgeStaleOrigins(
    base::TimeDelta window_to_be_deemed_active,
    base::OnceCallback<void(OperationResult)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::PurgeStaleOrigins)
      .WithArgs(window_to_be_deemed_active)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::FetchOrigins(
    base::OnceCallback<void(std::vector<mojom::StorageUsageInfoPtr>)>
        callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::FetchOrigins)
      .Then(std::move(callback));
}

base::SequenceBound<SharedStorageDatabase>*
AsyncSharedStorageDatabaseImpl::GetSequenceBoundDatabaseForTesting() {
  return database_ ? &database_ : nullptr;
}

void AsyncSharedStorageDatabaseImpl::IsOpenForTesting(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::IsOpenForTesting)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::DBStatusForTesting(
    base::OnceCallback<void(InitStatus)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::DBStatusForTesting)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::OverrideLastUsedTimeForTesting(
    url::Origin context_origin,
    base::Time new_last_used_time,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(database_);
  database_.AsyncCall(&SharedStorageDatabase::OverrideLastUsedTimeForTesting)
      .WithArgs(std::move(context_origin), new_last_used_time)
      .Then(std::move(callback));
}

void AsyncSharedStorageDatabaseImpl::OverrideSpecialStoragePolicyForTesting(
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK(database_);
  database_
      .AsyncCall(&SharedStorageDatabase::OverrideSpecialStoragePolicyForTesting)
      .WithArgs(std::move(special_storage_policy));
}

}  // namespace storage
