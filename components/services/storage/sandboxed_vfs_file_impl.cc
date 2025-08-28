// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/sandboxed_vfs_file_impl.h"

#include <cstring>

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "sql/sandboxed_vfs.h"

namespace storage {

SandboxedVfsFileImpl::SandboxedVfsFileImpl(base::File file,
                                           base::FilePath file_path,
                                           sql::SandboxedVfsFileType file_type,
                                           sql::SandboxedVfs* vfs)
    : SandboxedVfsFile(),
      file_(std::move(file)),
      sqlite_lock_mode_(SQLITE_LOCK_NONE),
      vfs_(vfs),
      file_type_(file_type),
      file_path_(std::move(file_path)) {}

SandboxedVfsFileImpl::~SandboxedVfsFileImpl() = default;

int SandboxedVfsFileImpl::Close() {
  file_.Close();
  delete this;
  return SQLITE_OK;
}

int SandboxedVfsFileImpl::Read(void* buffer, int size, sqlite3_int64 offset) {
  DCHECK(buffer);
  DCHECK_GE(size, 0);
  DCHECK_GE(offset, 0);

  // See http://www.sqlite.org/fileformat2.html#database_header
  constexpr int kSqliteDatabaseHeaderOffset = 0;
  constexpr int kSqliteDatabaseHeaderSize = 100;
  // SQLite's locking protocol only acquires locks on the database file. The
  // journal and the WAL file are always unlocked. Also, as an optimization,
  // SQLite first reads the database header without locking the file.
  DCHECK(sqlite_lock_mode_ > SQLITE_LOCK_NONE ||
         file_type_ != sql::SandboxedVfsFileType::kDatabase ||
         (offset == kSqliteDatabaseHeaderOffset &&
          size == kSqliteDatabaseHeaderSize))
      << "Read from database file with lock mode " << sqlite_lock_mode_
      << "of size" << size << " at offset " << offset;

  char* data = reinterpret_cast<char*>(buffer);

  // If we supported mmap()ed files, we'd check for a memory mapping here,
  // and try to fill as much of the request as possible from the mmap()ed
  // region.

  int bytes_read = UNSAFE_TODO(file_.Read(offset, data, size));
  DCHECK_LE(bytes_read, size);
  if (bytes_read == size) {
    return SQLITE_OK;
  }

  if (bytes_read < 0) {
    // SQLite first reads the database header without locking the file. On
    // Windows, this read will fail if there is an exclusive lock on the file,
    // even if the current process owns that lock.
    if (sqlite_lock_mode_ == SQLITE_LOCK_NONE) {
      // The unlocked read is considered an optimization. SQLite can continue
      // even if the read fails, as long as failure is communicated by zeroing
      // out the output buffer.
      UNSAFE_TODO(std::memset(data, 0, size));
      return SQLITE_OK;
    }

    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_READ;
  }

  // SQLite requires that we fill the unread bytes in the buffer with zeros.
  UNSAFE_TODO(std::memset(data + bytes_read, 0, size - bytes_read));
  return SQLITE_IOERR_SHORT_READ;
}

int SandboxedVfsFileImpl::Write(const void* buffer,
                                int size,
                                sqlite3_int64 offset) {
  DCHECK(buffer);
  DCHECK_GE(size, 0);
  DCHECK_GE(offset, 0);

  // SQLite's locking protocol only acquires locks on the database file. The
  // journal and the WAL file are always unlocked.
  DCHECK(sqlite_lock_mode_ == SQLITE_LOCK_EXCLUSIVE ||
         file_type_ != sql::SandboxedVfsFileType::kDatabase)
      << "Write to database file with lock mode " << sqlite_lock_mode_;

  const char* data = reinterpret_cast<const char*>(buffer);

  // If we supported mmap()ed files, we'd check for a memory mapping here,
  // and try to fill as much of the request as possible by copying to the
  // mmap()ed region.

  int bytes_written = UNSAFE_TODO(file_.Write(offset, data, size));
  DCHECK_LE(bytes_written, size);
  if (bytes_written >= size) {
    return SQLITE_OK;
  }

  base::File::Error last_error = base::File::GetLastFileError();
  vfs_->SetLastError(last_error);
  if (last_error == base::File::Error::FILE_ERROR_NO_SPACE) {
    return SQLITE_FULL;
  }

  return SQLITE_IOERR_WRITE;
}

int SandboxedVfsFileImpl::Truncate(sqlite3_int64 size) {
  if (file_.SetLength(size)) {
    return SQLITE_OK;
  }

  return SQLITE_IOERR_TRUNCATE;
}

int SandboxedVfsFileImpl::Sync(int flags) {
  // NOTE: SQLite passes in (SQLITE_SYNC_NORMAL or SQLITE_SYNC_FULL),
  //       potentially OR-ed with SQLITE_SYNC_DATAONLY. Implementing these could
  //       lead to better performance.
  if (!file_.Flush()) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_FSYNC;
  }

