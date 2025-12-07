// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_cache_storage_helper.h"

#include "base/functional/callback.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace browsing_data {

MockCacheStorageHelper::MockCacheStorageHelper(
    content::StoragePartition* storage_partition)
    : CacheStorageHelper(storage_partition) {}

MockCacheStorageHelper::~MockCacheStorageHelper() = default;

void MockCacheStorageHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
  fetched_ = true;
}

void MockCacheStorageHelper::DeleteCacheStorage(
    const blink::StorageKey& storage_key) {
  ASSERT_TRUE(fetched_);
  ASSERT_TRUE(storage_keys_.find(storage_key) != storage_keys_.end());
  storage_keys_[storage_key] = false;
}

void MockCacheStorageHelper::AddCacheStorageSamples() {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://cshost1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://cshost2:2/"));
  content::StorageUsageInfo info1(blink::StorageKey::CreateFirstParty(kOrigin1),
                                  1, base::Time());
  response_.push_back(info1);
  storage_keys_[blink::StorageKey::CreateFirstParty(kOrigin1)] = true;
  content::StorageUsageInfo info2(blink::StorageKey::CreateFirstParty(kOrigin2),
                                  2, base::Time());
  response_.push_back(info2);
  storage_keys_[blink::StorageKey::CreateFirstParty(kOrigin2)] = true;
}

void MockCacheStorageHelper::Notify() {
  ASSERT_FALSE(callback_.is_null());
  std::move(callback_).Run(response_);
}

void MockCacheStorageHelper::Reset() {
  for (auto& pair : storage_keys_) {
    pair.second = true;
  }
}

bool MockCacheStorageHelper::AllDeleted() {
  for (const auto& pair : storage_keys_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
