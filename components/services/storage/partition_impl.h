// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PARTITION_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_PARTITION_IMPL_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/services/storage/origin_context_impl.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/origin.h"

namespace storage {

class LocalStorageImpl;
class ServiceWorkerStorageControlImpl;
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

  const auto& origin_contexts() const { return origin_contexts_; }

  // Binds a new client endpoint to this partition.
  void BindReceiver(mojo::PendingReceiver<mojom::Partition> receiver);

  // mojom::Partition:
  void BindOriginContext(
      const url::Origin& origin,
      mojo::PendingReceiver<mojom::OriginContext> receiver) override;
  void BindSessionStorageControl(
      mojo::PendingReceiver<mojom::SessionStorageControl> receiver) override;
  void BindLocalStorageControl(
      mojo::PendingReceiver<mojom::LocalStorageControl> receiver) override;
  void BindServiceWorkerStorageControl(
      mojo::PendingReceiver<mojom::ServiceWorkerStorageControl> receiver)
      override;

 private:
  friend class OriginContextImpl;

  void OnDisconnect();
  void RemoveOriginContext(const url::Origin& origin);

  const raw_ptr<StorageServiceImpl> service_;
  const std::optional<base::FilePath> path_;
  mojo::ReceiverSet<mojom::Partition> receivers_;
  std::map<url::Origin, std::unique_ptr<OriginContextImpl>> origin_contexts_;

  std::unique_ptr<SessionStorageImpl> session_storage_;
  std::unique_ptr<LocalStorageImpl> local_storage_;
  std::unique_ptr<ServiceWorkerStorageControlImpl> service_worker_storage_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PARTITION_IMPL_H_