  // The unix VFS also syncs the file's directory on the first xSync() call.
  // Chrome's LevelDB Env implementation does the same for specific files
  // (database manifests).
  //
  // For WebSQL, we would want to sync the directory at file open time, when the
  // file is opened for writing.

  return SQLITE_OK;
}

int SandboxedVfsFileImpl::FileSize(sqlite3_int64* result_size) {
  int64_t length = file_.GetLength();
  if (length < 0) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_FSTAT;
  }

  // SQLite's unix VFS reports 1-byte files as empty. This is documented as a
  // workaround for a fairly obscure bug. See unixFileSize() in os_unix.c.
  if (length == 1) {
    length = 0;
  }

  *result_size = length;
  return SQLITE_OK;
}

namespace {

// True if our simplified implementation uses an exclusive lock for a mode.
bool IsExclusiveLockMode(int sqlite_lock_mode) {
  switch (sqlite_lock_mode) {
    case SQLITE_LOCK_NONE:
    case SQLITE_LOCK_SHARED:
      return false;

    case SQLITE_LOCK_RESERVED:
    case SQLITE_LOCK_PENDING:
    case SQLITE_LOCK_EXCLUSIVE:
      return true;
  }

  NOTREACHED() << "Unsupported mode: " << sqlite_lock_mode;
}

}  // namespace

int SandboxedVfsFileImpl::Lock(int mode) {
  DCHECK_GE(mode, sqlite_lock_mode_)
      << "SQLite asked the VFS to lock the file up to mode " << mode
      << " but the file is already locked at mode " << sqlite_lock_mode_;

#if BUILDFLAG(IS_FUCHSIA)
  return SQLITE_IOERR_LOCK;
#else
  base::File::LockMode file_lock_mode = base::File::LockMode::kExclusive;

  switch (mode) {
    case SQLITE_LOCK_NONE:
      return SQLITE_OK;

    case SQLITE_LOCK_SHARED:
      if (sqlite_lock_mode_ != SQLITE_LOCK_NONE) {
        return SQLITE_OK;
      }

      file_lock_mode = base::File::LockMode::kShared;
      break;

    case SQLITE_LOCK_RESERVED:
      // A SHARED lock is required before a RESERVED lock is acquired.
      DCHECK_EQ(sqlite_lock_mode_, SQLITE_LOCK_SHARED);
      file_lock_mode = base::File::LockMode::kExclusive;
      break;

    case SQLITE_LOCK_PENDING:
      NOTREACHED() << "SQLite never directly asks for PENDING locks";

    case SQLITE_LOCK_EXCLUSIVE:
      // A SHARED lock is required before an EXCLUSIVE lock is acquired.
      //
      // No higher level is required. In fact, SQLite upgrades the lock directly
      // from SHARED to EXCLUSIVE when rolling back a transaction, to avoid
      // having other readers queue up in the RESERVED state.
      DCHECK_GE(sqlite_lock_mode_, SQLITE_LOCK_SHARED);

      if (IsExclusiveLockMode(sqlite_lock_mode_)) {
        sqlite_lock_mode_ = mode;
        return SQLITE_OK;
      }
      file_lock_mode = base::File::LockMode::kExclusive;
      break;

    default:
      NOTREACHED() << "Unimplemented xLock() mode: " << mode;
  }

  DCHECK_EQ(IsExclusiveLockMode(mode),
            file_lock_mode == base::File::LockMode::kExclusive)
      << "Incorrect file_lock_mode logic for SQLite mode: " << mode;

  // On POSIX, it would be possible to upgrade atomically from a shared lock to
  // an exclusive lock. This implementation prioritizes the simplicity of no
  // platform-specific code over being faster in high contention cases.
  if (sqlite_lock_mode_ != SQLITE_LOCK_NONE) {
    base::File::Error error = file_.Unlock();
    if (error != base::File::FILE_OK) {
      vfs_->SetLastError(base::File::GetLastFileError());
      return SQLITE_IOERR_LOCK;
    }
    sqlite_lock_mode_ = SQLITE_LOCK_NONE;
  }

  base::File::Error error = file_.Lock(file_lock_mode);
  if (error != base::File::FILE_OK) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_LOCK;
  }

  sqlite_lock_mode_ = mode;
  return SQLITE_OK;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

