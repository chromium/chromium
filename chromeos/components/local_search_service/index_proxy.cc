// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/index_proxy.h"

#include "base/optional.h"
#include "chromeos/components/local_search_service/index.h"

namespace chromeos {
namespace local_search_service {

IndexProxy::IndexProxy(Index* index) : index_(index) {
  DCHECK(index_);
}

IndexProxy::~IndexProxy() = default;

void IndexProxy::BindReceiver(
    mojo::PendingReceiver<mojom::IndexProxy> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void IndexProxy::GetSize(GetSizeCallback callback) {
  const uint64_t num_items = index_->GetSize();
  std::move(callback).Run(num_items);
}

void IndexProxy::AddOrUpdate(const std::vector<Data>& data,
                             AddOrUpdateCallback callback) {
  index_->AddOrUpdate(data);
  std::move(callback).Run();
}

void IndexProxy::Delete(const std::vector<std::string>& ids,
                        DeleteCallback callback) {
  const uint64_t num_deleted = index_->Delete(ids);
  std::move(callback).Run(num_deleted);
}

void IndexProxy::Find(const base::string16& query,
                      uint32_t max_results,
                      FindCallback callback) {
  std::vector<Result> results;
  ResponseStatus status = index_->Find(query, max_results, &results);
  if (status != ResponseStatus::kSuccess) {
    std::move(callback).Run(status, base::nullopt);
  } else {
    std::move(callback).Run(status, std::move(results));
  }
}

void IndexProxy::ClearIndex(ClearIndexCallback callback) {
  index_->ClearIndex();
  std::move(callback).Run();
}

}  // namespace local_search_service
}  // namespace chromeos
