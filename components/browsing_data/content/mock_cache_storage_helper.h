// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_CACHE_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_CACHE_STORAGE_HELPER_H_

#include <list>
#include <map>

#include "base/functional/callback.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class StoragePartition;
}

namespace browsing_data {

// Mock for CacheStorageHelper.
// Use AddCacheStorageSamples() or add directly to response_ list, then
// call Notify().
class MockCacheStorageHelper : public CacheStorageHelper {
 public:
  explicit MockCacheStorageHelper(content::StoragePartition* storage_partition);

  MockCacheStorageHelper(const MockCacheStorageHelper&) = delete;
  MockCacheStorageHelper& operator=(const MockCacheStorageHelper&) = delete;

  // Adds some StorageUsageInfo samples.
  void AddCacheStorageSamples();

  // Notifies the callback.
  void Notify();

  // Marks all cache storage files as existing.
  void Reset();

  // Returns true if all cache storage files were deleted since the last
  // Reset() invocation.
  bool AllDeleted();

  // CacheStorageHelper.
  void StartFetching(FetchCallback callback) override;
  void DeleteCacheStorage(const blink::StorageKey& storage_key) override;

 private:
  ~MockCacheStorageHelper() override;

  FetchCallback callback_;
  bool fetched_ = false;
  std::map<blink::StorageKey, bool> storage_keys_;
  std::list<content::StorageUsageInfo> response_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_CACHE_STORAGE_HELPER_H_
