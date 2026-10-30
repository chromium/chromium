// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_quota_helper_impl.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using content::BrowserThread;

// static
scoped_refptr<BrowsingDataQuotaHelper> BrowsingDataQuotaHelper::Create(
    content::StoragePartition* storage_partition) {
  return base::MakeRefCounted<BrowsingDataQuotaHelperImpl>(
      storage_partition->GetQuotaManagerProxy());
}

void BrowsingDataQuotaHelperImpl::StartFetching(FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  // Query for storage keys. When complete, process the collected quota info.
  quota_manager_proxy_->PostTaskWithQuotaManager(base::BindOnce(
      [](FetchResultCallback callback,
         base::WeakPtr<storage::QuotaManagerImpl> quota_manager) {
        if (!quota_manager) {
          return;
        }
        quota_manager->GetAllStorageKeys(base::BindOnce(
            [](base::WeakPtr<storage::QuotaManagerImpl> quota_manager,
               FetchResultCallback callback,
               const std::set<blink::StorageKey>& storage_keys) {
              if (!quota_manager) {
                return;
              }
              base::ConcurrentCallbacks<QuotaInfo> aggregator;
              for (const blink::StorageKey& storage_key : storage_keys) {
                if (!browsing_data::IsWebScheme(
                        storage_key.origin().scheme())) {
                  continue;
                }
                quota_manager->GetStorageKeyUsageWithBreakdown(
                    storage_key,
                    base::BindOnce(
                        [](const blink::StorageKey& storage_key, int64_t usage,
                           blink::mojom::UsageBreakdownPtr usage_breakdown) {
                          return QuotaInfo{storage_key, usage};
                        },
                        storage_key)
                        .Then(aggregator.CreateCallback()));
              }
              std::move(aggregator).Done(std::move(callback));
            },
            quota_manager, std::move(callback)));
      },
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)),
          std::vector<BrowsingDataQuotaHelper::QuotaInfo>{})));
}

void BrowsingDataQuotaHelperImpl::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    base::OnceClosure completed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  quota_manager_proxy_->DeleteStorageKeyData(
      storage_key, base::SequencedTaskRunner::GetCurrentDefault(),
      base::IgnoreArgs<blink::mojom::QuotaStatusCode>(std::move(completed)));
}

BrowsingDataQuotaHelperImpl::BrowsingDataQuotaHelperImpl(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : quota_manager_proxy_(std::move(quota_manager_proxy)) {
  DCHECK(quota_manager_proxy_);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() = default;
