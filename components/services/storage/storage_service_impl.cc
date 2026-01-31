// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/storage_service_impl.h"

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "components/services/storage/filesystem_proxy_factory.h"
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

  // SQLite needs our VFS implementation to work over a FilesystemProxy. This
  // installs it as the default implementation for the service process.
  sql::SandboxedVfs::Register(
      kVfsName, std::make_unique<SandboxedVfsDelegate>(CreateFilesystemProxy()),
      /*make_default=*/true);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void StorageServiceImpl::BindLocalStorageControl(
    const std::optional<base::FilePath>& path,
    mojo::PendingReceiver<mojom::LocalStorageControl> receiver) {
  if (path.has_value()) {
    if (!path->IsAbsolute()) {
      // Refuse to bind LocalStorage for relative paths.
      return;
    }

    // TODO(crbug.com/396030877): Remove this workaround to remove the
    // pre-existing LocalStorage once the issue is resolved.
    auto iter = persistent_local_storage_map_.find(*path);
    if (iter != persistent_local_storage_map_.end()) {
      ShutDownAndRemoveLocalStorage(iter->second);
    }
  }

  auto new_local_storage = std::make_unique<LocalStorageImpl>(
      path.value_or(base::FilePath()),
      base::BindOnce(&StorageServiceImpl::ShutDownAndRemoveLocalStorage,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(receiver));
  if (path.has_value()) {
    persistent_local_storage_map_[*path] = new_local_storage.get();
  }
  local_storages_.insert(std::move(new_local_storage));
}

void StorageServiceImpl::BindSessionStorageControl(
    const std::optional<base::FilePath>& path,
    mojo::PendingReceiver<mojom::SessionStorageControl> receiver) {
  if (path.has_value()) {
    if (!path->IsAbsolute()) {
      // Refuse to bind SessionStorage for relative paths.
      return;
    }

    // TODO(crbug.com/396030877): Remove this workaround to remove the
    // pre-existing SessionStorage once the issue is resolved.
    auto iter = persistent_session_storage_map_.find(*path);
    if (iter != persistent_session_storage_map_.end()) {
      ShutDownAndRemoveSessionStorage(iter->second);
    }
  }

  auto new_session_storage = std::make_unique<SessionStorageImpl>(
      path.value_or(base::FilePath()),
#if BUILDFLAG(IS_ANDROID)
      // On Android there is no support for session storage restoring, and since
      // the restoring code is responsible for database cleanup, we must
      // manually delete the old database here before we open a new one.
      SessionStorageImpl::BackingMode::kClearDiskStateOnOpen,
#else
      path.has_value() ? SessionStorageImpl::BackingMode::kRestoreDiskState
                       : SessionStorageImpl::BackingMode::kNoDisk,
#endif
      base::OnceCallback<void(SessionStorageImpl*)>(
          base::BindOnce(&StorageServiceImpl::ShutDownAndRemoveSessionStorage,
                         weak_ptr_factory_.GetWeakPtr())),
      std::move(receiver));
  if (path.has_value()) {
    persistent_session_storage_map_[*path] = new_session_storage.get();
  }
  session_storages_.insert(std::move(new_session_storage));
}

void StorageServiceImpl::BindTestApi(
    mojo::ScopedMessagePipeHandle test_api_receiver) {
  GetTestApiBinderForTesting().Run(std::move(test_api_receiver));
}

void StorageServiceImpl::ShutDownAndRemoveSessionStorage(
    SessionStorageImpl* storage) {
  if (!storage->GetStoragePartitionDirectory().empty()) {
    persistent_session_storage_map_.erase(
        storage->GetStoragePartitionDirectory());
  }

  auto it = session_storages_.find(storage);
  if (it != session_storages_.end()) {
    session_storages_.erase(it);
  }
}

void StorageServiceImpl::ShutDownAndRemoveLocalStorage(
    LocalStorageImpl* storage) {
  if (!storage->GetStoragePartitionDirectory().empty()) {
    persistent_local_storage_map_.erase(
        storage->GetStoragePartitionDirectory());
  }

  auto it = local_storages_.find(storage);
  if (it != local_storages_.end()) {
    local_storages_.erase(it);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void StorageServiceImpl::BindDataDirectoryReceiver(
    mojo::PendingReceiver<mojom::Directory> receiver) {
  DCHECK(remote_data_directory_.is_bound());
  remote_data_directory_->Clone(std::move(receiver));
}
#endif

}  // namespace storage
