// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/backend_storage_delegate.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/numerics/clamped_math.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/vfs_util.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"
#include "components/sqlite_vfs/vfs_utils.h"

namespace persistent_cache::sqlite {

std::optional<PendingBackend> BackendStorageDelegate::MakePendingBackend(
    Client client,
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  ASSIGN_OR_RETURN(auto pending_file_set,
                   sqlite_vfs::MakePendingFileSet(
                       VfsClientFromClient(client), directory, base_name,
                       single_connection, journal_mode_wal));
  return PendingBackend(std::move(pending_file_set));
}

std::unique_ptr<Backend> BackendStorageDelegate::MakeBackend(
    Client client,
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  if (auto pending_backend = MakePendingBackend(
          client, directory, base_name, single_connection, journal_mode_wal);
      pending_backend.has_value()) {
    return SqliteBackendImpl::Bind(*std::move(pending_backend), client);
  }
  return nullptr;
}

std::optional<PendingBackend> BackendStorageDelegate::ShareReadOnlyConnection(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    const Backend& backend) {
  ASSIGN_OR_RETURN(
      auto pending_file_set,
      sqlite_vfs::ShareConnection(
          directory, base_name,
          static_cast<const SqliteBackendImpl&>(backend).file_set(),
          /*read_write=*/false));
  return PendingBackend(std::move(pending_file_set));
}

std::optional<PendingBackend> BackendStorageDelegate::ShareReadWriteConnection(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    const Backend& backend) {
  ASSIGN_OR_RETURN(
      auto pending_file_set,
      sqlite_vfs::ShareConnection(
          directory, base_name,
          static_cast<const SqliteBackendImpl&>(backend).file_set(),
          /*read_write=*/true));
  return PendingBackend(std::move(pending_file_set));
}

base::FilePath BackendStorageDelegate::GetBaseName(const base::FilePath& file) {
  return sqlite_vfs::GetBaseName(file);
}

int64_t BackendStorageDelegate::DeleteFiles(Client client,
                                            const base::FilePath& directory,
                                            const base::FilePath& base_name) {
  return sqlite_vfs::DeleteFiles(VfsClientFromClient(client), directory,
                                 base_name);
}

}  // namespace persistent_cache::sqlite
