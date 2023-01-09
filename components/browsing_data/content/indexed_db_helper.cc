// Copyright 2012 The Chromium Authors
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
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

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

void IndexedDBHelper::DeleteIndexedDB(const blink::StorageKey& storage_key,
                                      base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_->GetIndexedDBControl().DeleteForStorageKey(
      storage_key, std::move(callback));
}

void IndexedDBHelper::IndexedDBUsageInfoReceived(
    FetchCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> usages) {
  DCHECK(!callback.is_null());
  std::list<content::StorageUsageInfo> result;
  for (const auto& usage : usages) {
    if (!HasWebScheme(usage->storage_key.origin().GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.emplace_back(usage->storage_key, usage->total_size_bytes,
                        usage->last_modified);
  }
  std::move(callback).Run(std::move(result));
}

CannedIndexedDBHelper::CannedIndexedDBHelper(
    content::StoragePartition* storage_partition)
    : IndexedDBHelper(storage_partition) {}

CannedIndexedDBHelper::~CannedIndexedDBHelper() {}

void CannedIndexedDBHelper::Add(const blink::StorageKey& storage_key) {
  if (!HasWebScheme(storage_key.origin().GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_storage_keys_.insert(storage_key);
}

void CannedIndexedDBHelper::Reset() {
  pending_storage_keys_.clear();
}

bool CannedIndexedDBHelper::empty() const {
  return pending_storage_keys_.empty();
}

size_t CannedIndexedDBHelper::GetCount() const {
  return pending_storage_keys_.size();
}

const std::set<blink::StorageKey>& CannedIndexedDBHelper::GetStorageKeys()
    const {
  return pending_storage_keys_;
}

void CannedIndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& storage_key : pending_storage_keys_) {
    result.emplace_back(storage_key, 0, base::Time());
  }

  std::move(callback).Run(result);
}

void CannedIndexedDBHelper::DeleteIndexedDB(
    const blink::StorageKey& storage_key,
    base::OnceCallback<void(bool)> callback) {
  pending_storage_keys_.erase(storage_key);
  IndexedDBHelper::DeleteIndexedDB(storage_key, std::move(callback));
}

}  // namespace browsing_data
