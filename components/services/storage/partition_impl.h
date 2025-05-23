// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PARTITION_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_PARTITION_IMPL_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace storage {

class LocalStorageImpl;
class SessionStorageImpl;
class StorageServiceImpl;

// A PartitionImpl instance exclusively owns an isolated storage partition
// corresponding to either a persistent filesystem directory or an in-memory
// database.
class PartitionImpl : public mojom::Partition {
 public:
  // |service| owns and outlives this object.
  explicit PartitionImpl(StorageServiceImpl* service,
                         const std::optional<base::FilePath>& path);

  PartitionImpl(const PartitionImpl&) = delete;
  PartitionImpl& operator=(const PartitionImpl&) = delete;

  ~PartitionImpl() override;

  const std::optional<base::FilePath>& path() const { return path_; }

  const mojo::ReceiverSet<mojom::Partition>& receivers() const {
    return receivers_;
  }

  // Binds a new client endpoint to this partition.
  void BindReceiver(mojo::PendingReceiver<mojom::Partition> receiver);

  // mojom::Partition:
  void BindSessionStorageControl(
      mojo::PendingReceiver<mojom::SessionStorageControl> receiver) override;
  void BindLocalStorageControl(
      mojo::PendingReceiver<mojom::LocalStorageControl> receiver) override;
#if BUILDFLAG(IS_MAC)
  void BindLocalStorageControlAndReportLifecycle(
      mojom::LocalStorageLifecycle lifecycle,
      mojo::PendingReceiver<mojom::LocalStorageControl> receiver) override;
#endif  // BUILDFLAG(IS_MAC)

 private:
  void OnDisconnect();

  const raw_ptr<StorageServiceImpl> service_;
  const std::optional<base::FilePath> path_;
  mojo::ReceiverSet<mojom::Partition> receivers_;

  std::unique_ptr<SessionStorageImpl> session_storage_;
  std::unique_ptr<LocalStorageImpl> local_storage_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PARTITION_IMPL_H_
