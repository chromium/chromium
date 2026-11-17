// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_H_
#define COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_H_

#include <atomic>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/sqlite_encrypted_vfs/database_key_manager.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_encrypted_vfs {

// Returns true if `db_path` is a non-empty, absolute path without parent (`..`)
// references that converts losslessly to UTF-8 (the only path encoding SQLite
// accepts).
bool IsValidDatabasePath(const base::FilePath& db_path);

// Validates that `db_path` is a non-empty, absolute path without parent (`..`)
// references and resolves it through SQLite's default OS VFS (`xFullPathname`)
// so that symlinked paths resolve to the same canonical path across
// `DatabaseKeyManager`, `EncryptedDatabase`, and `SqliteEncryptedVfs`. Returns
// an empty `base::FilePath` if `db_path` or its resolved path is invalid.
base::FilePath NormalizeDatabasePath(const base::FilePath& db_path);

// Returns true if a `SqliteEncryptedVfs` is currently registered for the
// database at `normalized_db_path` (as returned by `NormalizeDatabasePath()`),
// i.e. another `EncryptedDatabase` already has that database open.
bool IsEncryptedVfsRegisteredForPath(const base::FilePath& normalized_db_path);

// A SQLite VFS that serves exactly one encrypted database.
//
// Each instance registers its own uniquely named `sqlite3_vfs` wrapping
// SQLite's default OS VFS, and carries the key for the single database it was
// created for. Callers select it with
// `sql::DatabaseOptions::set_vfs_name_discouraged(vfs->vfs_name())`, so there
// is no process-wide key state and no path-keyed registry: SQLite hands the
// instance back to us through `sqlite3_vfs::pAppData` on every callback.
//
// Standard filesystem operations (locking, sync, truncation) are delegated to
// the underlying OS VFS. Memory-mapped I/O is disabled by returning a null
// buffer from `xFetch`, because mmap bypasses `xRead` and would expose
// ciphertext to SQLite. Reads and writes are pass-through cleartext until the
// page cryptography lands.
//
// Instances must outlive every `sql::Database` opened against `vfs_name()`.
// Prefer `EncryptedDatabase`, which enforces that ordering for you.
class SqliteEncryptedVfs {
 public:
  // Registers a VFS bound to `db_path` and `key`. Returns `nullptr` if
  // registration fails.
  static std::unique_ptr<SqliteEncryptedVfs> Create(
      const base::FilePath& db_path,
      const DatabaseKey& key);

  SqliteEncryptedVfs(const SqliteEncryptedVfs&) = delete;
  SqliteEncryptedVfs& operator=(const SqliteEncryptedVfs&) = delete;

  ~SqliteEncryptedVfs();

  // Name to pass to `sql::DatabaseOptions::set_vfs_name_discouraged()`. The
  // returned pointer is owned by this instance, which must outlive the
  // `sql::Database` using it (`sql::DatabaseOptions` stores the name as a bare
  // pointer).
  const char* vfs_name() const { return vfs_name_.c_str(); }

  const DatabaseKey& key() const { return key_; }

  // Normalized path of the database this VFS serves.
  const base::FilePath& db_path() const { return db_path_; }

  // True if `path` resolves to the main database file of this VFS.
  bool IsMainDatabase(const base::FilePath& path) const;

  sqlite3_vfs* wrapped_vfs() const { return wrapped_vfs_; }

  // Called by the VFS callbacks to track open handles, so that destroying an
  // instance out from under a live `sql::Database` trips a `CHECK` instead of
  // becoming a use-after-free.
  void OnFileOpened();
  void OnFileClosed();

 private:
  SqliteEncryptedVfs(base::FilePath db_path,
                     DatabaseKey key,
                     std::string vfs_name,
                     sqlite3_vfs* wrapped_vfs);

  const base::FilePath db_path_;
  const DatabaseKey key_;
  const std::string vfs_name_;
  const raw_ptr<sqlite3_vfs> wrapped_vfs_;
  std::atomic<int> open_file_count_{0};

  // Registered with SQLite while this object is alive. Must stay at a stable
  // address, hence this class is neither copyable nor movable.
  sqlite3_vfs vfs_ = {};
};

}  // namespace sqlite_encrypted_vfs

#endif  // COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_H_
