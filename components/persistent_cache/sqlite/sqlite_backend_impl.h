// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_BACKEND_IMPL_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_BACKEND_IMPL_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_sandboxed_vfs.h"
#include "sql/database.h"

namespace persistent_cache {

class COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteBackendImpl : public Backend {
 public:
  static std::unique_ptr<Backend> Bind(PendingBackend pending_backend);

  using Passkey = base::PassKey<SqliteBackendImpl>;
  ~SqliteBackendImpl() override;

  SqliteBackendImpl(const SqliteBackendImpl&) = delete;
  SqliteBackendImpl(SqliteBackendImpl&&) = delete;
  SqliteBackendImpl& operator=(const SqliteBackendImpl&) = delete;
  SqliteBackendImpl& operator=(SqliteBackendImpl&&) = delete;

  // `Backend`:
  [[nodiscard]] base::expected<std::optional<EntryMetadata>, TransactionError>
  Find(std::string_view key, BufferProvider buffer_provider) override;
  base::expected<void, TransactionError> Insert(
      std::string_view key,
      base::span<const uint8_t> content,
      EntryMetadata metadata) override;
  BackendType GetType() const override;
  bool IsReadOnly() const override;
  LockState Abandon() override;

  const SqliteVfsFileSet& file_set() const { return vfs_file_set_; }

  // Returns a `SqliteVfsFileSet` holding the state from a `PendingBackend`.
  // Returns no value in case of error (e.g., the shared lock could not be
  // mapped into the process's address space).
  static std::optional<SqliteVfsFileSet> BindToFileSet(
      PendingBackend pending_backend);

 private:
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheTest, RecoveryFromTransientError);

  explicit SqliteBackendImpl(SqliteVfsFileSet vfs_file_set);
  [[nodiscard]] bool Initialize();

  // Returns a SQLite error code in case of failure.
  base::expected<std::optional<EntryMetadata>, int> FindImpl(
      std::string_view key,
      BufferProvider buffer_provider) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Inserts `content` and `metadata` into storage under `key`. Returns a SQLite
  // extended result code in case of error.
  base::expected<void, int> InsertImpl(std::string_view key,
                                       base::span<const uint8_t> content,
                                       EntryMetadata metadata)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the `TransactionError` corresponding to a SQLite extended result
  // code.
  static TransactionError TranslateError(int error_code);

  const base::FilePath database_path_;

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
  std::optional<sql::Database> db_ GUARDED_BY(lock_);

  base::Lock lock_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_BACKEND_IMPL_H_
