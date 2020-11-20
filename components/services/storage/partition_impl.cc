// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/partition_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
#include "components/services/storage/storage_service_impl.h"

namespace storage {

namespace {

const char kSessionStorageDirectory[] = "Session Storage";

}  // namespace

PartitionImpl::PartitionImpl(StorageServiceImpl* service,
                             const base::Optional<base::FilePath>& path)
    : service_(service), path_(path) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &PartitionImpl::OnDisconnect, base::Unretained(this)));
}

PartitionImpl::~PartitionImpl() = default;

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
  // This object deletes itself on disconnection.
  session_storage_ = new SessionStorageImpl(
      path_.value_or(base::FilePath()),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      base::SequencedTaskRunnerHandle::Get(),
#if defined(OS_ANDROID)
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
  // This object deletes itself on disconnection.
  local_storage_ = new LocalStorageImpl(
      path_.value_or(base::FilePath()), base::SequencedTaskRunnerHandle::Get(),
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
