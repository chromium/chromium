// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_disk_cache.h"

#include <limits>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/cache_type.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"

namespace storage {

// A callback shim that keeps track of cancellation of backend creation.
class ServiceWorkerDiskCache::CreateBackendCallbackShim
    : public base::RefCounted<CreateBackendCallbackShim> {
 public:
  explicit CreateBackendCallbackShim(ServiceWorkerDiskCache* object)
      : service_worker_disk_cache_(object) {}

  void Cancel() { service_worker_disk_cache_ = nullptr; }

  void Callback(disk_cache::BackendResult result) {
    if (service_worker_disk_cache_)
      service_worker_disk_cache_->OnCreateBackendComplete(std::move(result));
  }

 private:
  friend class base::RefCounted<CreateBackendCallbackShim>;

  ~CreateBackendCallbackShim() = default;

  raw_ptr<ServiceWorkerDiskCache>
      service_worker_disk_cache_;  // Unowned pointer.
};

ServiceWorkerDiskCacheEntry::ServiceWorkerDiskCacheEntry(
    disk_cache::Entry* disk_cache_entry,
    ServiceWorkerDiskCache* cache)
    : disk_cache_entry_(disk_cache_entry), cache_(cache) {
  DCHECK(disk_cache_entry);
  DCHECK(cache);
  cache_->AddOpenEntry(this);
}

ServiceWorkerDiskCacheEntry::~ServiceWorkerDiskCacheEntry() {
  if (disk_cache_entry_) {
    disk_cache_entry_->Close();
    cache_->RemoveOpenEntry(this);
  }
}

int ServiceWorkerDiskCacheEntry::Read(int index,
                                      int64_t offset,
                                      net::IOBuffer* buf,
                                      int buf_len,
                                      net::CompletionOnceCallback callback) {
  if (offset < 0 || offset > std::numeric_limits<int32_t>::max())
    return net::ERR_INVALID_ARGUMENT;
  if (!disk_cache_entry_)
    return net::ERR_ABORTED;
  return disk_cache_entry_->ReadData(index, static_cast<int>(offset), buf,
                                     buf_len, std::move(callback));
}

int ServiceWorkerDiskCacheEntry::Write(int index,
                                       int64_t offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       net::CompletionOnceCallback callback) {
  if (offset < 0 || offset > std::numeric_limits<int32_t>::max())
    return net::ERR_INVALID_ARGUMENT;
  if (!disk_cache_entry_)
    return net::ERR_ABORTED;
  const bool kTruncate = true;
  return disk_cache_entry_->WriteData(index, static_cast<int>(offset), buf,
                                      buf_len, std::move(callback), kTruncate);
}

int64_t ServiceWorkerDiskCacheEntry::GetSize(int index) {
  return disk_cache_entry_ ? disk_cache_entry_->GetDataSize(index) : 0L;
}

void ServiceWorkerDiskCacheEntry::Abandon() {
  disk_cache_entry_->Close();
  disk_cache_entry_ = nullptr;
}

ServiceWorkerDiskCache::ServiceWorkerDiskCache() = default;

ServiceWorkerDiskCache::~ServiceWorkerDiskCache() {
  Disable();
}

net::Error ServiceWorkerDiskCache::InitWithDiskBackend(
    const base::FilePath& disk_cache_directory,
    base::OnceClosure post_cleanup_callback,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Init(net::APP_CACHE, disk_cache_directory,
              std::numeric_limits<int64_t>::max(),
              std::move(post_cleanup_callback), std::move(callback));
}

net::Error ServiceWorkerDiskCache::InitWithMemBackend(
    int64_t mem_cache_size,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Init(net::MEMORY_CACHE, base::FilePath(), mem_cache_size,
              base::OnceClosure(), std::move(callback));
}

void ServiceWorkerDiskCache::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_disabled_)
    return;

  is_disabled_ = true;

  if (create_backend_callback_.get()) {
    create_backend_callback_->Cancel();
    create_backend_callback_ = nullptr;
    OnCreateBackendComplete(
        disk_cache::BackendResult::MakeError(net::ERR_ABORTED));
  }

  // We need to close open file handles in order to reinitialize the
  // service worker system on the fly. File handles held in both entries and in
  // the main disk_cache::Backend class need to be released.
  for (ServiceWorkerDiskCacheEntry* entry : open_entries_) {
    entry->Abandon();
  }
  open_entries_.clear();
  disk_cache_.reset();
}

void ServiceWorkerDiskCache::CreateEntry(int64_t key, EntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  if (is_disabled_) {
    std::move(callback).Run(net::ERR_ABORTED, nullptr);
    return;
  }

  if (is_initializing_or_waiting_to_initialize()) {
    // Unretained use is safe here because the callback is stored in
    // `pending_calls_`, which is owned by this instance.
    pending_calls_.emplace_back(
        base::BindOnce(&ServiceWorkerDiskCache::CreateEntry,
                       base::Unretained(this), key, std::move(callback)));
    return;
  }

  if (!disk_cache_) {
    std::move(callback).Run(net::ERR_FAILED, nullptr);
    return;
  }

  uint64_t call_id = GetNextCallId();
  DCHECK(!base::Contains(active_entry_calls_, call_id));
  active_entry_calls_.emplace(call_id, std::move(callback));

  disk_cache::EntryResult result = disk_cache_->CreateEntry(
      base::NumberToString(key), net::HIGHEST,
      base::BindOnce(&ServiceWorkerDiskCache::DidGetEntryResult,
                     weak_factory_.GetWeakPtr(), call_id));
  if (result.net_error() != net::ERR_IO_PENDING) {
    DidGetEntryResult(call_id, std::move(result));
  }
}

