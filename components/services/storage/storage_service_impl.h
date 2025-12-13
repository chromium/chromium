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
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace storage {

class LocalStorageImpl;
class SessionStorageImpl;
// Implementation of the main StorageService Mojo interface. This is the root
// owner of all Storage service instance state, managing the set of active
// persistent and in-memory local and session storage instances.
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

  // mojom::StorageService implementation:
  void EnableAggressiveDomStorageFlushing() override;
#if !BUILDFLAG(IS_ANDROID)
  void SetDataDirectory(
      const base::FilePath& path,
      mojo::PendingRemote<mojom::Directory> directory) override;
#endif
  void BindLocalStorageControl(
      const std::optional<base::FilePath>& path,
      mojo::PendingReceiver<mojom::LocalStorageControl> receiver) override;
  void BindSessionStorageControl(
      const std::optional<base::FilePath>& path,
      mojo::PendingReceiver<mojom::SessionStorageControl> receiver) override;
  void BindTestApi(mojo::ScopedMessagePipeHandle test_api_receiver) override;

  // These transfer ownership of the storage instance to a DeferredDeleter when
  // performing ShutDown. This allows the storage instance to be deleted after
  // ShutDown is complete. This prevents race conditions where a storage
  // instance for a user data directory is rebound while we wait for the
  // previous instance to ShutDown.
  void ShutDownAndRemoveSessionStorage(SessionStorageImpl* storage);
  void ShutDownAndRemoveLocalStorage(LocalStorageImpl* storage);

 private:
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

  // Sets of all isolated local and session storages owned by the service. This
  // includes both persistent and in-memory storages.
  std::set<std::unique_ptr<LocalStorageImpl>, base::UniquePtrComparator>
      local_storages_;
  std::set<std::unique_ptr<SessionStorageImpl>, base::UniquePtrComparator>
      session_storages_;

  // Mappings from a profile directory within the user data directory to the
  // corresponding storage instance in `local_storages` or `session_storages_`.
  // The pointers in these maps are not owned by the map and must be removed
  // when removed from `local_storages_` or `session_storages_`. Only persistent
  // storages have entries in these maps.
  std::map<base::FilePath, raw_ptr<LocalStorageImpl, CtnExperimental>>
      persistent_local_storage_map_;
  std::map<base::FilePath, raw_ptr<SessionStorageImpl, CtnExperimental>>
      persistent_session_storage_map_;

  base::WeakPtrFactory<StorageServiceImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_STORAGE_SERVICE_IMPL_H_
