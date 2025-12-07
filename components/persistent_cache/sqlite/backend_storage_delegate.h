// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_BACKEND_STORAGE_DELEGATE_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_BACKEND_STORAGE_DELEGATE_H_

#include "base/component_export.h"
#include "components/persistent_cache/backend_storage.h"

namespace persistent_cache {
class SqliteVfsFileSet;
}

namespace persistent_cache::sqlite {

// A delegate that manages storage on behalf of SqliteBackendImpl.
class COMPONENT_EXPORT(PERSISTENT_CACHE) BackendStorageDelegate
    : public BackendStorage::Delegate {
 public:
  // BackendStorage::Delegate:
  std::optional<PendingBackend> MakePendingBackend(
      const base::FilePath& directory,
      const base::FilePath& base_name,
      bool single_connection,
      bool journal_mode_wal) override;
  std::unique_ptr<Backend> MakeBackend(const base::FilePath& directory,
                                       const base::FilePath& base_name,
                                       bool single_connection,
                                       bool journal_mode_wal) override;
  std::optional<PendingBackend> ShareReadOnlyConnection(
      const base::FilePath& directory,
      const base::FilePath& base_name,
      const Backend& backend) override;
  std::optional<PendingBackend> ShareReadWriteConnection(
      const base::FilePath& directory,
      const base::FilePath& base_name,
      const Backend& backend) override;

  // Returns the basename of `file` without its extension if its extension is
  // ".db".
  base::FilePath GetBaseName(const base::FilePath& file) override;

  // Deletes all SQLite files for `base_name` in `directory` (e.g., the .db and
  // .journal files).
  int64_t DeleteFiles(const base::FilePath& directory,
                      const base::FilePath& base_name) override;

  // Returns a new `PendingBackend` sharing the database connection in
  // `directory` for the cache named `base_name` and referenced by `file_set`.
  // The returned instance is granted read-only access if `read_write` is false;
  // otherwise, read/write access.
  std::optional<PendingBackend> ShareConnection(
      const base::FilePath& directory,
      const base::FilePath& base_name,
      const SqliteVfsFileSet& file_set,
      bool read_write);
};

}  // namespace persistent_cache::sqlite

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_BACKEND_STORAGE_DELEGATE_H_