int SandboxedVfsFileImpl::Unlock(int mode) {
  // The 2nd term in the DCHECK predicate is there because SQLite occasionally
  // attempts to unlock (to SQLITE_LOCK_NONE) a file that was already unlocked.
  // We're not aware of any other case of no-op VFS unlock calls.
  DCHECK(mode < sqlite_lock_mode_ ||
         (mode == sqlite_lock_mode_ && mode == SQLITE_LOCK_NONE))
      << "SQLite asked the VFS to unlock the file down to mode " << mode
      << " but the file is already at mode " << sqlite_lock_mode_;

  // No-op if we're already unlocked or at the requested mode.
  if (sqlite_lock_mode_ == mode || sqlite_lock_mode_ == SQLITE_LOCK_NONE) {
    return SQLITE_OK;
  }

#if BUILDFLAG(IS_FUCHSIA)
  return SQLITE_IOERR_UNLOCK;
#else
  // On POSIX, it is possible to downgrade atomically from an exclusive lock to
  // a shared lock. SQLite's unix VFS takes advantage of this. This
  // implementation prioritizes the simplicity of no platform-specific code over
  // being faster in high contention cases.
  base::File::Error error = file_.Unlock();
  if (error != base::File::FILE_OK) {
    vfs_->SetLastError(base::File::GetLastFileError());
    return SQLITE_IOERR_UNLOCK;
  }

  if (mode == SQLITE_LOCK_NONE) {
    sqlite_lock_mode_ = mode;
    return SQLITE_OK;
  }

  DCHECK_EQ(mode, SQLITE_LOCK_SHARED);
  error = file_.Lock(base::File::LockMode::kShared);
  if (error == base::File::FILE_OK) {
    sqlite_lock_mode_ = mode;
    return SQLITE_OK;
  }

  // Gave up the exclusive lock, but failed to get a shared lock.
  vfs_->SetLastError(base::File::GetLastFileError());
  sqlite_lock_mode_ = SQLITE_LOCK_NONE;
  return SQLITE_IOERR_UNLOCK;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

int SandboxedVfsFileImpl::CheckReservedLock(int* has_reserved_lock) {
  if (IsExclusiveLockMode(sqlite_lock_mode_)) {
    *has_reserved_lock = 1;
    return SQLITE_OK;
  }

  if (sqlite_lock_mode_ == SQLITE_LOCK_SHARED) {
    // Lock modes at or above RESERVED map to exclusive locks in our simplified
    // implementation. If this process has a shared lock, no other process can
    // have an exclusive lock.
    *has_reserved_lock = 0;
    return SQLITE_OK;
  }

#if BUILDFLAG(IS_FUCHSIA)
  return SQLITE_IOERR_CHECKRESERVEDLOCK;
#else
  // On POSIX, it's possible to query the existing lock state of a file. The
  // SQLite unix VFS takes advantage of this. On Windows, this isn't the case.
  // Follow the strategy of the Windows VFS, which checks by trying to get an
  // exclusive lock on the file.
  base::File::Error error = file_.Lock(base::File::LockMode::kShared);
  if (error != base::File::FILE_OK) {
    *has_reserved_lock = 1;
    return SQLITE_OK;
  }

  *has_reserved_lock = 0;
  if (file_.Unlock() == base::File::FILE_OK) {
    return SQLITE_OK;
  }

  // We acquired a shared lock that we can't get rid of.
  sqlite_lock_mode_ = SQLITE_LOCK_SHARED;
  return SQLITE_IOERR_CHECKRESERVEDLOCK;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

int SandboxedVfsFileImpl::FileControl(int opcode, void* data) {
  switch (opcode) {
    case SQLITE_FCNTL_MMAP_SIZE:
      // Implementing memory-mapping will require handling this correctly.
      return SQLITE_NOTFOUND;
    default:
      return SQLITE_NOTFOUND;
  }
}

int SandboxedVfsFileImpl::SectorSize() {
  return 0;
}

int SandboxedVfsFileImpl::DeviceCharacteristics() {
  // TODO(pwnall): Figure out if we can get away with returning 0 on Windows.
#if BUILDFLAG(IS_WIN)
  return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
#else
  // NOTE: SQLite's unix VFS attempts to detect the underlying filesystem and
  // sets some flags based on the result.
  return 0;
#endif  // BUILDFLAG(IS_WIN)
}

int SandboxedVfsFileImpl::ShmMap(int page_index,
                                 int page_size,
                                 int extend_file_if_needed,
                                 void volatile** result) {
  DCHECK_GE(page_index, 0);
  DCHECK_GE(page_size, 0);
  DCHECK(result);

  // https://www.sqlite.org/wal.html#use_of_wal_without_shared_memory states
  // that SQLite only attempts to use shared memory "-shm" files for databases
  // in WAL mode that may be accessed by multiple processes (are not EXCLUSIVE).
  //
  // Chrome will not only use WAL mode on EXCLUSIVE databases.
  NOTREACHED() << "SQLite should not attempt to use shared memory";
}

int SandboxedVfsFileImpl::ShmLock(int offset, int size, int flags) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(size, 0);

  // https://www.sqlite.org/wal.html#use_of_wal_without_shared_memory states
  // that SQLite only attempts to use shared memory "-shm" files for databases
  // in WAL mode that may be accessed by multiple processes (are not EXCLUSIVE).
  //
  // Chrome will not only use WAL mode on EXCLUSIVE databases.
  NOTREACHED() << "SQLite should not attempt to use shared memory";
}

void SandboxedVfsFileImpl::ShmBarrier() {
  // https://www.sqlite.org/wal.html#use_of_wal_without_shared_memory states
  // that SQLite only attempts to use shared memory "-shm" files for databases
  // in WAL mode that may be accessed by multiple processes (are not EXCLUSIVE).
  //
  // Chrome will not only use WAL mode on EXCLUSIVE databases.
  NOTREACHED() << "SQLite should not attempt to use shared memory";
}

int SandboxedVfsFileImpl::ShmUnmap(int also_delete_file) {
  // https://www.sqlite.org/wal.html#use_of_wal_without_shared_memory states
  // that SQLite only attempts to use shared memory "-shm" files for databases
  // in WAL mode that may be accessed by multiple processes (are not EXCLUSIVE).
  //
  // Chrome will not only use WAL mode on EXCLUSIVE databases.
  NOTREACHED() << "SQLite should not attempt to use shared memory";
}

int SandboxedVfsFileImpl::Fetch(sqlite3_int64 offset, int size, void** result) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(size, 0);
  DCHECK(result);

  // NOTE: This would be needed for mmap()ed file support.
  *result = nullptr;
  return SQLITE_IOERR;
}

int SandboxedVfsFileImpl::Unfetch(sqlite3_int64 offset, void* fetch_result) {
  DCHECK_GE(offset, 0);
  DCHECK(fetch_result);

  // NOTE: This would be needed for mmap()ed file support.
  return SQLITE_IOERR;
}

}  // namespace storage
