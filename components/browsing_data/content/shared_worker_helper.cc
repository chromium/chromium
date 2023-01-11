// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/shared_worker_helper.h"

#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace browsing_data {

SharedWorkerHelper::SharedWorkerInfo::SharedWorkerInfo(
    const GURL& worker,
    const std::string& name,
    const blink::StorageKey& storage_key)
    : worker(worker), name(name), storage_key(storage_key) {}

SharedWorkerHelper::SharedWorkerInfo::SharedWorkerInfo(
    const SharedWorkerInfo& other) = default;

SharedWorkerHelper::SharedWorkerInfo::~SharedWorkerInfo() = default;

bool SharedWorkerHelper::SharedWorkerInfo::operator<(
    const SharedWorkerInfo& other) const {
  return std::tie(worker, name, storage_key) <
         std::tie(other.worker, other.name, other.storage_key);
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
    const blink::StorageKey& storage_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  storage_partition_->GetSharedWorkerService()->TerminateWorker(worker, name,
                                                                storage_key);
}

CannedSharedWorkerHelper::CannedSharedWorkerHelper(
    content::StoragePartition* storage_partition)
    : SharedWorkerHelper(storage_partition) {}

CannedSharedWorkerHelper::~CannedSharedWorkerHelper() = default;

void CannedSharedWorkerHelper::AddSharedWorker(
    const GURL& worker,
    const std::string& name,
    const blink::StorageKey& storage_key) {
  if (!HasWebScheme(worker))
    return;  // Non-websafe state is not considered browsing data.

  pending_shared_worker_info_.insert(
      SharedWorkerInfo(worker, name, storage_key));
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
  std::move(callback).Run(result);
}

void CannedSharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name,
    const blink::StorageKey& storage_key) {
  for (auto it = pending_shared_worker_info_.begin();
       it != pending_shared_worker_info_.end();) {
    if (it->worker == worker && it->name == name &&
        it->storage_key == storage_key) {
      SharedWorkerHelper::DeleteSharedWorker(it->worker, it->name,
                                             it->storage_key);
      it = pending_shared_worker_info_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace browsing_data
