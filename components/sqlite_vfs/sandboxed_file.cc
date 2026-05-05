// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/sandboxed_file.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/platform_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "components/sqlite_vfs/file_system_id.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_vfs {

SandboxedFile::SandboxedFile(Client client,
                             FileType file_type,
                             base::File file,
                             AccessRights access_rights,
                             std::optional<SharedLocks> shared_locks,
                             base::UnguessableToken shared_locks_id,
                             base::File wal_index_file)
    : client_(client),
      file_type_(file_type),
      underlying_file_(std::move(file)),
      access_rights_(access_rights),
      shared_locks_(std::move(shared_locks)),
      shared_locks_id_(std::move(shared_locks_id)),
      wal_index_file_(std::move(wal_index_file)) {
  CHECK(!shared_locks_ || file_type_ == FileType::kMainDb);
  CHECK(!wal_index_file_.IsValid() || file_type_ == FileType::kMainDb);
  CHECK(!wal_index_file_.IsValid() || shared_locks_.has_value());

  // Cache the ID of the main database file for double-open protection; see
  // RegisterSandboxedFiles.
  if (file_type_ == FileType::kMainDb && underlying_file_.IsValid()) {
    file_system_id_ = GetFileSystemId(client_, underlying_file_);
  }
}

SandboxedFile::~SandboxedFile() = default;

base::File SandboxedFile::TakeUnderlyingFile(FileType file_type) {
  CHECK_EQ(file_type, file_type_);
  // Lock the file via filesystem APIs if this is the main database file and its
  // creator wishes this to be the only connection allowed.
  if (file_type == FileType::kMainDb && is_single_connection() &&
      !AcquireSingleConnectionlock()) {
    return {};
  }
  return std::move(underlying_file_);
}

void SandboxedFile::OnFileOpened(base::File file) {
  CHECK(file.IsValid());
  opened_file_ = std::move(file);
}

const base::File& SandboxedFile::GetFile() const {
  return underlying_file_.IsValid() ? underlying_file_ : opened_file_;
}

base::File& SandboxedFile::GetFile() {
  return const_cast<base::File&>(
      const_cast<const SandboxedFile*>(this)->GetFile());
}

int SandboxedFile::Close() {
  CHECK(IsValid());
  underlying_file_ = std::move(opened_file_);

  // Unlock the file via filesystem APIs if this is the main database file and
  // its creator wishes this to be the only connection allowed.
  if (file_type_ == FileType::kMainDb && is_single_connection()) {
    ReleaseSingleConnectionlock();
  }
  return SQLITE_OK;
}

LockState SandboxedFile::Abandon() {
  CHECK_EQ(file_type_, FileType::kMainDb);
  CHECK(!is_single_connection());
  LockState state = shared_locks_->Abandon();
  base::UmaHistogramEnumeration(GetHistogramName(client_, "LockStateOnAbandon"),
                                state);
  return state;
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
  CHECK(buffer);
  CHECK_GE(offset, 0);
  CHECK(IsValid());

  if (opened_file_.WriteAndCheck(
          offset,
          // SAFETY: `buffer` always points to at least `size` valid bytes.
          UNSAFE_BUFFERS(base::span(static_cast<const uint8_t*>(buffer),
                                    base::checked_cast<size_t>(size))))) {
    return SQLITE_OK;
  }

  // Distinguish disk full from general I/O errors.
  return base::File::GetLastFileError() ==
                 base::File::Error::FILE_ERROR_NO_SPACE
             ? SQLITE_FULL
             : SQLITE_IOERR_WRITE;
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
  CHECK_EQ(file_type_, FileType::kMainDb);
  // Ensures valid lock states are used (see: sqlite3OsLock(...) assertions).
  CHECK(mode == SQLITE_LOCK_SHARED || mode == SQLITE_LOCK_RESERVED ||
        mode == SQLITE_LOCK_EXCLUSIVE);

  // Do nothing if there is already a lock of this type or more restrictive.
  if (sqlite_lock_mode_ >= mode) {
    return SQLITE_OK;
  }

  if (is_single_connection()) {
    sqlite_lock_mode_ = mode;
    return SQLITE_OK;
  }

  return shared_locks_->Lock(mode, sqlite_lock_mode_);
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
  CHECK_EQ(file_type_, FileType::kMainDb);

  // Ensures valid lock states are used (see: sqlite3OsUnlock(...) assertions).
  CHECK(mode == SQLITE_LOCK_NONE || mode == SQLITE_LOCK_SHARED);

  // Do nothing if there is already a lock of this type or less restrictive.
  if (sqlite_lock_mode_ <= mode) {
    return SQLITE_OK;
  }

  if (is_single_connection()) {
    sqlite_lock_mode_ = mode;
    return SQLITE_OK;
  }

  return shared_locks_->Unlock(mode, sqlite_lock_mode_);
}

