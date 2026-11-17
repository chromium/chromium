// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/stringprintf.h"
#include "sql/initialization.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_encrypted_vfs {

namespace {

int Unlock(sqlite3_file* sqlite_file, int file_lock);

// Resolves `path` through the OS VFS, so that the same database opened via
// different (e.g. symlinked or relative) paths compares equal.
base::FilePath NormalizePath(sqlite3_vfs* vfs, const base::FilePath& path) {
  if (path.empty() || !vfs || !vfs->xFullPathname) {
    return path;
  }
  std::vector<char> full_path(std::max(vfs->mxPathname + 1, 1024), '\0');
  std::string utf8_path = path.AsUTF8Unsafe();
  int rc = vfs->xFullPathname(vfs, utf8_path.c_str(),
                              static_cast<int>(full_path.size()) - 1,
                              full_path.data());
  if (rc == SQLITE_OK || rc == SQLITE_OK_SYMLINK) {
    return base::FilePath::FromUTF8Unsafe(full_path.data());
  }
  // Fail closed (e.g. `ELOOP` on a symlink loop): falling back to the
  // unresolved path could give one database two different spellings, and thus
  // two different keys.
  return base::FilePath();
}

// Returns the name under which the VFS for the database at the normalized
// `db_path` is registered.
//
// The name embeds a 32-bit hash of the path, so two different databases could
// in principle collide. This is treated as too rare to design around (a
// process has a handful of encrypted databases open at once), and it fails
// closed: `SqliteEncryptedVfs::Create()` refuses to register a second VFS
// under an existing name, so the colliding database fails to open with
// `kPermanentError` rather than ever being served by the other database's VFS
// or key.
std::string GetVfsName(const base::FilePath& normalized_db_path) {
  return base::StringPrintf(
      "sqlite_encrypted_vfs_%08x",
      base::PersistentHash(normalized_db_path.AsUTF8Unsafe()));
}

// Internal representation of `sqlite3_file` for `SqliteEncryptedVfs`.
struct SqliteEncryptedVfsFile {
  RAW_PTR_EXCLUSION const sqlite3_io_methods* methods = nullptr;
  RAW_PTR_EXCLUSION sqlite3_file* wrapped_file = nullptr;
  // The VFS instance that opened this file. Guaranteed to outlive the file:
  // `SqliteEncryptedVfs` `CHECK`s that all of its files are closed before it is
  // destroyed.
  RAW_PTR_EXCLUSION SqliteEncryptedVfs* vfs = nullptr;
  // Key of the database this file belongs to, including its journal and WAL
  // files. Used by the page and stream ciphers.
  std::optional<DatabaseKey> key;
  // True for the main database file, as reported by `SQLITE_OPEN_MAIN_DB`.
  bool is_main_db = false;
};

SqliteEncryptedVfs* GetInstance(sqlite3_vfs* vfs) {
  return static_cast<SqliteEncryptedVfs*>(vfs->pAppData);
}

SqliteEncryptedVfsFile* AsVfsFile(sqlite3_file* wrapper_file) {
  return reinterpret_cast<SqliteEncryptedVfsFile*>(wrapper_file);
}

sqlite3_file* GetWrappedFile(sqlite3_file* wrapper_file) {
  return AsVfsFile(wrapper_file)->wrapped_file;
}

int Close(sqlite3_file* sqlite_file) {
  // Explicitly release any lock held before closing. On Windows, file locks
  // are eventually released on handle closure, but releasing explicitly
  // prevents potential transient `kBusy` errors on immediate reopen.
  Unlock(sqlite_file, SQLITE_LOCK_NONE);

  SqliteEncryptedVfsFile* file = AsVfsFile(sqlite_file);
  int r = file->wrapped_file->pMethods->xClose(file->wrapped_file);

  file->vfs->OnFileClosed();

  // Free the underlying OS file allocated in `Open()` and destroy the wrapper.
  // The storage itself belongs to SQLite, so leave a valid, empty object
  // behind: assigning a default-constructed value would instead run `operator=`
  // over already-destroyed members.
  sqlite3_free(file->wrapped_file);
  file->~SqliteEncryptedVfsFile();
  new (file) SqliteEncryptedVfsFile();
  return r;
}

int Read(sqlite3_file* sqlite_file, void* buf, int amt, sqlite3_int64 ofs) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xRead(wrapped_file, buf, amt, ofs);
}

