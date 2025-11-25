// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/backend_storage_delegate.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/constants.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace persistent_cache::sqlite {

namespace {

// Returns a duplicate of `source_file` (at `source_file_path`) which has either
// read-only (`source_is_read_write` = false) or read-write (otherwise) access
// that, itself, has either read-only (`target_read_write` = false) or
// read-write access.
base::File DuplicateFile(const base::File& source_file,
                         const base::FilePath& source_file_path,
                         bool source_is_read_write,
                         bool target_read_write) {
  CHECK(source_file.IsValid());
  // Can't upgrade from read-only to read-write.
  CHECK(!target_read_write || source_is_read_write);

  if (source_is_read_write == target_read_write) {
    // Caller requests the same rights. Simple duplication as-is.
    return source_file.Duplicate();
  }

#if BUILDFLAG(IS_WIN)
  // Duplicate the handle to the file with restricted rights.
  HANDLE handle = nullptr;
  if (!::DuplicateHandle(
          /*hSourceProcessHandle=*/::GetCurrentProcess(),
          /*hSourceHandle=*/source_file.GetPlatformFile(),
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
  return base::File(source_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
#endif
}

}  // namespace

std::optional<PendingBackend> BackendStorageDelegate::MakePendingBackend(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  // Write-ahead logging journaling is only supported for single connections.
  CHECK(!journal_mode_wal || single_connection);
  PendingBackend pending_backend;

  uint32_t create_flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                          base::File::FLAG_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE |
                          base::File::FLAG_CAN_DELETE_ON_CLOSE;

  if (single_connection) {
    // If only a single connection is allowed, there is no need to allow others
    // to open the files for reading or writing. Delete is always allowed so
    // that the files can be deleted even while in-use.
    create_flags |= base::File::FLAG_WIN_EXCLUSIVE_READ |
                    base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  }

  // Make sure handles to these files are safe to pass to untrusted processes.
  create_flags = base::File::AddFlagsForPassingToUntrustedProcess(create_flags);

  auto db_file_path =
      directory.Append(base_name).AddExtension(kDbFileExtension);
  pending_backend.sqlite_data.db_file = base::File(db_file_path, create_flags);
  if (!pending_backend.sqlite_data.db_file.IsValid()) {
    base::UmaHistogramExactLinear(
        "PersistentCache.Sqlite.DbFile.CreateError",
        -pending_backend.sqlite_data.db_file.error_details(),
        -base::File::FILE_ERROR_MAX);
    return std::nullopt;
  }

  auto journal_file_path =
      directory.Append(base_name).AddExtension(kJournalFileExtension);
  pending_backend.sqlite_data.journal_file =
      base::File(journal_file_path, create_flags);
  if (!pending_backend.sqlite_data.journal_file.IsValid()) {
    base::UmaHistogramExactLinear(
        "PersistentCache.Sqlite.JournalFile.CreateError",
        -pending_backend.sqlite_data.journal_file.error_details(),
        -base::File::FILE_ERROR_MAX);
    return std::nullopt;
  }

  if (journal_mode_wal) {
    auto wal_file_path =
        directory.Append(base_name).AddExtension(kWalJournalFileExtension);
    pending_backend.sqlite_data.wal_file =
        base::File(wal_file_path, create_flags);
    if (!pending_backend.sqlite_data.wal_file.IsValid()) {
      base::UmaHistogramExactLinear(
          "PersistentCache.Sqlite.WalJournalFile.CreateError",
          -pending_backend.sqlite_data.wal_file.error_details(),
          -base::File::FILE_ERROR_MAX);
      return std::nullopt;
    }
  }

  if (!single_connection) {
    // The shared lock is only needed if multiple connections are permitted.
    pending_backend.sqlite_data.shared_lock =
        base::UnsafeSharedMemoryRegion::Create(sizeof(SharedAtomicLock));
    if (!pending_backend.sqlite_data.shared_lock.IsValid()) {
      return std::nullopt;
    }
  }

  pending_backend.read_write = true;

  return pending_backend;
}

std::unique_ptr<Backend> BackendStorageDelegate::MakeBackend(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  if (auto pending_backend = MakePendingBackend(
          directory, base_name, single_connection, journal_mode_wal);
      pending_backend.has_value()) {
    return SqliteBackendImpl::Bind(*std::move(pending_backend));
  }
  return nullptr;
}

std::optional<PendingBackend> BackendStorageDelegate::ShareReadOnlyConnection(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    const Backend& backend) {
  return ShareConnection(directory, base_name, backend, /*read_write=*/false);
}

std::optional<PendingBackend> BackendStorageDelegate::ShareReadWriteConnection(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    const Backend& backend) {
  return ShareConnection(directory, base_name, backend, /*read_write=*/true);
}

base::FilePath BackendStorageDelegate::GetBaseName(const base::FilePath& file) {
  return file.MatchesFinalExtension(kDbFileExtension)
             ? file.BaseName().RemoveFinalExtension()
             : base::FilePath();
}

int64_t BackendStorageDelegate::DeleteFiles(const base::FilePath& directory,
                                            const base::FilePath& base_name) {
  auto file_path = directory.Append(base_name).AddExtension(kDbFileExtension);
  int64_t bytes_recovered = base::GetFileSize(file_path).value_or(0);
  bool delete_success = base::DeleteFile(file_path);
  base::UmaHistogramBoolean(
      "PersistentCache.ParamsManager.DbFile.DeleteSuccess", delete_success);
  if (!delete_success) {
    return 0;
  }

  file_path = directory.Append(base_name).AddExtension(kJournalFileExtension);
  auto file_size = base::GetFileSize(file_path).value_or(0);
  delete_success = base::DeleteFile(file_path);
  base::UmaHistogramBoolean(
      "PersistentCache.ParamsManager.JournalFile.DeleteSuccess",
      delete_success);
  if (delete_success) {
    bytes_recovered = base::ClampAdd(bytes_recovered, file_size);
  }

  file_path =
      directory.Append(base_name).AddExtension(kWalJournalFileExtension);
  file_size = base::GetFileSize(file_path).value_or(0);
  delete_success = base::DeleteFile(file_path);
  base::UmaHistogramBoolean(
      "PersistentCache.ParamsManager.WalJournalFile.DeleteSuccess",
      delete_success);
  if (delete_success) {
    bytes_recovered = base::ClampAdd(bytes_recovered, file_size);
  }

  // TODO (https://crbug.com/377475540): Cleanup when deletion of journal
  // failed.
  return bytes_recovered;
}

std::optional<PendingBackend> BackendStorageDelegate::ShareConnection(
    const base::FilePath& directory,
    const base::FilePath& base_name,
    const Backend& backend,
    bool read_write) {
  const SqliteBackendImpl& sqlite_backend =
      static_cast<const SqliteBackendImpl&>(backend);
  const SqliteVfsFileSet& file_set = sqlite_backend.file_set();

  // Cannot share a single-connection backend. If it ever becomes interesting to
  // connect to a backend in one process and then move it to another process,
  // we shall introduce a way to `Unbind()` a backend to convert it back into a
  // `PendingBackend`.
  CHECK(!file_set.is_single_connection());
  // All connections using a write-ahead log are single-connection.
  CHECK(!file_set.wal_journal_mode());

  PendingBackend pending_backend;

  pending_backend.sqlite_data.db_file =
      DuplicateFile(file_set.GetDbFile(),
                    directory.Append(base_name).AddExtension(kDbFileExtension),
                    !file_set.read_only(), read_write);
  if (!pending_backend.sqlite_data.db_file.IsValid()) {
    return std::nullopt;
  }

  pending_backend.sqlite_data.journal_file = DuplicateFile(
      file_set.GetJournalFile(),
      directory.Append(base_name).AddExtension(kJournalFileExtension),
      !file_set.read_only(), read_write);
  if (!pending_backend.sqlite_data.journal_file.IsValid()) {
    return std::nullopt;
  }

  pending_backend.sqlite_data.shared_lock =
      file_set.GetSharedLock().Duplicate();
  if (!pending_backend.sqlite_data.shared_lock.IsValid()) {
    return std::nullopt;
  }

  pending_backend.read_write = read_write;

  return pending_backend;
}

}  // namespace persistent_cache::sqlite
