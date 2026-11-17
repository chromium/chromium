// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_ENCRYPTED_VFS_ENCRYPTED_DATABASE_H_
#define COMPONENTS_SQLITE_ENCRYPTED_VFS_ENCRYPTED_DATABASE_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "sql/database.h"

namespace sqlite_encrypted_vfs {

class SqliteEncryptedVfs;

// An encrypted `sql::Database`, bundled with the `SqliteEncryptedVfs` instance
// serving it.
//
// This is the consumer-facing surface of encrypted SQLite. Callers never see a
// VFS name, a key, or a key file: once `DatabaseKeyManager` is initialized for
// the process via `DatabaseKeyManager::Init()` and ready (see
// `DatabaseKeyManager::WhenReady()`), callers construct an `EncryptedDatabase`
// and call `Open()` synchronously on their database sequence. Any non-`kOk`
// result from `Open()` means fail closed - there is no plaintext fallback.
//
// Threading: Like `sql::Database`, `EncryptedDatabase` and the `sql::Database`
// returned by `DB()` perform blocking filesystem I/O and must be constructed,
// used, and destroyed on a single sequence that allows blocking
// (`base::MayBlock()`), such as a background worker sequence gated by a
// `base::DeferredSequencedTaskRunner` started from
// `DatabaseKeyManager::WhenReady()`.
class COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS) EncryptedDatabase {
 public:
  enum class OpenResult {
    kOk,
    // Key decryption failed because the OS key provider is temporarily
    // unavailable
    // (`os_crypt_async::Encryptor::DecryptFlags::temporarily_unavailable`).
    // Callers must fail closed without deleting the database or `-key` file, as
    // a subsequent attempt may succeed.
    kTransientError,
    // The database's `-key` file exists but its key can never be recovered
    // with the current OS-level key (e.g. the OS key was lost or reset, or the
    // `-key` file is corrupted). The existing database contents are
    // unreadable; callers may recover by calling `Delete()` and opening a
    // fresh database with a new `EncryptedDatabase`.
    kKeyUnrecoverable,
    // Opening failed for any other non-transient reason (e.g. I/O failure on
    // the `-key` file, invalid path, or SQLite open failure). This does not
    // imply the existing database is unreadable, so callers must not delete it
    // in response.
    kPermanentError,
  };

  // Constructs an unopened `EncryptedDatabase` configured with `options` and
  // `tag`. Call `Open()` to acquire the key, register the VFS, and open the
  // database.
  //
  // `options` is applied as given, except for the settings this class owns:
  // the VFS name and memory-mapped I/O. Those are forced when `Open()` runs, so
  // a caller cannot accidentally open an encrypted database unencrypted, or
  // through a path that bypasses the VFS.
  EncryptedDatabase(sql::DatabaseOptions options, sql::Database::Tag tag);

  EncryptedDatabase(const EncryptedDatabase&) = delete;
  EncryptedDatabase& operator=(const EncryptedDatabase&) = delete;

  ~EncryptedDatabase();

  // Sets or resets the error callback forwarded to the underlying
  // `sql::Database`. May be called before or after `Open()`.
  void set_error_callback(sql::Database::ErrorCallback error_callback);
  void reset_error_callback();

  // Synchronously acquires the database's key from the process-wide
  // `DatabaseKeyManager` singleton, registers a VFS bound to it, and opens the
  // database at `db_path` (which must be an absolute path without parent
  // references). Must be called after `DatabaseKeyManager::WhenReady()` has
  // signaled readiness. There is deliberately no fallback to an unencrypted
  // database.
  //
  // May only be called once per object, even if it fails (`CHECK`ed). To retry,
  // create a new `EncryptedDatabase`.
  OpenResult Open(const base::FilePath& db_path);

  // Returns true if the underlying `sql::Database` is currently open.
  bool is_open() const;

  // The database connection. Remains valid after `Open()` succeeds until this
  // object is destroyed or `Delete()` is called (including if the underlying
  // `sql::Database` is poisoned via `sql::Database::Poison()`). `CHECK`s that
  // `Open()` has succeeded and `Delete()` has not been called.
  sql::Database& DB();

  // Synchronously closes the connection and removes the database, its journal
  // and WAL siblings, and finally its key file. Returns `true` only if every
  // file is gone.
  //
  // The key file is deleted last, and only if the database files were removed:
  // otherwise an unopenable encrypted database would be left behind.
  //
  // Deletes nothing and returns `false` if `Open()` never resolved a database
  // path (not called, invalid path, or the database was already open through
  // another `EncryptedDatabase`). May be called after a failed `Open()` to
  // recover from an unusable database.
  //
  // `DB()` and `Open()` must not be called afterwards; the only valid remaining
  // operation is destruction.
  bool Delete();

 private:
  OpenResult OpenImpl(const base::FilePath& db_path);

  SEQUENCE_CHECKER(sequence_checker_);

  sql::DatabaseOptions options_;
  const sql::Database::Tag tag_;
  sql::Database::ErrorCallback error_callback_;
  base::FilePath db_path_;
  // Set by the first `Open()`, successful or not; `Open()` must not be called
  // again on the same object.
  bool open_called_ = false;
  // Set by `Delete()`; `Open()` must not be called afterwards.
  bool deleted_ = false;
  std::unique_ptr<SqliteEncryptedVfs> vfs_;
  // Declared after `vfs_` so that the connection is torn down first.
  std::unique_ptr<sql::Database> db_;
};

}  // namespace sqlite_encrypted_vfs

#endif  // COMPONENTS_SQLITE_ENCRYPTED_VFS_ENCRYPTED_DATABASE_H_
