// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs.h"

#include <cstring>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "sql/initialization.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_encrypted_vfs {

namespace {

int Unlock(sqlite3_file* sqlite_file, int file_lock);

// Internal representation of sqlite3_file for SqliteEncryptedVfs.
struct SqliteEncryptedVfsFile {
  RAW_PTR_EXCLUSION const sqlite3_io_methods* methods;
  RAW_PTR_EXCLUSION sqlite3_file* wrapped_file;
};

sqlite3_vfs* GetWrappedVfs(sqlite3_vfs* vfs) {
  return static_cast<sqlite3_vfs*>(vfs->pAppData);
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
  // prevents potential transient kBusy errors on immediate reopen.
  Unlock(sqlite_file, SQLITE_LOCK_NONE);

  SqliteEncryptedVfsFile* file = AsVfsFile(sqlite_file);
  int r = file->wrapped_file->pMethods->xClose(file->wrapped_file);

  // Free the underlying OS file allocated in Open() and invoke the destructor
  // for the wrapper struct before zeroing memory.
  sqlite3_free(file->wrapped_file);
  file->~SqliteEncryptedVfsFile();
  *file = {};
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

// Memory-mapped I/O (xFetch/xUnfetch) bypasses xRead() and reads directly
// from the kernel page cache, which would expose raw ciphertext to SQLite.
// Returning SQLITE_OK with *pp = nullptr signals to SQLite that the page cannot
// be mapped, causing SQLite to fall back to standard xRead/xWrite.
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
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);

  // Allocate memory for the underlying VFS file structure according to its
  // required size (szOsFile).
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

  // TODO(crbug.com/476343961): Consider deduplicating sqlite3_io_methods
  // initialization with sql/vfs_wrapper.cc.
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
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xDelete(wrapped_vfs, file_name, sync_dir);
}

int Access(sqlite3_vfs* vfs, const char* file_name, int flag, int* res) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xAccess(wrapped_vfs, file_name, flag, res);
}

int FullPathname(sqlite3_vfs* vfs,
                 const char* relative_path,
                 int buf_size,
                 char* absolute_path) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xFullPathname(wrapped_vfs, relative_path, buf_size,
                                    absolute_path);
}

int Randomness(sqlite3_vfs* vfs, int buf_size, char* buffer) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xRandomness(wrapped_vfs, buf_size, buffer);
}

int Sleep(sqlite3_vfs* vfs, int microseconds) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xSleep(wrapped_vfs, microseconds);
}

int GetLastError(sqlite3_vfs* vfs, int e, char* s) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xGetLastError(wrapped_vfs, e, s);
}

int CurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* now) {
  sqlite3_vfs* wrapped_vfs = GetWrappedVfs(vfs);
  return wrapped_vfs->xCurrentTimeInt64(wrapped_vfs, now);
}

SqliteEncryptedVfs* g_instance = nullptr;

}  // namespace

SqliteEncryptedVfs::SqliteEncryptedVfs() {
  CHECK(!g_instance);
  g_instance = this;
}

SqliteEncryptedVfs::~SqliteEncryptedVfs() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SqliteEncryptedVfs::Register() {
  sql::EnsureSqliteInitialized();
  GetInstance();
}

// static
SqliteEncryptedVfs* SqliteEncryptedVfs::GetInstance() {
  [[maybe_unused]] static const bool registered = [] {
    if (sqlite3_vfs_find(kSqliteVfsName)) {
      return true;
    }

    // Locate the underlying base VFS to wrap. On Fuchsia, SQLite uses
    // "unix-none" as its base VFS.
    static constexpr const char* kBaseVfsName =
#if BUILDFLAG(IS_FUCHSIA)
        "unix-none";
#else
        nullptr;
#endif
    sqlite3_vfs* wrapped_vfs = sqlite3_vfs_find(kBaseVfsName);
    CHECK(wrapped_vfs);

    struct SqliteVfsDeleter {
      void operator()(sqlite3_vfs* vfs) const { sqlite3_free(vfs); }
    };
    std::unique_ptr<sqlite3_vfs, SqliteVfsDeleter> wrapper_vfs(
        static_cast<sqlite3_vfs*>(sqlite3_malloc(sizeof(sqlite3_vfs))));
    *wrapper_vfs = {};

    constexpr int kSqliteVfsApiVersion = 3;
    wrapper_vfs->iVersion = kSqliteVfsApiVersion;
    DCHECK_GE(wrapped_vfs->iVersion, kSqliteVfsApiVersion);

    // Set size of file buffer that SQLite must allocate when opening files.
    wrapper_vfs->szOsFile = sizeof(SqliteEncryptedVfsFile);
    wrapper_vfs->mxPathname = wrapped_vfs->mxPathname;
    wrapper_vfs->pNext = nullptr;
    wrapper_vfs->zName = kSqliteVfsName;
    // Store the underlying VFS pointer in pAppData so methods can delegate.
    wrapper_vfs->pAppData = wrapped_vfs;

    wrapper_vfs->xOpen = &Open;
    wrapper_vfs->xDelete = &Delete;
    wrapper_vfs->xAccess = &Access;
    wrapper_vfs->xFullPathname = &FullPathname;
    // Dynamic extension loading is disabled in Chromium for security.
    wrapper_vfs->xDlOpen = nullptr;
    wrapper_vfs->xDlError = nullptr;
    wrapper_vfs->xDlSym = nullptr;
    wrapper_vfs->xDlClose = nullptr;
    wrapper_vfs->xRandomness = &Randomness;
    wrapper_vfs->xSleep = &Sleep;
    wrapper_vfs->xCurrentTime = nullptr;
    wrapper_vfs->xGetLastError = &GetLastError;
    DCHECK(wrapped_vfs->xCurrentTimeInt64 != nullptr);
    wrapper_vfs->xCurrentTimeInt64 = &CurrentTimeInt64;
    wrapper_vfs->xSetSystemCall = nullptr;
    wrapper_vfs->xGetSystemCall = nullptr;
    wrapper_vfs->xNextSystemCall = nullptr;

    // Register our VFS without making it default (makeDflt = 0), so callers
    // opt-in explicitly via DatabaseOptions::set_vfs_name_discouraged.
    // Intentionally leaked since VFS registrations outlive the process.
    if (SQLITE_OK == sqlite3_vfs_register(wrapper_vfs.get(), /*makeDflt=*/0)) {
      ANNOTATE_LEAKING_OBJECT_PTR(wrapper_vfs.get());
      wrapper_vfs.release();
    }
    return true;
  }();

  static base::NoDestructor<SqliteEncryptedVfs> instance;
  return instance.get();
}

}  // namespace sqlite_encrypted_vfs
