// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_BACKEND_IMPL_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_BACKEND_IMPL_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/types/pass_key.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"
#include "sql/database.h"

namespace persistent_cache {

class COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteBackendImpl : public Backend {
 public:
  using Passkey = base::PassKey<SqliteBackendImpl>;
  explicit SqliteBackendImpl(BackendParams backend_params);
  explicit SqliteBackendImpl(SqliteVfsFileSet vfs_file_set);
  ~SqliteBackendImpl() override;

  SqliteBackendImpl(const SqliteBackendImpl&) = delete;
  SqliteBackendImpl(SqliteBackendImpl&&) = delete;
  SqliteBackendImpl& operator=(const SqliteBackendImpl&) = delete;
  SqliteBackendImpl& operator=(SqliteBackendImpl&&) = delete;

  // `Backend`:
  [[nodiscard]] bool Initialize() override;
  [[nodiscard]] std::unique_ptr<Entry> Find(std::string_view key) override;
  void Insert(std::string_view key,
              base::span<const uint8_t> content,
              EntryMetadata metadata) override;
  BackendType GetType() const override;

 private:
  static SqliteVfsFileSet GetVfsFileSetFromParams(BackendParams backend_params);
  base::FilePath database_path_;

  // The set of of `SanboxedFiles` accessible by this backend. This class owns
  // the `SandboxedFiles`.
  SqliteVfsFileSet vfs_file_set_;

  // Owns the registration / unregistration of the `SanboxedFiles` own by this
  // backend to the `SqliteSandboxedVfsDelegate`. Must be defined after
  // `vfs_file_set_` to ensures unregistration occurs before the `vfs_file_set_`
  // is released.
  SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner_;

  // Defined after `unregister_runner_` to ensure that files remain available
  // through the VFS throughout the database's lifetime.
  sql::Database db_;
  bool initialized_ = false;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_BACKEND_IMPL_H_