int SandboxedFile::CheckReservedLock(int* has_reserved_lock) {
  CHECK_EQ(file_type_, FileType::kMainDb);
  if (is_single_connection()) {
    *has_reserved_lock = sqlite_lock_mode_ >= SQLITE_LOCK_RESERVED;
  } else {
    *has_reserved_lock = shared_locks_->IsReserved() ? 1 : 0;
  }
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
  CHECK_GE(page_index, 0);
  // Single-connection databases (which are in exclusive mode) do not use a
  // mapped WAL-index; see https://sqlite.org/wal.html#noshm
  CHECK(!is_single_connection());

  if (!wal_index_file_.IsValid()) {
    return SQLITE_IOERR_SHMMAP;
  }

  // Grow the collection of mappings if `page_index` is out of bounds.
  size_t index = base::checked_cast<size_t>(page_index);
  if (shm_mappings_.size() <= index) {
    shm_mappings_.resize(index + 1);
  }

  // Map the page at `page_index` if this is the first request for it.
  if (!shm_mappings_[index]) {
    // Make sure that the file is large enough; expanding it as needed.
    const int64_t file_size = wal_index_file_.GetLength();
    if (file_size < 0) {
      return SQLITE_IOERR_SHMMAP;  // Failed to get size of file.
    }

    int64_t required_size =
        base::CheckMul<int64_t>(index + 1, page_size).ValueOrDie();
    if (file_size < required_size) {
      if (access_rights_ == AccessRights::kReadOnly) {
        // This read-only connection does not have the rights required to grow
        // the file. In general, a read-only connection will not try to do this.
        // The exception is when a read-only connection tries to map the
        // WAL-index before any read-write connection has had a chance to build
        // it. Return SQLITE_READONLY_CANTINIT so that SQLite knows to consider
        // the WAL-index to be "unreliable". This connection will use its own
        // in-memory WAL-index until it discovers that a writer has made the
        // shared index "reliable".
        return SQLITE_READONLY_CANTINIT;
      }
      if (!extend_file_if_needed) {
        // The caller doesn't wish to resize the file, so return a null ptr and
        // report success.
        *result = nullptr;
        return SQLITE_OK;
      }
      if (!wal_index_file_.SetLength(required_size)) {
        return SQLITE_IOERR_SHMMAP;  // Failed to grow the file.
      }
    }

    // Map the page.
    base::MemoryMappedFile::Access access =
        access_rights_ == AccessRights::kReadWrite
            ? base::MemoryMappedFile::READ_WRITE
            : base::MemoryMappedFile::READ_ONLY;

    auto mapping = std::make_unique<base::MemoryMappedFile>();
    base::MemoryMappedFile::Region region{
        base::CheckMul<int64_t>(index, page_size).ValueOrDie(),
        static_cast<size_t>(page_size)};
    if (!mapping->Initialize(wal_index_file_.Duplicate(), region, access)) {
      return SQLITE_IOERR_SHMMAP;  // Failed to map the page.
    }
    shm_mappings_[index] = std::move(mapping);
  }

  *result = shm_mappings_[index]->mutable_bytes().data();
  if (access_rights_ == AccessRights::kReadWrite) {
    return SQLITE_OK;
  }
  // Returning SQLITE_READONLY tells SQLite not to attempt writes to the
  // mapping, which would cause a segmentation fault since the memory is mapped
  // read-only.
  return SQLITE_READONLY;
}

int SandboxedFile::ShmLock(int offset, int size, int flags) {
  // Single-connection databases (which are in exclusive mode) do not use the
  // WAL locks to synchronize access to the WAL-index across connections; see
  // https://sqlite.org/wal.html#noshm
  CHECK(!is_single_connection());
  CHECK(flags == (SQLITE_SHM_LOCK | SQLITE_SHM_SHARED) ||
        flags == (SQLITE_SHM_LOCK | SQLITE_SHM_EXCLUSIVE) ||
        flags == (SQLITE_SHM_UNLOCK | SQLITE_SHM_SHARED) ||
        flags == (SQLITE_SHM_UNLOCK | SQLITE_SHM_EXCLUSIVE));
  return shared_locks_->ShmLock(
      offset, size,
      ((flags & SQLITE_SHM_LOCK) != 0 ? SharedLocks::LockOperation::kAcquire
                                      : SharedLocks::LockOperation::kRelease),
      ((flags & SQLITE_SHM_EXCLUSIVE) != 0 ? SharedLocks::LockType::kExclusive
                                           : SharedLocks::LockType::kShared));
}

void SandboxedFile::ShmBarrier() {
  // Single-connection databases (which are in exclusive mode) do not use a
  // memory barrier to synchronize access to the WAL-index across connections;
  // see https://sqlite.org/wal.html#noshm
  CHECK(!is_single_connection());
  shared_locks_->ShmBarrier();
}

int SandboxedFile::ShmUnmap(int also_delete_file) {
  // Single-connection databases (which are in exclusive mode) do not use a
  // mapped WAL-index; see https://sqlite.org/wal.html#noshm
  CHECK(!is_single_connection());

  shm_mappings_.clear();

  // The underlying file is held open via wal_index_file_. Never try to delete
  // it.
  return SQLITE_OK;
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

bool SandboxedFile::AcquireSingleConnectionlock() {
  CHECK(underlying_file_.IsValid());
  const auto error = underlying_file_.Lock(base::File::LockMode::kExclusive);
  base::UmaHistogramExactLinear(GetHistogramName(client_, "LockResult"), -error,
                                -base::File::FILE_ERROR_MAX);
  return error == base::File::FILE_OK;
}

void SandboxedFile::ReleaseSingleConnectionlock() {
  CHECK(underlying_file_.IsValid());
  underlying_file_.Unlock();
}

}  // namespace sqlite_vfs