int Write(sqlite3_file* sqlite_file,
          const void* buf,
          int amt,
          sqlite3_int64 ofs) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xWrite(wrapped_file, buf, amt, ofs);
}

int Truncate(sqlite3_file* sqlite_file, sqlite3_int64 size) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xTruncate(wrapped_file, size);
}

int Sync(sqlite3_file* sqlite_file, int flags) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xSync(wrapped_file, flags);
}

int FileSize(sqlite3_file* sqlite_file, sqlite3_int64* size) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xFileSize(wrapped_file, size);
}

int Lock(sqlite3_file* sqlite_file, int file_lock) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xLock(wrapped_file, file_lock);
}

int Unlock(sqlite3_file* sqlite_file, int file_lock) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xUnlock(wrapped_file, file_lock);
}

int CheckReservedLock(sqlite3_file* sqlite_file, int* result) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xCheckReservedLock(wrapped_file, result);
}

int FileControl(sqlite3_file* sqlite_file, int op, void* arg) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xFileControl(wrapped_file, op, arg);
}

int SectorSize(sqlite3_file* sqlite_file) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xSectorSize(wrapped_file);
}

int DeviceCharacteristics(sqlite3_file* sqlite_file) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xDeviceCharacteristics(wrapped_file);
}

int ShmMap(sqlite3_file* sqlite_file,
           int region,
           int size,
           int extend,
           void volatile** pp) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xShmMap(wrapped_file, region, size, extend,
                                         pp);
}

int ShmLock(sqlite3_file* sqlite_file, int ofst, int n, int flags) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xShmLock(wrapped_file, ofst, n, flags);
}

void ShmBarrier(sqlite3_file* sqlite_file) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  wrapped_file->pMethods->xShmBarrier(wrapped_file);
}

int ShmUnmap(sqlite3_file* sqlite_file, int del) {
  sqlite3_file* wrapped_file = GetWrappedFile(sqlite_file);
  return wrapped_file->pMethods->xShmUnmap(wrapped_file, del);
}

// Memory-mapped I/O (`xFetch`/`xUnfetch`) bypasses `xRead` and reads directly
// from the kernel page cache, which would expose raw ciphertext to SQLite.
// Returning `SQLITE_OK` with `*pp = nullptr` signals to SQLite that the page
// cannot be mapped, causing SQLite to fall back to standard `xRead`/`xWrite`.
int Fetch(sqlite3_file* sqlite_file, sqlite3_int64 off, int amt, void** pp) {
  *pp = nullptr;
  return SQLITE_OK;
}

int Unfetch(sqlite3_file* sqlite_file, sqlite3_int64 off, void* p) {
  return SQLITE_OK;
}

int Open(sqlite3_vfs* vfs,
         const char* file_name,
         sqlite3_file* wrapper_file,
         int desired_flags,
         int* used_flags) {
  SqliteEncryptedVfs* instance = GetInstance(vfs);
  CHECK(instance);

  const bool is_main_db = (desired_flags & SQLITE_OPEN_MAIN_DB) != 0;
  if (is_main_db) {
    // This VFS holds the key for exactly one database. Opening any other main
    // database through it is a programming error.
    CHECK(file_name);
    CHECK(instance->IsMainDatabase(base::FilePath::FromUTF8Unsafe(file_name)));
  }

  sqlite3_vfs* wrapped_vfs = instance->wrapped_vfs();

  // Allocate memory for the underlying VFS file structure according to its
  // required size (`szOsFile`).
  sqlite3_file* wrapped_file =
      static_cast<sqlite3_file*>(sqlite3_malloc(wrapped_vfs->szOsFile));
  if (!wrapped_file) {
    return SQLITE_NOMEM;
  }

  int rc = wrapped_vfs->xOpen(wrapped_vfs, file_name, wrapped_file,
                              desired_flags, used_flags);
  if (rc != SQLITE_OK) {
    sqlite3_free(wrapped_file);
    return rc;
  }

  // Placement-new constructs our wrapper file in the buffer provided by SQLite
  // and binds the underlying wrapped file handle.
  SqliteEncryptedVfsFile* file = AsVfsFile(wrapper_file);
  new (file) SqliteEncryptedVfsFile();
  file->wrapped_file = wrapped_file;
  file->vfs = instance;
  file->is_main_db = is_main_db;
  // Journal, WAL and temporary files of this database inherit its key.
  file->key = instance->key();
  instance->OnFileOpened();

  // TODO(crbug.com/476343961): Consider deduplicating `sqlite3_io_methods`
  // initialization with `sql/vfs_wrapper.cc`.
  if (wrapped_file->pMethods->iVersion == 1) {
    static const sqlite3_io_methods io_methods = {
        1,
        Close,
        Read,
        Write,
        Truncate,
        Sync,
        FileSize,
        Lock,
        Unlock,
        CheckReservedLock,
        FileControl,
        SectorSize,
        DeviceCharacteristics,
    };
    file->methods = &io_methods;
  } else if (wrapped_file->pMethods->iVersion == 2) {
    static const sqlite3_io_methods io_methods = {
        2,
        Close,
        Read,
        Write,
        Truncate,
        Sync,
        FileSize,
        Lock,
        Unlock,
        CheckReservedLock,
        FileControl,
        SectorSize,
        DeviceCharacteristics,
        ShmMap,
        ShmLock,
        ShmBarrier,
        ShmUnmap,
    };
    file->methods = &io_methods;
  } else {
    static const sqlite3_io_methods io_methods = {
        3,
        Close,
        Read,
        Write,
        Truncate,
        Sync,
        FileSize,
        Lock,
        Unlock,
        CheckReservedLock,
        FileControl,
        SectorSize,
        DeviceCharacteristics,
        ShmMap,
        ShmLock,
        ShmBarrier,
        ShmUnmap,
        Fetch,
        Unfetch,
    };
    file->methods = &io_methods;
  }
  return SQLITE_OK;
}

