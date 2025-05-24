// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/partition_impl.h"

#include <memory>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
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

#if BUILDFLAG(IS_MAC)
void PartitionImpl::BindLocalStorageControlAndReportLifecycle(
    mojom::LocalStorageLifecycle lifecycle,
    mojo::PendingReceiver<mojom::LocalStorageControl> receiver) {
  SCOPED_CRASH_KEY_NUMBER("396030877", "local_storage_lifecycle",
                          static_cast<int>(lifecycle));
  local_storage_ = std::make_unique<LocalStorageImpl>(
      path_.value_or(base::FilePath()),
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(receiver));
}
#endif  // BUILDFLAG(IS_MAC)

void PartitionImpl::OnDisconnect() {
  if (receivers_.empty()) {
    // Deletes |this|.
    service_->RemovePartition(this);
  }
}

}  // namespace storage
