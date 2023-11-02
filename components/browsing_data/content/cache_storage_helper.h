// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_CACHE_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_CACHE_STORAGE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "url/origin.h"

namespace content {
class StoragePartition;
struct StorageUsageInfo;
}

namespace browsing_data {

// CacheStorageHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored for Cache Storage.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class CacheStorageHelper
    : public base::RefCountedThreadSafe<CacheStorageHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  // Create a CacheStorageHelper instance for the Cache Storage
  // stored in |context|'s associated profile's user data directory.
  explicit CacheStorageHelper(content::StoragePartition* partition);

  CacheStorageHelper(const CacheStorageHelper&) = delete;
  CacheStorageHelper& operator=(const CacheStorageHelper&) = delete;

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);
  // Requests the Cache Storage data for an origin be deleted.
  virtual void DeleteCacheStorage(const url::Origin& origin);

 protected:
  virtual ~CacheStorageHelper();

  // Owned by the profile.
  raw_ptr<content::StoragePartition> partition_;

 private:
  friend class base::RefCountedThreadSafe<CacheStorageHelper>;
};

// This class is an implementation of CacheStorageHelper that does
// not fetch its information from the Cache Storage context, but is passed the
// info by a call when accessed.
class CannedCacheStorageHelper : public CacheStorageHelper {
 public:
  explicit CannedCacheStorageHelper(
      content::StoragePartition* storage_partition);

  CannedCacheStorageHelper(const CannedCacheStorageHelper&) = delete;
  CannedCacheStorageHelper& operator=(const CannedCacheStorageHelper&) = delete;

  // Add a Cache Storage to the set of canned Cache Storages that is
  // returned by this helper.
  void Add(const url::Origin& origin);

  // Clear the list of canned Cache Storages.
  void Reset();

  // True if no Cache Storages are currently stored.
  bool empty() const;

  // Returns the number of currently stored Cache Storages.
  size_t GetCount() const;

  // Returns the current list of Cache Storages.
  const std::set<url::Origin>& GetOrigins() const;

  // CacheStorageHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteCacheStorage(const url::Origin& origin) override;

 private:
  ~CannedCacheStorageHelper() override;

  std::set<url::Origin> pending_origins_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_CACHE_STORAGE_HELPER_H_
