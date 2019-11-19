// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/storage_service_impl.h"

#include "components/services/storage/partition_impl.h"

namespace storage {

StorageServiceImpl::StorageServiceImpl(
    mojo::PendingReceiver<mojom::StorageService> receiver)
    : receiver_(this, std::move(receiver)) {}

StorageServiceImpl::~StorageServiceImpl() = default;

void StorageServiceImpl::BindPartition(
    const base::Optional<base::FilePath>& path,
    mojo::PendingReceiver<mojom::Partition> receiver) {
  if (path.has_value()) {
    if (!path->IsAbsolute()) {
      // Refuse to bind Partitions for relative paths.
      return;
    }

    // If this is a persistent partition that already exists, bind to it and
    // we're done.
    auto iter = persistent_partition_map_.find(*path);
    if (iter != persistent_partition_map_.end()) {
      iter->second->BindReceiver(std::move(receiver));
      return;
    }
  }

  auto new_partition = std::make_unique<PartitionImpl>(this, path);
  new_partition->BindReceiver(std::move(receiver));
  if (path.has_value())
    persistent_partition_map_[*path] = new_partition.get();
  partitions_.insert(std::move(new_partition));
}

void StorageServiceImpl::RemovePartition(PartitionImpl* partition) {
  if (partition->path().has_value())
    persistent_partition_map_.erase(partition->path().value());

  auto iter = partitions_.find(partition);
  if (iter != partitions_.end())
    partitions_.erase(iter);
}

}  // namespace storage
