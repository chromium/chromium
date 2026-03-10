// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;

// static
scoped_refptr<BrowsingDataQuotaHelper> BrowsingDataQuotaHelper::Create(
    content::StoragePartition* storage_partition) {
  return base::MakeRefCounted<BrowsingDataQuotaHelperImpl>(
      storage_partition->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  // Query for storage keys. When complete, process the collected quota info.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &storage::QuotaManager::GetAllStorageKeys, quota_manager_,
          base::BindOnce(&BrowsingDataQuotaHelperImpl::GotStorageKeys,
                         weak_factory_.GetWeakPtr(),
                         base::BindPostTask(content::GetUIThreadTaskRunner({}),
                                            std::move(callback)))));
}

void BrowsingDataQuotaHelperImpl::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    base::OnceClosure completed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &storage::QuotaManager::DeleteStorageKeyData, quota_manager_,
          storage_key,
          base::BindPostTask(content::GetUIThreadTaskRunner({}),
                             base::IgnoreArgs<blink::mojom::QuotaStatusCode>(
                                 std::move(completed)))));
}

BrowsingDataQuotaHelperImpl::BrowsingDataQuotaHelperImpl(
    storage::QuotaManager* quota_manager)
    : BrowsingDataQuotaHelper(), quota_manager_(quota_manager) {
  DCHECK(quota_manager);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() = default;

void BrowsingDataQuotaHelperImpl::GotStorageKeys(
    FetchResultCallback callback,
    const std::set<blink::StorageKey>& storage_keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::ConcurrentCallbacks<QuotaInfo> aggregator;
  for (const blink::StorageKey& storage_key : storage_keys) {
    if (!browsing_data::IsWebScheme(storage_key.origin().scheme())) {
      continue;  // Non-websafe state is not considered browsing data.
    }
    quota_manager_->GetStorageKeyUsageWithBreakdown(
        storage_key, base::BindOnce(
                         [](const blink::StorageKey& storage_key, int64_t usage,
                            blink::mojom::UsageBreakdownPtr usage_breakdown) {
                           return QuotaInfo{storage_key, usage};
                         },
                         storage_key)
                         .Then(aggregator.CreateCallback()));
  }

  std::move(aggregator)
      .Done(base::BindPostTask(content::GetUIThreadTaskRunner({}),
                               std::move(callback)));
}
