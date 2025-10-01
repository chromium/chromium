// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

#include <utility>

#include "base/files/platform_file.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/sqlite/sqlite3.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace persistent_cache {

namespace {

constexpr uint32_t kMaxSharedLocks = 0x08000000;
constexpr uint32_t kSharedMask = 0x0FFFFFFF;
constexpr uint32_t kReservedBit = 0x20000000;
constexpr uint32_t kPendingBit = 0x40000000;

}  // namespace

SandboxedFile::SandboxedFile(
    base::File file,
    base::FilePath file_path,
    AccessRights access_rights,
    base::WritableSharedMemoryMapping mapped_shared_lock)
    : file_path_(std::move(file_path)),
      underlying_file_(std::move(file)),
      access_rights_(access_rights),
      mapped_shared_lock_(std::move(mapped_shared_lock)) {}
SandboxedFile::~SandboxedFile() = default;

base::File SandboxedFile::TakeUnderlyingFile() {
  return std::move(underlying_file_);
}

void SandboxedFile::OnFileOpened(base::File file) {
  CHECK(file.IsValid());
  opened_file_ = std::move(file);
}

base::File SandboxedFile::DuplicateFile(AccessRights access_rights) {
  // Can't upgrade from read-only to read-write.
  CHECK((access_rights == AccessRights::kReadOnly) ||
        (access_rights_ == AccessRights::kReadWrite));
  CHECK(underlying_file_.IsValid() || opened_file_.IsValid());

  base::File& source =
      underlying_file_.IsValid() ? underlying_file_ : opened_file_;
  if (access_rights == access_rights_) {
    // Caller requests the same rights. Simple duplication as-is.
    return source.Duplicate();
  }

#if BUILDFLAG(IS_WIN)
  // Duplicate the handle to the file with restricted rights.
  HANDLE handle = nullptr;
  if (!::DuplicateHandle(
          /*hSourceProcessHandle=*/::GetCurrentProcess(),
          /*hSourceHandle=*/source.GetPlatformFile(),
          /*hTargetProcessHandle=*/::GetCurrentProcess(),
          /*lpTargetHandle=*/&handle,
          /*dwDesiredAccess=*/FILE_GENERIC_READ,
          /*bInheritHandle=*/FALSE,
          /*dwOptions=*/0)) {
    // Duplication failed; return an invalid File.
    DWORD error = ::GetLastError();
    return base::File(base::File::OSErrorToFileError(error));
  }
  return base::File(handle);
#else
  // It's not possible to get a new file descriptor with reduced permissions to
  // the same file description, so open the file anew with read-only access.

  // It is a programming error to attempt to emit a read-only view to the file
  // when the path to the file was not provided at construction.
  CHECK(!file_path_.empty());
  return base::File(file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
#endif
}

int SandboxedFile::Close() {
  CHECK(IsValid());
  underlying_file_ = std::move(opened_file_);
  return SQLITE_OK;
}

int SandboxedFile::Read(void* buffer, int size, sqlite3_int64 offset) {
  // Make a safe span from the pair <buffer, size>. The buffer and the
  // size are received from sqlite.
  CHECK(buffer);
  CHECK_GE(size, 0);
  CHECK_GE(offset, 0);
  const size_t checked_size = base::checked_cast<size_t>(size);
  // SAFETY: `buffer` always points to at least `size` valid bytes.
  auto data =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(buffer), checked_size));

  // Read data from the file.
  CHECK(IsValid());
  std::optional<size_t> bytes_read = opened_file_.Read(offset, data);
  if (!bytes_read.has_value()) {
    return SQLITE_IOERR_READ;
  }

  // The buffer was fully read.
  if (bytes_read.value() == checked_size) {
    return SQLITE_OK;
  }

  // Some bytes were read but the buffer was not filled. SQLite requires that
  // the unread bytes must be filled with zeros.
  auto remaining_bytes = data.subspan(bytes_read.value());
  std::fill(remaining_bytes.begin(), remaining_bytes.end(), 0);
  return SQLITE_IOERR_SHORT_READ;
}

