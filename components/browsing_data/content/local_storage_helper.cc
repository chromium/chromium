// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_storage_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
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
    if (HasStorageScheme(info.origin))
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

CannedLocalStorageHelper::CannedLocalStorageHelper(BrowserContext* context)
    : LocalStorageHelper(context) {}

void CannedLocalStorageHelper::Add(const blink::StorageKey& storage_key) {
  if (!HasStorageScheme(storage_key.origin()))
    return;
  pending_storage_keys_.insert(storage_key);
}

void CannedLocalStorageHelper::Reset() {
  pending_storage_keys_.clear();
}

bool CannedLocalStorageHelper::empty() const {
  return pending_storage_keys_.empty();
}

size_t CannedLocalStorageHelper::GetCount() const {
  return pending_storage_keys_.size();
}

const std::set<blink::StorageKey>& CannedLocalStorageHelper::GetStorageKeys()
    const {
  return pending_storage_keys_;
}

void CannedLocalStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const auto& storage_key : pending_storage_keys_)
    result.emplace_back(
        // TODO(https://crbug.com/1199077): Pass the real StorageKey when
        // StorageUsageInfo is converted.
        storage_key.origin(), 0, base::Time());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CannedLocalStorageHelper::DeleteStorageKey(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  pending_storage_keys_.erase(storage_key);
  LocalStorageHelper::DeleteStorageKey(storage_key, std::move(callback));
}

CannedLocalStorageHelper::~CannedLocalStorageHelper() = default;

}  // namespace browsing_data
