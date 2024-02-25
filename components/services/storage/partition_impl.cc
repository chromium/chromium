// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/partition_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
#include "components/services/storage/service_worker/service_worker_storage_control_impl.h"
#include "components/services/storage/storage_service_impl.h"

namespace storage {

namespace {

const char kSessionStorageDirectory[] = "Session Storage";

template <typename T>
base::OnceClosure MakeDeferredDeleter(std::unique_ptr<T> object) {
  return base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner, T* object) {
        task_runner->DeleteSoon(FROM_HERE, object);
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      // NOTE: We release `object` immediately. In the case
      // where this task never runs, we prefer to leak the
      // object rather than potentilaly destroying it on the
      // wrong sequence.
      object.release());
}

template <typename T>
void ShutDown(std::unique_ptr<T> object) {
  if (T* ptr = object.get())
    ptr->ShutDown(MakeDeferredDeleter(std::move(object)));
}

}  // namespace

PartitionImpl::PartitionImpl(StorageServiceImpl* service,
                             const std::optional<base::FilePath>& path)
    : service_(service), path_(path) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &PartitionImpl::OnDisconnect, base::Unretained(this)));
}

PartitionImpl::~PartitionImpl() {
  ShutDown(std::move(local_storage_));
  ShutDown(std::move(session_storage_));
}

void PartitionImpl::BindReceiver(
    mojo::PendingReceiver<mojom::Partition> receiver) {
  DCHECK(receivers_.empty() || path_.has_value())
      << "In-memory partitions must have at most one client.";

  receivers_.Add(this, std::move(receiver));
}

void PartitionImpl::BindOriginContext(
    const url::Origin& origin,
    mojo::PendingReceiver<mojom::OriginContext> receiver) {
  auto iter = origin_contexts_.find(origin);
  if (iter == origin_contexts_.end()) {
    auto result = origin_contexts_.emplace(
        origin, std::make_unique<OriginContextImpl>(this, origin));
    iter = result.first;
  }

  iter->second->BindReceiver(std::move(receiver));
}

void PartitionImpl::BindSessionStorageControl(
    mojo::PendingReceiver<mojom::SessionStorageControl> receiver) {
  session_storage_ = std::make_unique<SessionStorageImpl>(
      path_.value_or(base::FilePath()),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      base::SequencedTaskRunner::GetCurrentDefault(),
#if BUILDFLAG(IS_ANDROID)
      // On Android there is no support for session storage restoring, and since
      // the restoring code is responsible for database cleanup, we must
      // manually delete the old database here before we open a new one.
      SessionStorageImpl::BackingMode::kClearDiskStateOnOpen,
#else
      path_.has_value() ? SessionStorageImpl::BackingMode::kRestoreDiskState
                        : SessionStorageImpl::BackingMode::kNoDisk,
#endif
      std::string(kSessionStorageDirectory), std::move(receiver));
}

void PartitionImpl::BindLocalStorageControl(
    mojo::PendingReceiver<mojom::LocalStorageControl> receiver) {
  local_storage_ = std::make_unique<LocalStorageImpl>(
      path_.value_or(base::FilePath()),
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(receiver));
}

void PartitionImpl::BindServiceWorkerStorageControl(
    mojo::PendingReceiver<mojom::ServiceWorkerStorageControl> receiver) {
  service_worker_storage_ = std::make_unique<ServiceWorkerStorageControlImpl>(
      path_.value_or(base::FilePath()),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      std::move(receiver));
}

void PartitionImpl::OnDisconnect() {
  if (receivers_.empty()) {
    // Deletes |this|.
    service_->RemovePartition(this);
  }
}

void PartitionImpl::RemoveOriginContext(const url::Origin& origin) {
  origin_contexts_.erase(origin);
}

}  // namespace storage
