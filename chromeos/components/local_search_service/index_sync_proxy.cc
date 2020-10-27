// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/index_sync_proxy.h"

#include "base/optional.h"
#include "chromeos/components/local_search_service/index_sync.h"

namespace chromeos {
namespace local_search_service {

IndexSyncProxy::IndexSyncProxy(IndexSync* index) : index_(index) {
  DCHECK(index_);
}

IndexSyncProxy::~IndexSyncProxy() = default;

void IndexSyncProxy::BindReceiver(
    mojo::PendingReceiver<mojom::IndexSyncProxy> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void IndexSyncProxy::GetSize(GetSizeCallback callback) {
  const uint64_t num_items = index_->GetSizeSync();
  std::move(callback).Run(num_items);
}

void IndexSyncProxy::AddOrUpdate(const std::vector<Data>& data,
                                 AddOrUpdateCallback callback) {
  index_->AddOrUpdateSync(data);
  std::move(callback).Run();
}

void IndexSyncProxy::Delete(const std::vector<std::string>& ids,
                            DeleteCallback callback) {
  const uint64_t num_deleted = index_->DeleteSync(ids);
  std::move(callback).Run(num_deleted);
}

void IndexSyncProxy::Find(const base::string16& query,
                          uint32_t max_results,
                          FindCallback callback) {
  std::vector<Result> results;
  ResponseStatus status = index_->FindSync(query, max_results, &results);
  if (status != ResponseStatus::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
  } else {
    std::move(callback).Run(status, std::move(results));
  }
}

void IndexSyncProxy::ClearIndex(ClearIndexCallback callback) {
  index_->ClearIndexSync();
  std::move(callback).Run();
}

}  // namespace local_search_service
}  // namespace chromeos
