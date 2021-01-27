// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/indexed_db_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"

using content::BrowserThread;
using content::StorageUsageInfo;

namespace browsing_data {

IndexedDBHelper::IndexedDBHelper(content::StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {
  DCHECK(storage_partition_);
}

IndexedDBHelper::~IndexedDBHelper() {}

void IndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  storage_partition_->GetIndexedDBControl().GetUsage(
      base::BindOnce(&IndexedDBHelper::IndexedDBUsageInfoReceived,
                     base::WrapRefCounted(this), std::move(callback)));
}

void IndexedDBHelper::DeleteIndexedDB(const url::Origin& origin,
                                      base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_->GetIndexedDBControl().DeleteForOrigin(
      origin, std::move(callback));
}

void IndexedDBHelper::IndexedDBUsageInfoReceived(
    FetchCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> origins) {
  DCHECK(!callback.is_null());
  std::list<content::StorageUsageInfo> result;
  for (const auto& origin_usage : origins) {
    if (!HasWebScheme(origin_usage->origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.emplace_back(StorageUsageInfo(origin_usage->origin,
                                         origin_usage->total_size_bytes,
                                         origin_usage->last_modified));
  }
  std::move(callback).Run(std::move(result));
}

CannedIndexedDBHelper::CannedIndexedDBHelper(
    content::StoragePartition* storage_partition)
    : IndexedDBHelper(storage_partition) {}

CannedIndexedDBHelper::~CannedIndexedDBHelper() {}

void CannedIndexedDBHelper::Add(const url::Origin& origin) {
  if (!HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedIndexedDBHelper::Reset() {
  pending_origins_.clear();
}

bool CannedIndexedDBHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedIndexedDBHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedIndexedDBHelper::GetOrigins() const {
  return pending_origins_;
}

void CannedIndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CannedIndexedDBHelper::DeleteIndexedDB(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  pending_origins_.erase(origin);
  IndexedDBHelper::DeleteIndexedDB(origin, std::move(callback));
}

}  // namespace browsing_data
