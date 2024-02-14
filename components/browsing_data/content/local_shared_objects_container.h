// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_SHARED_OBJECTS_CONTAINER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_SHARED_OBJECTS_CONTAINER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/memory/ref_counted.h"
#include "components/browsing_data/content/cookie_helper.h"

class GURL;

namespace content {
class StoragePartition;
}

namespace browsing_data {
class CannedCacheStorageHelper;
class CannedCookieHelper;
class CannedLocalStorageHelper;

class LocalSharedObjectsContainer {
 public:
  LocalSharedObjectsContainer(
      content::StoragePartition* storage_partition,
      bool ignore_empty_localstorage,
      browsing_data::CookieHelper::IsDeletionDisabledCallback callback);

  LocalSharedObjectsContainer(const LocalSharedObjectsContainer&) = delete;
  LocalSharedObjectsContainer& operator=(const LocalSharedObjectsContainer&) =
      delete;

  ~LocalSharedObjectsContainer();

  // Returns the number of objects stored in the container.
  size_t GetObjectCount() const;

  // Returns the number of objects for the given |origin|.
  size_t GetObjectCountForDomain(const GURL& origin) const;

  // Updates the ignored empty storage keys, which won't be included in the
  // object and domain counts.
  // Note: If `ignore_empty_localstorage` is true, the ignored empty storage
  //       keys are also updated automatically when the storage helper's
  //       `StartFetching` method is called.
  void UpdateIgnoredEmptyStorageKeys(base::OnceClosure done) const;

  // Returns the number of unique sites in the container.
  size_t GetHostCount() const;

  // Returns the set of unique hosts in the container.
  std::set<std::string> GetHosts() const;

  // Returns the number of unique sites for the given |registrable_domain|.
  size_t GetHostCountForDomain(const GURL& registrable_domain) const;

  // Empties the container.
  void Reset();

  CannedCookieHelper* cookies() const { return cookies_.get(); }
  CannedLocalStorageHelper* local_storages() const {
    return local_storages_.get();
  }
  CannedCacheStorageHelper* cache_storages() const {
    return cache_storages_.get();
  }
  CannedLocalStorageHelper* session_storages() const {
    return session_storages_.get();
  }

 private:
  std::map<url::Origin, int> GetObjectCountPerOriginMap() const;

  scoped_refptr<CannedCookieHelper> cookies_;
  scoped_refptr<CannedLocalStorageHelper> local_storages_;
  scoped_refptr<CannedCacheStorageHelper> cache_storages_;
  scoped_refptr<CannedLocalStorageHelper> session_storages_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_SHARED_OBJECTS_CONTAINER_H_
