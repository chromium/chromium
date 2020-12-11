// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_CACHE_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_CACHE_STORAGE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/cache_storage_context.h"
#include "url/origin.h"

namespace content {
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
  explicit CacheStorageHelper(content::CacheStorageContext* context);

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);
  // Requests the Cache Storage data for an origin be deleted.
  virtual void DeleteCacheStorage(const url::Origin& origin);

 protected:
  virtual ~CacheStorageHelper();

  // Owned by the profile.
  content::CacheStorageContext* cache_storage_context_;

 private:
  friend class base::RefCountedThreadSafe<CacheStorageHelper>;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageHelper);
};

// This class is an implementation of CacheStorageHelper that does
// not fetch its information from the Cache Storage context, but is passed the
// info by a call when accessed.
class CannedCacheStorageHelper : public CacheStorageHelper {
 public:
  explicit CannedCacheStorageHelper(content::CacheStorageContext* context);

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

  DISALLOW_COPY_AND_ASSIGN(CannedCacheStorageHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_CACHE_STORAGE_HELPER_H_
