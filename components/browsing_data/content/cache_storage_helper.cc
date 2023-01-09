// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cache_storage_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
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

void CacheStorageHelper::DeleteCacheStorage(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(https://crbug.com/1199077): Pass the real StorageKey into this
  // function directly.
  partition_->GetCacheStorageControl()->DeleteForStorageKey(
      blink::StorageKey(origin));
}

CannedCacheStorageHelper::CannedCacheStorageHelper(
    content::StoragePartition* partition)
    : CacheStorageHelper(partition) {}

CannedCacheStorageHelper::~CannedCacheStorageHelper() {}

void CannedCacheStorageHelper::Add(const url::Origin& origin) {
  if (!HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedCacheStorageHelper::Reset() {
  pending_origins_.clear();
}

bool CannedCacheStorageHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedCacheStorageHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedCacheStorageHelper::GetOrigins() const {
  return pending_origins_;
}

void CannedCacheStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(blink::StorageKey(origin), 0, base::Time());

  std::move(callback).Run(result);
}

void CannedCacheStorageHelper::DeleteCacheStorage(const url::Origin& origin) {
  pending_origins_.erase(origin);
  CacheStorageHelper::DeleteCacheStorage(origin);
}

}  // namespace browsing_data
