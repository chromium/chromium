// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cache_storage_helper.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::StorageUsageInfo;

namespace browsing_data {
namespace {

void GetAllStorageKeysInfoForCacheStorageCallback(
    CacheStorageHelper::FetchCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> usage_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const storage::mojom::StorageUsageInfoPtr& usage : usage_info) {
    if (!HasWebScheme(usage->storage_key.origin().GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.emplace_back(usage->storage_key, usage->total_size_bytes,
                        usage->last_modified);
  }

  std::move(callback).Run(result);
}

}  // namespace

CacheStorageHelper::CacheStorageHelper(content::StoragePartition* partition)
    : partition_(partition) {
  DCHECK(partition);
}

CacheStorageHelper::~CacheStorageHelper() {}

void CacheStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  partition_->GetCacheStorageControl()->GetAllStorageKeysInfo(base::BindOnce(
      &GetAllStorageKeysInfoForCacheStorageCallback, std::move(callback)));
}

void CacheStorageHelper::DeleteCacheStorage(
    const blink::StorageKey& storage_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  partition_->GetCacheStorageControl()->DeleteForStorageKey(storage_key);
}

CannedCacheStorageHelper::CannedCacheStorageHelper(
    content::StoragePartition* partition)
    : CacheStorageHelper(partition) {}

CannedCacheStorageHelper::~CannedCacheStorageHelper() {}

void CannedCacheStorageHelper::Add(const blink::StorageKey& storage_key) {
  if (!HasWebScheme(storage_key.origin().GetURL())) {
    return;  // Non-websafe state is not considered browsing data.
  }

  pending_storage_key_.insert(storage_key);
}

void CannedCacheStorageHelper::Reset() {
  pending_storage_key_.clear();
}

bool CannedCacheStorageHelper::empty() const {
  return pending_storage_key_.empty();
}

size_t CannedCacheStorageHelper::GetCount() const {
  return pending_storage_key_.size();
}

const std::set<blink::StorageKey>& CannedCacheStorageHelper::GetStorageKeys()
    const {
  return pending_storage_key_;
}

void CannedCacheStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& storage_key : pending_storage_key_) {
    result.emplace_back(storage_key, 0, base::Time());
  }

  std::move(callback).Run(result);
}

void CannedCacheStorageHelper::DeleteCacheStorage(
    const blink::StorageKey& storage_key) {
  pending_storage_key_.erase(storage_key);
  CacheStorageHelper::DeleteCacheStorage(storage_key);
}

}  // namespace browsing_data