int Delete(sqlite3_vfs* vfs, const char* file_name, int sync_dir) {
  // Note: the sibling key file is deliberately *not* deleted here. SQLite
  // deletes files that share the database's name routinely - in particular the
  // rollback journal on every commit in DELETE journal mode - and the key must
  // survive those. Key file lifetime belongs to
  // `DatabaseKeyManager::DeleteKeyForDatabase()`.
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xDelete(wrapped_vfs, file_name, sync_dir);
}

int Access(sqlite3_vfs* vfs, const char* file_name, int flag, int* res) {
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xAccess(wrapped_vfs, file_name, flag, res);
}

int FullPathname(sqlite3_vfs* vfs,
                 const char* relative_path,
                 int buf_size,
                 char* absolute_path) {
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xFullPathname(wrapped_vfs, relative_path, buf_size,
                                    absolute_path);
}

int Randomness(sqlite3_vfs* vfs, int buf_size, char* buffer) {
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xRandomness(wrapped_vfs, buf_size, buffer);
}

int Sleep(sqlite3_vfs* vfs, int microseconds) {
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xSleep(wrapped_vfs, microseconds);
}

int GetLastError(sqlite3_vfs* vfs, int e, char* s) {
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xGetLastError(wrapped_vfs, e, s);
}

int CurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* now) {
  sqlite3_vfs* wrapped_vfs = GetInstance(vfs)->wrapped_vfs();
  return wrapped_vfs->xCurrentTimeInt64(wrapped_vfs, now);
}

}  // namespace

bool IsValidDatabasePath(const base::FilePath& db_path) {
  if (db_path.empty() || !db_path.IsAbsolute() || db_path.ReferencesParent()) {
    return false;
  }
  // SQLite only accepts UTF-8 paths. `AsUTF8Unsafe()` returns an empty or lossy
  // string for paths that do not convert cleanly, and an empty path would be
  // resolved relative to the current working directory by `xFullPathname`.
  // Reject any path that does not round-trip through UTF-8 exactly.
  const std::string utf8_path = db_path.AsUTF8Unsafe();
  return !utf8_path.empty() &&
         base::FilePath::FromUTF8Unsafe(utf8_path) == db_path;
}

base::FilePath NormalizeDatabasePath(const base::FilePath& db_path) {
  if (!IsValidDatabasePath(db_path)) {
    return base::FilePath();
  }
  sql::EnsureSqliteInitialized();
  sqlite3_vfs* wrapped_vfs = sqlite3_vfs_find(nullptr);
  CHECK(wrapped_vfs);
  base::FilePath normalized = NormalizePath(wrapped_vfs, db_path);
  return IsValidDatabasePath(normalized) ? normalized : base::FilePath();
}

bool IsEncryptedVfsRegisteredForPath(const base::FilePath& normalized_db_path) {
  CHECK(IsValidDatabasePath(normalized_db_path));
  sql::EnsureSqliteInitialized();
  sqlite3_vfs* vfs = sqlite3_vfs_find(GetVfsName(normalized_db_path).c_str());
  // Compare the bound path rather than trusting the name alone, so that a VFS
  // name collision (see `GetVfsName()`) is not mistaken for this database
  // being open.
  return vfs && GetInstance(vfs)->db_path() == normalized_db_path;
}

