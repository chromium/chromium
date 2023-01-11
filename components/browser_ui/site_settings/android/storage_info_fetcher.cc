// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/site_settings/android/storage_info_fetcher.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"

using content::BrowserContext;
using content::BrowserThread;

namespace browser_ui {

StorageInfoFetcher::StorageInfoFetcher(content::BrowserContext* context) {
  quota_manager_ = context->GetDefaultStoragePartition()->GetQuotaManager();
}

StorageInfoFetcher::~StorageInfoFetcher() = default;

void StorageInfoFetcher::FetchStorageInfo(FetchCallback fetch_callback) {
  // Balanced in OnFetchCompleted.
  AddRef();

  fetch_callback_ = std::move(fetch_callback);

  // QuotaManager must be called on IO thread, but the callback must then be
  // called on the UI thread.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &StorageInfoFetcher::GetUsageInfo, this,
          base::BindOnce(&StorageInfoFetcher::OnGetUsageInfoInternal, this)));
}

void StorageInfoFetcher::ClearStorage(const std::string& host,
                                      blink::mojom::StorageType type,
                                      ClearCallback clear_callback) {
  // Balanced in OnUsageCleared.
  AddRef();

  clear_callback_ = std::move(clear_callback);
  type_to_delete_ = type;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &storage::QuotaManager::DeleteHostData, quota_manager_, host, type,
          base::BindOnce(&StorageInfoFetcher::OnUsageClearedInternal, this)));
}

void StorageInfoFetcher::GetUsageInfo(storage::GetUsageInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  quota_manager_->GetUsageInfo(std::move(callback));
}

void StorageInfoFetcher::OnGetUsageInfoInternal(
    storage::UsageInfoEntries entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  entries_ = std::move(entries);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&StorageInfoFetcher::OnFetchCompleted, this));
}

void StorageInfoFetcher::OnFetchCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(fetch_callback_).Run(entries_);

  Release();
}

void StorageInfoFetcher::OnUsageClearedInternal(
    blink::mojom::QuotaStatusCode code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  quota_manager_->ResetUsageTracker(type_to_delete_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&StorageInfoFetcher::OnClearCompleted, this, code));
}

void StorageInfoFetcher::OnClearCompleted(blink::mojom::QuotaStatusCode code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(clear_callback_).Run(code);

  Release();
}

}  // namespace browser_ui
