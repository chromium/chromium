// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/storage_service_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/partition_impl.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "components/services/storage/sandboxed_vfs_delegate.h"
#include "components/services/storage/test_api_stubs.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "sql/database.h"
#include "sql/sandboxed_vfs.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace storage {

namespace {

// We don't use out-of-process Storage Service on Android, so we can avoid
// pulling all the related code (including Directory mojom) into the build.
#if !BUILDFLAG(IS_ANDROID)
// The name under which we register our own sandboxed VFS instance when running
// out-of-process.
constexpr char kVfsName[] = "storage_service";

using DirectoryBinder =
    base::RepeatingCallback<void(mojo::PendingReceiver<mojom::Directory>)>;
std::unique_ptr<FilesystemProxy> CreateRestrictedFilesystemProxy(
    const base::FilePath& directory_path,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    DirectoryBinder binder,
    scoped_refptr<base::SequencedTaskRunner> binder_task_runner) {
  mojo::PendingRemote<mojom::Directory> directory;
  binder_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(binder, directory.InitWithNewPipeAndPassReceiver()));
  return std::make_unique<FilesystemProxy>(FilesystemProxy::RESTRICTED,
                                           directory_path, std::move(directory),
                                           std::move(io_task_runner));
}
#endif

}  // namespace

StorageServiceImpl::StorageServiceImpl(
    mojo::PendingReceiver<mojom::StorageService> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

StorageServiceImpl::~StorageServiceImpl() = default;

void StorageServiceImpl::EnableAggressiveDomStorageFlushing() {
  StorageAreaImpl::EnableAggressiveCommitDelay();
}

#if !BUILDFLAG(IS_ANDROID)
void StorageServiceImpl::SetDataDirectory(
    const base::FilePath& path,
    mojo::PendingRemote<mojom::Directory> directory) {
  remote_data_directory_path_ = path;
  remote_data_directory_.Bind(std::move(directory));

  // We can assume we must be sandboxed if we're getting a remote data
  // directory handle. Override the default FilesystemProxy factory to produce
  // instances restricted to operations within |path|, which can operate
  // from within a sandbox.
  SetFilesystemProxyFactory(base::BindRepeating(
      &CreateRestrictedFilesystemProxy, remote_data_directory_path_,
      io_task_runner_,
      base::BindRepeating(&StorageServiceImpl::BindDataDirectoryReceiver,
                          weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunner::GetCurrentDefault()));

  // Prevent SQLite from trying to use mmap, as SandboxedVfs does not currently
  // support this.
  //
  // TODO(crbug.com/40144971): Configure this per Database instance.
  sql::Database::DisableMmapByDefault();

  // SQLite needs our VFS implementation to work over a FilesystemProxy. This
  // installs it as the default implementation for the service process.
  sql::SandboxedVfs::Register(
      kVfsName, std::make_unique<SandboxedVfsDelegate>(CreateFilesystemProxy()),
      /*make_default=*/true);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void StorageServiceImpl::BindPartition(
    const std::optional<base::FilePath>& path,
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

void StorageServiceImpl::BindTestApi(
    mojo::ScopedMessagePipeHandle test_api_receiver) {
  GetTestApiBinderForTesting().Run(std::move(test_api_receiver));
}

void StorageServiceImpl::RemovePartition(PartitionImpl* partition) {
  if (partition->path().has_value())
    persistent_partition_map_.erase(partition->path().value());

  auto iter = partitions_.find(partition);
  if (iter != partitions_.end())
    partitions_.erase(iter);
}

#if !BUILDFLAG(IS_ANDROID)
void StorageServiceImpl::BindDataDirectoryReceiver(
    mojo::PendingReceiver<mojom::Directory> receiver) {
  DCHECK(remote_data_directory_.is_bound());
  remote_data_directory_->Clone(std::move(receiver));
}
#endif

}  // namespace storage