void ServiceWorkerDiskCache::OpenEntry(int64_t key, EntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  if (is_disabled_) {
    std::move(callback).Run(net::ERR_ABORTED, nullptr);
    return;
  }

  if (is_initializing_or_waiting_to_initialize()) {
    // Unretained use is safe here because the callback is stored in
    // `pending_calls_`, which is owned by this instance.
    pending_calls_.emplace_back(
        base::BindOnce(&ServiceWorkerDiskCache::OpenEntry,
                       base::Unretained(this), key, std::move(callback)));
    return;
  }

  if (!disk_cache_) {
    std::move(callback).Run(net::ERR_FAILED, nullptr);
    return;
  }

  uint64_t call_id = GetNextCallId();
  DCHECK(!base::Contains(active_entry_calls_, call_id));
  active_entry_calls_.emplace(call_id, std::move(callback));

  disk_cache::EntryResult result = disk_cache_->OpenEntry(
      base::NumberToString(key), net::HIGHEST,
      base::BindOnce(&ServiceWorkerDiskCache::DidGetEntryResult,
                     weak_factory_.GetWeakPtr(), call_id));
  if (result.net_error() != net::ERR_IO_PENDING) {
    DidGetEntryResult(call_id, std::move(result));
  }
}

void ServiceWorkerDiskCache::DoomEntry(int64_t key,
                                       net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  if (is_disabled_) {
    std::move(callback).Run(net::ERR_ABORTED);
    return;
  }

  if (is_initializing_or_waiting_to_initialize()) {
    // Unretained use is safe here because the callback is stored in
    // `pending_calls_`, which is owned by this instance.
    pending_calls_.emplace_back(
        base::BindOnce(&ServiceWorkerDiskCache::DoomEntry,
                       base::Unretained(this), key, std::move(callback)));
    return;
  }

  if (!disk_cache_) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  uint64_t call_id = GetNextCallId();
  DCHECK(!base::Contains(active_doom_calls_, call_id));
  active_doom_calls_.emplace(call_id, std::move(callback));

  net::Error net_error = disk_cache_->DoomEntry(
      base::NumberToString(key), net::HIGHEST,
      base::BindOnce(&ServiceWorkerDiskCache::DidDoomEntry,
                     weak_factory_.GetWeakPtr(), call_id));
  if (net_error != net::ERR_IO_PENDING) {
    DidDoomEntry(call_id, net_error);
  }
}

base::WeakPtr<ServiceWorkerDiskCache> ServiceWorkerDiskCache::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

net::Error ServiceWorkerDiskCache::Init(net::CacheType cache_type,
                                        const base::FilePath& cache_directory,
                                        int64_t cache_size,
                                        base::OnceClosure post_cleanup_callback,
                                        net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_initializing_or_waiting_to_initialize() && !disk_cache_.get());
  is_disabled_ = false;
  create_backend_callback_ =
      base::MakeRefCounted<CreateBackendCallbackShim>(this);

  disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr,
      cache_directory, cache_size, disk_cache::ResetHandling::kNeverReset,
      /*net_log=*/nullptr, std::move(post_cleanup_callback),
      base::BindOnce(&CreateBackendCallbackShim::Callback,
                     create_backend_callback_));
  net::Error rv = result.net_error;
  if (rv == net::ERR_IO_PENDING)
    init_callback_ = std::move(callback);
  else
    OnCreateBackendComplete(std::move(result));
  return rv;
}

void ServiceWorkerDiskCache::OnCreateBackendComplete(
    disk_cache::BackendResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result.net_error == net::OK) {
    disk_cache_ = std::move(result.backend);
  }
  create_backend_callback_ = nullptr;

  // Invoke our clients callback function.
  if (!init_callback_.is_null()) {
    std::move(init_callback_).Run(result.net_error);
  }

  // Service pending calls that were queued up while we were initializing.
  for (auto& call : pending_calls_)
    std::move(call).Run();
  pending_calls_.clear();
}

uint64_t ServiceWorkerDiskCache::GetNextCallId() {
  return next_call_id_++;
}

void ServiceWorkerDiskCache::DidGetEntryResult(uint64_t call_id,
                                               disk_cache::EntryResult result) {
  auto it = active_entry_calls_.find(call_id);
  CHECK(it != active_entry_calls_.end(), base::NotFatalUntil::M130);
  EntryCallback callback = std::move(it->second);
  active_entry_calls_.erase(it);

  net::Error net_error = result.net_error();
  std::unique_ptr<ServiceWorkerDiskCacheEntry> entry;
  if (net_error == net::OK) {
    entry = std::make_unique<ServiceWorkerDiskCacheEntry>(result.ReleaseEntry(),
                                                          this);
  }

  std::move(callback).Run(net_error, std::move(entry));
}

void ServiceWorkerDiskCache::DidDoomEntry(uint64_t call_id, int net_error) {
  auto it = active_doom_calls_.find(call_id);
  CHECK(it != active_doom_calls_.end(), base::NotFatalUntil::M130);
  net::CompletionOnceCallback callback = std::move(it->second);
  active_doom_calls_.erase(it);

  std::move(callback).Run(net_error);
}

void ServiceWorkerDiskCache::AddOpenEntry(ServiceWorkerDiskCacheEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  open_entries_.insert(entry);
}

void ServiceWorkerDiskCache::RemoveOpenEntry(
    ServiceWorkerDiskCacheEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  open_entries_.erase(entry);
}

}  // namespace storage
