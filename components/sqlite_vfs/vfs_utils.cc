// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/vfs_utils.h"

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "components/sqlite_vfs/constants.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "components/sqlite_vfs/sandboxed_file.h"
#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace sqlite_vfs {

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

std::optional<PendingFileSet> MakePendingFileSet(
    Client client,
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal) {
  // Write-ahead logging journaling is only supported for single connections.
  CHECK(!journal_mode_wal || single_connection);
  PendingFileSet pending_file_set;

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

  auto db_file_path =
      directory.Append(base_name).AddExtension(kDbFileExtension);
  pending_file_set.db_file = base::File(db_file_path, create_flags);
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "CreateResult", FileType::kMainDb),
      -pending_file_set.db_file.error_details(), -base::File::FILE_ERROR_MAX);
  if (!pending_file_set.db_file.IsValid()) {
    return std::nullopt;
  }

  auto journal_file_path =
      directory.Append(base_name).AddExtension(kJournalFileExtension);
  pending_file_set.journal_file = base::File(journal_file_path, create_flags);
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "CreateResult", FileType::kMainJournal),
      -pending_file_set.journal_file.error_details(),
      -base::File::FILE_ERROR_MAX);
  if (!pending_file_set.journal_file.IsValid()) {
    return std::nullopt;
  }

  if (journal_mode_wal) {
    auto wal_file_path =
        directory.Append(base_name).AddExtension(kWalJournalFileExtension);
    pending_file_set.wal_file = base::File(wal_file_path, create_flags);
    base::UmaHistogramExactLinear(
        GetHistogramName(client, "CreateResult", FileType::kWal),
        -pending_file_set.wal_file.error_details(),
        -base::File::FILE_ERROR_MAX);
    if (!pending_file_set.wal_file.IsValid()) {
      return std::nullopt;
    }
  }

  if (!single_connection) {
    // The shared lock is only needed if multiple connections are permitted.
    pending_file_set.shared_lock =
        base::UnsafeSharedMemoryRegion::Create(sizeof(SharedAtomicLock));
    if (!pending_file_set.shared_lock.IsValid()) {
      return std::nullopt;
    }
  }

  pending_file_set.read_write = true;

  return pending_file_set;
}

std::optional<PendingFileSet> ShareConnection(const base::FilePath& directory,
                                              const base::FilePath& base_name,
                                              const SqliteVfsFileSet& file_set,
                                              bool read_write) {
  // Cannot share a single-connection backend. If it ever becomes interesting to
  // connect to a backend in one process and then move it to another process,
  // we shall introduce a way to `Unbind()` a backend to convert it back into a
  // `PendingFileSet`.
  CHECK(!file_set.is_single_connection());
  // All connections using a write-ahead log are single-connection.
  CHECK(!file_set.wal_journal_mode());

  PendingFileSet pending_file_set;

  pending_file_set.db_file =
      DuplicateFile(file_set.GetDbFile(),
                    directory.Append(base_name).AddExtension(kDbFileExtension),
                    !file_set.read_only(), read_write);
  if (!pending_file_set.db_file.IsValid()) {
    return std::nullopt;
  }

  pending_file_set.journal_file = DuplicateFile(
      file_set.GetJournalFile(),
      directory.Append(base_name).AddExtension(kJournalFileExtension),
      !file_set.read_only(), read_write);
  if (!pending_file_set.journal_file.IsValid()) {
    return std::nullopt;
  }

  pending_file_set.shared_lock = file_set.GetSharedLock().Duplicate();
  if (!pending_file_set.shared_lock.IsValid()) {
    return std::nullopt;
  }

  pending_file_set.read_write = read_write;

  return pending_file_set;
}

base::FilePath GetBaseName(const base::FilePath& file) {
  return file.MatchesFinalExtension(kDbFileExtension)
             ? file.BaseName().RemoveFinalExtension()
             : base::FilePath();
}

int64_t DeleteFiles(Client client,
                    const base::FilePath& directory,
                    const base::FilePath& base_name) {
  auto file_path = directory.Append(base_name).AddExtension(kDbFileExtension);
  int64_t bytes_recovered = base::GetFileSize(file_path).value_or(0);
  base::File::Error delete_result = base::DeleteFile(file_path)
                                        ? base::File::FILE_OK
                                        : base::File::GetLastFileError();
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "DeleteResult", FileType::kMainDb),
      -delete_result, -base::File::FILE_ERROR_MAX);
  if (delete_result != base::File::FILE_OK) {
    return 0;
  }

  file_path = directory.Append(base_name).AddExtension(kJournalFileExtension);
  auto file_size = base::GetFileSize(file_path).value_or(0);
  delete_result = base::DeleteFile(file_path) ? base::File::FILE_OK
                                              : base::File::GetLastFileError();
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "DeleteResult", FileType::kMainJournal),
      -delete_result, -base::File::FILE_ERROR_MAX);
  if (delete_result == base::File::FILE_OK) {
    bytes_recovered = base::ClampAdd(bytes_recovered, file_size);
  }

  file_path =
      directory.Append(base_name).AddExtension(kWalJournalFileExtension);
  file_size = base::GetFileSize(file_path).value_or(0);
  delete_result = base::DeleteFile(file_path) ? base::File::FILE_OK
                                              : base::File::GetLastFileError();
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "DeleteResult", FileType::kWal), -delete_result,
      -base::File::FILE_ERROR_MAX);
  if (delete_result == base::File::FILE_OK) {
    bytes_recovered = base::ClampAdd(bytes_recovered, file_size);
  }

  // TODO (https://crbug.com/377475540): Cleanup when deletion of journal
  // failed.
  return bytes_recovered;
}

}  // namespace sqlite_vfs
