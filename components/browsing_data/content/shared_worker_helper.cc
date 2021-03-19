// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/shared_worker_helper.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/storage_partition.h"

namespace browsing_data {

SharedWorkerHelper::SharedWorkerInfo::SharedWorkerInfo(
    const GURL& worker,
    const std::string& name,
    const url::Origin& constructor_origin)
    : worker(worker), name(name), constructor_origin(constructor_origin) {}

SharedWorkerHelper::SharedWorkerInfo::SharedWorkerInfo(
    const SharedWorkerInfo& other) = default;

SharedWorkerHelper::SharedWorkerInfo::~SharedWorkerInfo() = default;

bool SharedWorkerHelper::SharedWorkerInfo::operator<(
    const SharedWorkerInfo& other) const {
  return std::tie(worker, name, constructor_origin) <
         std::tie(other.worker, other.name, other.constructor_origin);
}

SharedWorkerHelper::SharedWorkerHelper(
    content::StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {}

SharedWorkerHelper::~SharedWorkerHelper() = default;

void SharedWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  // We always return an empty list, as there are no "persistent" shared
  // workers.
  std::list<SharedWorkerInfo> result;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}
void SharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name,
    const url::Origin& constructor_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  storage_partition_->GetSharedWorkerService()->TerminateWorker(
      worker, name, constructor_origin);
}

CannedSharedWorkerHelper::CannedSharedWorkerHelper(
    content::StoragePartition* storage_partition)
    : SharedWorkerHelper(storage_partition) {}

CannedSharedWorkerHelper::~CannedSharedWorkerHelper() = default;

void CannedSharedWorkerHelper::AddSharedWorker(
    const GURL& worker,
    const std::string& name,
    const url::Origin& constructor_origin) {
  if (!HasWebScheme(worker))
    return;  // Non-websafe state is not considered browsing data.

  pending_shared_worker_info_.insert(
      SharedWorkerInfo(worker, name, constructor_origin));
}

void CannedSharedWorkerHelper::Reset() {
  pending_shared_worker_info_.clear();
}

bool CannedSharedWorkerHelper::empty() const {
  return pending_shared_worker_info_.empty();
}

size_t CannedSharedWorkerHelper::GetSharedWorkerCount() const {
  return pending_shared_worker_info_.size();
}

const std::set<CannedSharedWorkerHelper::SharedWorkerInfo>&
CannedSharedWorkerHelper::GetSharedWorkerInfo() const {
  return pending_shared_worker_info_;
}

void CannedSharedWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<SharedWorkerInfo> result;
  for (auto& it : pending_shared_worker_info_)
    result.push_back(it);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CannedSharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name,
    const url::Origin& constructor_origin) {
  for (auto it = pending_shared_worker_info_.begin();
       it != pending_shared_worker_info_.end();) {
    if (it->worker == worker && it->name == name &&
        it->constructor_origin == constructor_origin) {
      SharedWorkerHelper::DeleteSharedWorker(it->worker, it->name,
                                             it->constructor_origin);
      it = pending_shared_worker_info_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace browsing_data