// static
std::unique_ptr<SqliteEncryptedVfs> SqliteEncryptedVfs::Create(
    const base::FilePath& db_path,
    const DatabaseKey& key) {
  CHECK(IsValidDatabasePath(db_path));

  sql::EnsureSqliteInitialized();
  sqlite3_vfs* wrapped_vfs = sqlite3_vfs_find(nullptr);
  CHECK(wrapped_vfs);

  base::FilePath normalized_path = NormalizePath(wrapped_vfs, db_path);
  if (!IsValidDatabasePath(normalized_path)) {
    return nullptr;
  }
  std::string vfs_name = GetVfsName(normalized_path);
  if (sqlite3_vfs_find(vfs_name.c_str())) {
    return nullptr;
  }

  auto instance = base::WrapUnique(new SqliteEncryptedVfs(
      std::move(normalized_path), key, std::move(vfs_name), wrapped_vfs));

  // Registered without making it default (`makeDflt = 0`): callers opt in
  // explicitly via `sql::DatabaseOptions::set_vfs_name_discouraged()`.
  if (sqlite3_vfs_register(&instance->vfs_, /*makeDflt=*/0) != SQLITE_OK) {
    return nullptr;
  }
  return instance;
}

SqliteEncryptedVfs::SqliteEncryptedVfs(base::FilePath db_path,
                                       DatabaseKey key,
                                       std::string vfs_name,
                                       sqlite3_vfs* wrapped_vfs)
    : db_path_(std::move(db_path)),
      key_(std::move(key)),
      vfs_name_(std::move(vfs_name)),
      wrapped_vfs_(wrapped_vfs) {
  constexpr int kSqliteVfsApiVersion = 3;
  DCHECK_GE(wrapped_vfs->iVersion, kSqliteVfsApiVersion);
  vfs_.iVersion = kSqliteVfsApiVersion;

  // Size of the file buffer SQLite must allocate when opening files.
  vfs_.szOsFile = sizeof(SqliteEncryptedVfsFile);
  vfs_.mxPathname = wrapped_vfs->mxPathname;
  vfs_.pNext = nullptr;
  vfs_.zName = vfs_name_.c_str();
  // SQLite hands this back to every callback, which is how each callback finds
  // the database it belongs to.
  vfs_.pAppData = this;

  vfs_.xOpen = &sqlite_encrypted_vfs::Open;
  vfs_.xDelete = &sqlite_encrypted_vfs::Delete;
  vfs_.xAccess = &sqlite_encrypted_vfs::Access;
  vfs_.xFullPathname = &sqlite_encrypted_vfs::FullPathname;
  // Dynamic extension loading is disabled in Chromium for security.
  vfs_.xDlOpen = nullptr;
  vfs_.xDlError = nullptr;
  vfs_.xDlSym = nullptr;
  vfs_.xDlClose = nullptr;
  vfs_.xRandomness = &sqlite_encrypted_vfs::Randomness;
  vfs_.xSleep = &sqlite_encrypted_vfs::Sleep;
  vfs_.xCurrentTime = nullptr;
  vfs_.xGetLastError = &sqlite_encrypted_vfs::GetLastError;
  DCHECK(wrapped_vfs->xCurrentTimeInt64 != nullptr);
  vfs_.xCurrentTimeInt64 = &sqlite_encrypted_vfs::CurrentTimeInt64;
  vfs_.xSetSystemCall = nullptr;
  vfs_.xGetSystemCall = nullptr;
  vfs_.xNextSystemCall = nullptr;
}

SqliteEncryptedVfs::~SqliteEncryptedVfs() {
  // Destroying the VFS while SQLite still holds files opened through it would
  // be a use-after-free. Close the `sql::Database` first.
  CHECK_EQ(open_file_count_.load(), 0);
  sqlite3_vfs_unregister(&vfs_);
}

bool SqliteEncryptedVfs::IsMainDatabase(const base::FilePath& path) const {
  return NormalizePath(wrapped_vfs_, path) == db_path_;
}

void SqliteEncryptedVfs::OnFileOpened() {
  open_file_count_.fetch_add(1);
}

void SqliteEncryptedVfs::OnFileClosed() {
  CHECK_GT(open_file_count_.fetch_sub(1), 0);
}

}  // namespace sqlite_encrypted_vfs
