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

LocalStorageHelper::LocalStorageHelper(
    content::StoragePartition* storage_partition)
    : dom_storage_context_(storage_partition->GetDOMStorageContext()) {
  DCHECK(dom_storage_context_);
}

LocalStorageHelper::~LocalStorageHelper() = default;

void LocalStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  dom_storage_context_->GetLocalStorageUsage(
      base::BindOnce(&GetUsageInfoCallback, std::move(callback)));
}

}  // namespace browsing_data
