// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/services/storage/partition_impl.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace storage {

class PartitionImpl;

// Implementation of the main StorageService Mojo interface. This is the root
// owner of all Storage service instance state, managing the set of active
// persistent and in-memory partitions.
class StorageServiceImpl : public mojom::StorageService {
 public:
  explicit StorageServiceImpl(
      mojo::PendingReceiver<mojom::StorageService> receiver);
  ~StorageServiceImpl() override;

  const auto& partitions() const { return partitions_; }

  // mojom::StorageService implementation:
  void BindPartition(const base::Optional<base::FilePath>& path,
                     mojo::PendingReceiver<mojom::Partition> receiver) override;

 private:
  friend class PartitionImpl;

  // Removes a partition from the set of tracked partitions.
  void RemovePartition(PartitionImpl* partition);

  const mojo::Receiver<mojom::StorageService> receiver_;

  // The set of all isolated partitions owned by the service. This includes both
  // persistent and in-memory partitions.
  std::set<std::unique_ptr<PartitionImpl>, base::UniquePtrComparator>
      partitions_;

  // A mapping from FilePath to the corresponding PartitionImpl instance in
  // |partitions_|. The pointers stored here are not owned by this map and must
  // be removed when removed from |partitions_|. Only persistent partitions have
  // entries in this map.
  std::map<base::FilePath, PartitionImpl*> persistent_partition_map_;

  DISALLOW_COPY_AND_ASSIGN(StorageServiceImpl);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_
