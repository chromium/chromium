// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/partition_impl.h"

#include <utility>

#include "base/bind.h"
#include "components/services/storage/storage_service_impl.h"

namespace storage {

PartitionImpl::PartitionImpl(StorageServiceImpl* service,
                             const base::Optional<base::FilePath>& path)
    : service_(service), path_(path) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &PartitionImpl::OnDisconnect, base::Unretained(this)));
}

PartitionImpl::~PartitionImpl() = default;

void PartitionImpl::BindReceiver(
    mojo::PendingReceiver<mojom::Partition> receiver) {
  DCHECK(receivers_.empty() || path_.has_value())
      << "In-memory partitions must have at most one client.";

  receivers_.Add(this, std::move(receiver));
}

void PartitionImpl::BindOriginContext(
    const url::Origin& origin,
    mojo::PendingReceiver<mojom::OriginContext> receiver) {
  auto iter = origin_contexts_.find(origin);
  if (iter == origin_contexts_.end()) {
    auto result = origin_contexts_.emplace(
        origin, std::make_unique<OriginContextImpl>(this, origin));
    iter = result.first;
  }

  iter->second->BindReceiver(std::move(receiver));
}

void PartitionImpl::OnDisconnect() {
  if (receivers_.empty()) {
    // Deletes |this|.
    service_->RemovePartition(this);
  }
}

void PartitionImpl::RemoveOriginContext(const url::Origin& origin) {
  origin_contexts_.erase(origin);
}

}  // namespace storage
