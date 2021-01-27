// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cache_storage_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/storage_usage_info.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::CacheStorageContext;
using content::StorageUsageInfo;

namespace browsing_data {
namespace {

void GetAllOriginsInfoForCacheStorageCallback(
    CacheStorageHelper::FetchCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> usage_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const storage::mojom::StorageUsageInfoPtr& usage : usage_info) {
    if (!HasWebScheme(usage->origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.emplace_back(content::StorageUsageInfo(
        usage->origin, usage->total_size_bytes, usage->last_modified));
  }

  std::move(callback).Run(result);
}

}  // namespace

CacheStorageHelper::CacheStorageHelper(
    CacheStorageContext* cache_storage_context)
    : cache_storage_context_(cache_storage_context) {
  DCHECK(cache_storage_context_);
}

CacheStorageHelper::~CacheStorageHelper() {}

void CacheStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  cache_storage_context_->GetAllOriginsInfo(base::BindOnce(
      &GetAllOriginsInfoForCacheStorageCallback, std::move(callback)));
}

void CacheStorageHelper::DeleteCacheStorage(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cache_storage_context_->DeleteForOrigin(origin);
}

CannedCacheStorageHelper::CannedCacheStorageHelper(
    content::CacheStorageContext* context)
    : CacheStorageHelper(context) {}

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
    result.emplace_back(origin, 0, base::Time());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CannedCacheStorageHelper::DeleteCacheStorage(const url::Origin& origin) {
  pending_origins_.erase(origin);
  CacheStorageHelper::DeleteCacheStorage(origin);
}

}  // namespace browsing_data
