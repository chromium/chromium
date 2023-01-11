// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_storage_helper.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"
#include "url/url_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace browsing_data {

namespace {

// Only websafe state is considered browsing data.
bool HasStorageScheme(const url::Origin& origin) {
  return base::Contains(url::GetWebStorageSchemes(), origin.scheme());
}

void GetUsageInfoCallback(LocalStorageHelper::FetchCallback callback,
                          const std::vector<content::StorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const content::StorageUsageInfo& info : infos) {
    if (HasStorageScheme(info.storage_key.origin()))
      result.push_back(info);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace

LocalStorageHelper::LocalStorageHelper(BrowserContext* context)
    : dom_storage_context_(
          context->GetDefaultStoragePartition()->GetDOMStorageContext()) {
  DCHECK(dom_storage_context_);
}

LocalStorageHelper::~LocalStorageHelper() = default;

void LocalStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  dom_storage_context_->GetLocalStorageUsage(
      base::BindOnce(&GetUsageInfoCallback, std::move(callback)));
}

void LocalStorageHelper::DeleteStorageKey(const blink::StorageKey& storage_key,
                                          base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dom_storage_context_->DeleteLocalStorage(storage_key, std::move(callback));
}

//---------------------------------------------------------

CannedLocalStorageHelper::CannedLocalStorageHelper(
    BrowserContext* context,
    bool update_ignored_empty_keys_on_fetch)
    : LocalStorageHelper(context),
      update_ignored_empty_keys_on_fetch_(update_ignored_empty_keys_on_fetch) {}

void CannedLocalStorageHelper::Add(const blink::StorageKey& storage_key) {
  if (!HasStorageScheme(storage_key.origin()))
    return;

  pending_storage_keys_.insert(storage_key);

  // Note: Assume that `storage_key` isn't currently empty, to avoid a
  //       false-negative in the case that the calling code forgets to call
  //       `UpdateIgnoredEmptyKeys` afterwards.
  non_empty_pending_storage_keys_.insert(storage_key);
}

void CannedLocalStorageHelper::Reset() {
  pending_storage_keys_.clear();
  non_empty_pending_storage_keys_.clear();
}

bool CannedLocalStorageHelper::empty() const {
  return non_empty_pending_storage_keys_.empty();
}

size_t CannedLocalStorageHelper::GetCount() const {
  return non_empty_pending_storage_keys_.size();
}

const std::set<blink::StorageKey>& CannedLocalStorageHelper::GetStorageKeys()
    const {
  return non_empty_pending_storage_keys_;
}

void CannedLocalStorageHelper::UpdateIgnoredEmptyKeysInternal(
    base::OnceClosure done,
    const std::list<content::StorageUsageInfo>& storage_usage_info) {
  non_empty_pending_storage_keys_.clear();

  std::vector<blink::StorageKey> non_empty_storage_keys_list;
  non_empty_storage_keys_list.reserve(storage_usage_info.size());
  for (const auto& usage_info : storage_usage_info)
    non_empty_storage_keys_list.push_back(usage_info.storage_key);
  const base::flat_set<blink::StorageKey> non_empty_storage_keys(
      std::move(non_empty_storage_keys_list));

  for (const auto& storage_key : pending_storage_keys_) {
    if (non_empty_storage_keys.contains(storage_key)) {
      non_empty_pending_storage_keys_.insert(storage_key);
    }
  }

  std::move(done).Run();
}

void CannedLocalStorageHelper::UpdateIgnoredEmptyKeys(base::OnceClosure done) {
  LocalStorageHelper::StartFetching(
      base::BindOnce(&CannedLocalStorageHelper::UpdateIgnoredEmptyKeysInternal,
                     this, std::move(done)));
}

void CannedLocalStorageHelper::StartFetchingInternal(FetchCallback callback) {
  std::list<content::StorageUsageInfo> result;
  for (const auto& storage_key : non_empty_pending_storage_keys_)
    result.emplace_back(storage_key, 0, base::Time());

  std::move(callback).Run(result);
}

void CannedLocalStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  if (update_ignored_empty_keys_on_fetch_) {
    UpdateIgnoredEmptyKeys(
        base::BindOnce(&CannedLocalStorageHelper::StartFetchingInternal, this,
                       std::move(callback)));
  } else {
    StartFetchingInternal(std::move(callback));
  }
}

void CannedLocalStorageHelper::DeleteStorageKey(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  pending_storage_keys_.erase(storage_key);
  non_empty_pending_storage_keys_.erase(storage_key);
  LocalStorageHelper::DeleteStorageKey(storage_key, std::move(callback));
}

CannedLocalStorageHelper::~CannedLocalStorageHelper() = default;

}  // namespace browsing_data