int SandboxedFile::Write(const void* buffer, int size, sqlite3_int64 offset) {
  // Make a safe span from the pair <buffer, size>. The buffer and the
  // size are received from sqlite.
  CHECK(buffer);
  CHECK_GE(size, 0);
  CHECK_GE(offset, 0);
  const size_t checked_size = base::checked_cast<size_t>(size);
  // SAFETY: `buffer` always points to at least `size` valid bytes.
  auto data = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(buffer), checked_size));

  CHECK(IsValid());
  std::optional<size_t> bytes_written = opened_file_.Write(offset, data);
  if (!bytes_written.has_value()) {
    return SQLITE_IOERR_WRITE;
  }
  CHECK_LE(bytes_written.value(), checked_size);

  // The bytes were successfully written to disk.
  if (bytes_written.value() == checked_size) {
    return SQLITE_OK;
  }

  // Detect the case where there is no space on the disk.
  base::File::Error last_error = base::File::GetLastFileError();
  if (last_error == base::File::Error::FILE_ERROR_NO_SPACE) {
    return SQLITE_FULL;
  }

  // A generic write error.
  return SQLITE_IOERR_WRITE;
}

int SandboxedFile::Truncate(sqlite3_int64 size) {
  CHECK(IsValid());
  if (!opened_file_.SetLength(size)) {
    return SQLITE_IOERR_TRUNCATE;
  }
  return SQLITE_OK;
}

int SandboxedFile::Sync(int flags) {
  CHECK(IsValid());
  if (!opened_file_.Flush()) {
    return SQLITE_IOERR_FSYNC;
  }
  return SQLITE_OK;
}

int SandboxedFile::FileSize(sqlite3_int64* result_size) {
  CHECK(IsValid());
  int64_t length = opened_file_.GetLength();
  if (length < 0) {
    return SQLITE_IOERR_FSTAT;
  }

  *result_size = length;
  return SQLITE_OK;
}

