// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace storage {

class ServiceWorkerDiskCache;

// Thin wrapper around disk_cache::Entry.
class ServiceWorkerDiskCacheEntry {
 public:
  // The newly created entry takes ownership of `disk_cache_entry` and closes it
  // on destruction. |cache| must outlive the newly created entry.
  ServiceWorkerDiskCacheEntry(disk_cache::Entry* disk_cache_entry,
                              ServiceWorkerDiskCache* cache);
  ~ServiceWorkerDiskCacheEntry();

  // See `disk_cache::Entry::ReadData()`.
  int Read(int index,
           int64_t offset,
           net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback);

  // See `disk_cache::Entry::WriteData()`.
  int Write(int index,
            int64_t offset,
            net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback);
  int64_t GetSize(int index);

  // Should only be called by ServiceWorkerDiskCache.
  void Abandon();

 private:
  // The disk_cache::Entry is owned by this entry and closed on destruction.
  raw_ptr<disk_cache::Entry, DanglingUntriaged> disk_cache_entry_;

  // The cache that this entry belongs to.
  const raw_ptr<ServiceWorkerDiskCache> cache_;
};

// net::DiskCache wrapper for the cache used by service worker resources.
//
// Provides ways to create/open/doom service worker disk cache entries.
class ServiceWorkerDiskCache {
 public:
  ServiceWorkerDiskCache();
  ~ServiceWorkerDiskCache();

  // Initializes the object to use disk backed storage.
  net::Error InitWithDiskBackend(const base::FilePath& disk_cache_directory,
                                 base::OnceClosure post_cleanup_callback,
                                 net::CompletionOnceCallback callback);

  // Initializes the object to use memory only storage.
  // This is used for Chrome's incognito browsing.
  net::Error InitWithMemBackend(int64_t disk_cache_size,
                                net::CompletionOnceCallback callback);

  void Disable();
  bool is_disabled() const { return is_disabled_; }

  using EntryCallback =
      base::OnceCallback<void(int rv,
                              std::unique_ptr<ServiceWorkerDiskCacheEntry>)>;

  // Creates/opens/dooms a disk cache entry associated with `key`.
  void CreateEntry(int64_t key, EntryCallback callback);
  void OpenEntry(int64_t key, EntryCallback callback);
  void DoomEntry(int64_t key, net::CompletionOnceCallback callback);

  base::WeakPtr<ServiceWorkerDiskCache> GetWeakPtr();

  void set_is_waiting_to_initialize(bool is_waiting_to_initialize) {
    is_waiting_to_initialize_ = is_waiting_to_initialize;
  }

 private:
  class CreateBackendCallbackShim;
  friend class ServiceWorkerDiskCacheEntry;

  bool is_initializing_or_waiting_to_initialize() const {
    return create_backend_callback_.get() != nullptr ||
           is_waiting_to_initialize_;
  }

  net::Error Init(net::CacheType cache_type,
                  const base::FilePath& directory,
                  int64_t cache_size,
                  base::OnceClosure post_cleanup_callback,
                  net::CompletionOnceCallback callback);
  void OnCreateBackendComplete(disk_cache::BackendResult result);

  uint64_t GetNextCallId();

  void DidGetEntryResult(uint64_t call_id, disk_cache::EntryResult result);
  void DidDoomEntry(uint64_t call_id, int net_error);

  // Called by ServiceWorkerDiskCacheEntry constructor.
  void AddOpenEntry(ServiceWorkerDiskCacheEntry* entry);
  // Called by ServiceWorkerDiskCacheEntry destructor.
  void RemoveOpenEntry(ServiceWorkerDiskCacheEntry* entry);

  bool is_disabled_ = false;
  bool is_waiting_to_initialize_ = false;
  net::CompletionOnceCallback init_callback_;
  scoped_refptr<CreateBackendCallbackShim> create_backend_callback_;
  std::vector<base::OnceClosure> pending_calls_;
  uint64_t next_call_id_ = 0;
  std::map</*call_id=*/uint64_t, EntryCallback> active_entry_calls_;
  std::map</*call_id=*/uint64_t, net::CompletionOnceCallback>
      active_doom_calls_;
  std::set<raw_ptr<ServiceWorkerDiskCacheEntry>> open_entries_;
  std::unique_ptr<disk_cache::Backend> disk_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceWorkerDiskCache> weak_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_DISK_CACHE_H_
