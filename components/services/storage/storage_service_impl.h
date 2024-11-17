// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/services/storage/partition_impl.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace storage {

class PartitionImpl;

// Implementation of the main StorageService Mojo interface. This is the root
// owner of all Storage service instance state, managing the set of active
// persistent and in-memory partitions.
class StorageServiceImpl : public mojom::StorageService {
 public:
  // NOTE: |io_task_runner| is only used in sandboxed environments and can be
  // null otherwise. If non-null, it should specify a task runner that will
  // never block and is thus capable of reliably facilitating IPC to the
  // browser.
  StorageServiceImpl(mojo::PendingReceiver<mojom::StorageService> receiver,
                     scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  StorageServiceImpl(const StorageServiceImpl&) = delete;
  StorageServiceImpl& operator=(const StorageServiceImpl&) = delete;

  ~StorageServiceImpl() override;

  const auto& partitions() const { return partitions_; }

  // mojom::StorageService implementation:
  void EnableAggressiveDomStorageFlushing() override;
#if !BUILDFLAG(IS_ANDROID)
  void SetDataDirectory(
      const base::FilePath& path,
      mojo::PendingRemote<mojom::Directory> directory) override;
#endif
  void BindPartition(const std::optional<base::FilePath>& path,
                     mojo::PendingReceiver<mojom::Partition> receiver) override;
  void BindTestApi(mojo::ScopedMessagePipeHandle test_api_receiver) override;

 private:
  friend class PartitionImpl;

  // Removes a partition from the set of tracked partitions.
  void RemovePartition(PartitionImpl* partition);

#if !BUILDFLAG(IS_ANDROID)
  // Binds a Directory receiver to the same remote implementation to which
  // |remote_data_directory_| is bound. It is invalid to call this when
  // |remote_data_directory_| is unbound.
  void BindDataDirectoryReceiver(
      mojo::PendingReceiver<mojom::Directory> receiver);
#endif

  const mojo::Receiver<mojom::StorageService> receiver_;
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

#if !BUILDFLAG(IS_ANDROID)
  // If bound, the service will assume it should not perform certain filesystem
  // operations directly and will instead go through this interface.
  base::FilePath remote_data_directory_path_;
  mojo::Remote<mojom::Directory> remote_data_directory_;
#endif

  // The set of all isolated partitions owned by the service. This includes both
  // persistent and in-memory partitions.
  std::set<std::unique_ptr<PartitionImpl>, base::UniquePtrComparator>
      partitions_;

  // A mapping from FilePath to the corresponding PartitionImpl instance in
  // |partitions_|. The pointers stored here are not owned by this map and must
  // be removed when removed from |partitions_|. Only persistent partitions have
  // entries in this map.
  std::map<base::FilePath, raw_ptr<PartitionImpl, CtnExperimental>>
      persistent_partition_map_;

  base::WeakPtrFactory<StorageServiceImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_