// This function implements the database locking mechanism as defined by the
// SQLite VFS (Virtual File System) interface. It is responsible for escalating
// locks on the database file to ensure that multiple processes can access the
// database in a controlled and serialized manner, preventing data corruption.
//
// In this shared memory implementation, the lock states are managed directly
// in a shared memory region accessible by all client processes, rather than
// relying on traditional file-system locks (like fcntl on Unix or LockFileEx
// on Windows).
//
// The lock implementation mirrors the state transitions of the standard SQLite
// locking mechanism:
//
//     SHARED: Allows multiple readers.
//     RESERVED: A process signals its intent to write.
//     PENDING: A writer is waiting for readers to finish.
//     EXCLUSIVE: A single process has exclusive write access.
//
// The valid transitions are:
//
//    UNLOCKED -> SHARED
//    SHARED -> RESERVED
//    SHARED -> (PENDING) -> EXCLUSIVE
//    RESERVED -> (PENDING) -> EXCLUSIVE
//    PENDING -> EXCLUSIVE
//
// See original implementation:
//    https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/src/src/os_win.c;l=3514;drc=4a0b7a332f3aeb27814cfa12dc0ebdbbd994a928
//
// Some issues related to file system locks:
//    https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/src/src/os_unix.c;l=1077;drc=5d60f47001bf64b48abac68ed59621e528144ea4
//
// The SQLite core uses two distinct strategies to acquire an EXCLUSIVE lock.
// This VFS implementation must correctly handle lock requests from both paths.
//
// 1. Normal transaction path
//   The standard database operations (INSERT, UPDATE, BEGIN COMMIT, etc.) on a
//   healthy database will escalate the lock sequentially:
//       SHARED -> RESERVED -> PENDING -> EXCLUSIVE.
//   The intermediate RESERVED lock is mandatory. It signals an intent to write
//   while still permitting other connections to hold SHARED locks for reading.
//
// 2. Hot-journal recovery path
//   A special case that occurs upon initial connection when a hot-journal is
//   detected, indicating a previous crash or power loss. A direct request for
//   an EXCLUSIVE lock is required. In this state, the database is known to be
//   inconsistent. The RESERVED lock is intentionally skipped because its
//   purpose is to allow concurrent readers, which would be disastrous. A direct
//   EXCLUSIVE lock acts as an emergency lockdown, preventing ALL other
//   connections from reading corrupt data until the recovery process is
//   complete.
//
//   see:
//     https://source.chromium.org/chromium/chromium/src/+/main:third_party/sqlite/src/src/pager.c;l=5260;drc=65d0312c96cd23958372fac8940314c782a6b03c
int SandboxedFile::Lock(int mode) {
  // Ensures valid lock states are used (see: sqlite3OsLock(...) assertions).
  CHECK(mode == SQLITE_LOCK_SHARED || mode == SQLITE_LOCK_RESERVED ||
        mode == SQLITE_LOCK_EXCLUSIVE);

  // Do nothing if there is already a lock of this type or more restrictive.
  if (sqlite_lock_mode_ >= mode) {
    return SQLITE_OK;
  }

  auto& lock_state = GetLockState();
  switch (mode) {
    case SQLITE_LOCK_SHARED: {
      // Try to increment the SHARED lock count as long as the PENDING lock
      // remains unheld and there is room remaining to count a new SHARED lock.
      uint32_t shared_state = lock_state.load();
      for (int i = 0; i < 5; ++i) {
        if ((shared_state & kPendingBit) != 0 ||
            (shared_state & kSharedMask) == kMaxSharedLocks) {
          break;
        }
        if (lock_state.compare_exchange_strong(shared_state,
                                               shared_state + 1)) {
          // The SHARED lock was successfully acquired.
          sqlite_lock_mode_ = SQLITE_LOCK_SHARED;
          return SQLITE_OK;
        }
        // Perform up to four retries in case this client is racing against
        // other changes to the shared lock.
      }
      return SQLITE_BUSY;
    }

    case SQLITE_LOCK_RESERVED: {
      // To acquire a RESERVED lock, the current connection must already have
      // a shared access to it.
      CHECK_EQ(sqlite_lock_mode_, SQLITE_LOCK_SHARED);

      // Acquire a RESERVED lock to prevent a different writer to declare its
      // intention to modify the database. At this point, readers are still
      // allowed to get a SHARED lock on the database.
      const uint32_t acquired_shared_state = lock_state.fetch_or(kReservedBit);
      if ((acquired_shared_state & kReservedBit) != 0) {
        return SQLITE_BUSY;
      }

      // The RESERVED lock was successfully acquired.
      sqlite_lock_mode_ = SQLITE_LOCK_RESERVED;
      return SQLITE_OK;
    }

    case SQLITE_LOCK_EXCLUSIVE: {
      // Acquiring an EXCLUSIVE lock may happen through multiple calls to
      // SandboxedFile::Lock(...) and the PENDING lock may be kept between these
      // calls.

      // To acquire an EXCLUSIVE lock, the current connection must already have
      // at least SHARED lock. Owning RESERVED lock not mandatory.
      CHECK_GE(sqlite_lock_mode_, SQLITE_LOCK_SHARED);

      // Acquire the PENDING lock, if not already acquired. Hold it until the
      // EXCLUSIVE lock is obtained. No new SHARED locks will be granted in
      // the meantime, but current SHARED locks remain valid.
      uint32_t shared_state = 0;
      if (sqlite_lock_mode_ < SQLITE_LOCK_PENDING) {
        shared_state = lock_state.fetch_or(kPendingBit);
        if ((shared_state & kPendingBit) != 0) {
          // This connection is not the owner of the PENDING lock.
          return SQLITE_BUSY;
        }
        // The PENDING lock was acquired. Keep it for subsequent calls until all
        // SHARED locks are released.
        sqlite_lock_mode_ = SQLITE_LOCK_PENDING;
        // Update the copy of the current state of the lock for use below.
        shared_state |= kPendingBit;
      } else {
        // Fetch the current state of the lock for use below.
        shared_state = lock_state.load();
      }

      // Do not grant the EXCLUSIVE lock until all other readers have released
      // their SHARED locks. This connection still owns and keeps a SHARED lock.
      if ((shared_state & kSharedMask) != 1) {
        return SQLITE_BUSY;
      }

      // There is no active SHARED lock except for this connection. The PENDING
      // lock is owned by this connection so it is valid to grant the EXCLUSIVE
      // lock.
      sqlite_lock_mode_ = SQLITE_LOCK_EXCLUSIVE;
      return SQLITE_OK;
    }
  }

  return SQLITE_IOERR_LOCK;
}

