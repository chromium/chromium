// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/sqlite/sqlite3.h"

namespace persistent_cache {

SandboxedFile::SandboxedFile(base::File file, AccessRights access_rights)
    : underlying_file_(std::move(file)),
      access_rights_(access_rights),
      sqlite_lock_mode_(SQLITE_LOCK_NONE) {}
SandboxedFile::~SandboxedFile() = default;

base::File SandboxedFile::TakeUnderlyingFile() {
  return std::move(underlying_file_);
}

void SandboxedFile::OnFileOpened(base::File file) {
  CHECK(file.IsValid());
  opened_file_ = std::move(file);
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

int SandboxedFile::Lock(int mode) {
  // TODO(https://crbug.com/377475540): Implement a cross-process lock.
  if (mode > sqlite_lock_mode_) {
    sqlite_lock_mode_ = mode;
  }
  return SQLITE_OK;
}

int SandboxedFile::Unlock(int mode) {
  // TODO(https://crbug.com/377475540): Implement a cross-process lock.
  if (mode < sqlite_lock_mode_) {
    sqlite_lock_mode_ = mode;
  }
  return SQLITE_OK;
}

int SandboxedFile::CheckReservedLock(int* has_reserved_lock) {
  // TODO(https://crbug.com/377475540): Implement a cross-process lock.
  *has_reserved_lock = sqlite_lock_mode_ >= SQLITE_LOCK_RESERVED;
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

}  // namespace persistent_cache