// This function is the counterpart to Lock and is responsible for reducing the
// lock level on the database file. This typically happens after a transaction
// is committed or rolled back, or when a process holding a write lock is
// ready to allow other readers in.
//
// The valid transitions are:
//
//    SHARED -> UNLOCKED
//    EXCLUSIVE -> UNLOCKED
//    EXCLUSIVE -> SHARED
//
// It is also valid to release any pending state (PENDING or RESERVED) even if
// the state never went to EXCLUSIVE. This can happen when a connection gives up
// on trying to get an EXCLUSIVE lock.
int SandboxedFile::Unlock(int mode) {
  // Ensures valid lock states are used (see: sqlite3OsUnlock(...) assertions).
  CHECK(mode == SQLITE_LOCK_NONE || mode == SQLITE_LOCK_SHARED);

  // Do nothing if there is already a lock of this type or less restrictive.
  if (sqlite_lock_mode_ <= mode) {
    return SQLITE_OK;
  }

  auto& lock_state = GetLockState();

  // Release the RESERVED or RESERVED and PENDING bits, if held.
  if (uint32_t clear_mask =
          (sqlite_lock_mode_ >= SQLITE_LOCK_PENDING
               ? (kPendingBit | kReservedBit)
               : (sqlite_lock_mode_ == SQLITE_LOCK_RESERVED ? kReservedBit
                                                            : 0U))) {
    lock_state.fetch_and(~clear_mask);
  }

  // Release the SHARED lock if no longer needed.
  if (mode == SQLITE_LOCK_NONE) {
    const uint32_t readers_shared_state = lock_state.fetch_sub(1);
    CHECK_GE(readers_shared_state & kSharedMask, 1u);
  }

  // Lock was successfully released.
  sqlite_lock_mode_ = mode;
  return SQLITE_OK;
}

int SandboxedFile::CheckReservedLock(int* has_reserved_lock) {
  uint32_t shared_state = GetLockState().load();
  *has_reserved_lock = (shared_state & kReservedBit) != 0;
  return SQLITE_OK;
}

int SandboxedFile::FileControl(int opcode, void* data) {
  return SQLITE_NOTFOUND;
}

int SandboxedFile::SectorSize() {
  return 0;
}

int SandboxedFile::DeviceCharacteristics() {
  return 0;
}

int SandboxedFile::ShmMap(int page_index,
                          int page_size,
                          int extend_file_if_needed,
                          void volatile** result) {
  // TODO(https://crbug.com/377475540): Implement WAL mode.
  return SQLITE_IOERR_SHMMAP;
}

int SandboxedFile::ShmLock(int offset, int size, int flags) {
  // TODO(https://crbug.com/377475540): Implement WAL mode.
  return SQLITE_IOERR_SHMLOCK;
}

void SandboxedFile::ShmBarrier() {
  // TODO(https://crbug.com/377475540): Implement WAL mode.
}

int SandboxedFile::ShmUnmap(int also_delete_file) {
  // TODO(https://crbug.com/377475540): Implement WAL mode.
  return SQLITE_IOERR_SHMMAP;
}

int SandboxedFile::Fetch(sqlite3_int64 offset, int size, void** result) {
  // TODO(https://crbug.com/377475540): Implement shared memory.
  *result = nullptr;
  return SQLITE_IOERR;
}

int SandboxedFile::Unfetch(sqlite3_int64 offset, void* fetch_result) {
  // TODO(https://crbug.com/377475540): Implement shared memory.
  return SQLITE_IOERR;
}

LockState& SandboxedFile::GetLockState() {
  CHECK(mapped_shared_lock_.IsValid());
  return *mapped_shared_lock_.GetMemoryAs<LockState>();
}

}  // namespace persistent_cache
