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
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/constants.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/metrics_util.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "components/sqlite_vfs/sandboxed_file.h"
#include "components/sqlite_vfs/shared_locks.h"
#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

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
  PendingFileSet pending_file_set;

  const auto root_path = directory.Append(base_name);
  const auto db_file_path = root_path.AddExtension(kDbFileExtension);
  const auto journal_file_path = root_path.AddExtension(kJournalFileExtension);
  const auto wal_file_path = root_path.AddExtension(kWalJournalFileExtension);
  const auto wal_index_file_path =
      directory
          .Append(base::FilePath::FromASCII(
              base::Uuid::GenerateRandomV4().AsLowercaseString()))
          .AddExtension(kWalIndexFileExtension);

  // A helper that is run on exit in case of error to delete any files created
  // during operation.
  absl::Cleanup clean_on_error = [&]() {
    if (!pending_file_set.db_file.IsValid() ||
        !pending_file_set.db_file.created()) {
      // Nothing to do if the main database file was not created.
      return;
    }
    // Delete all files before closing them.
    const auto delete_file = [](const base::FilePath& path, base::File file) {
      if (file.IsValid()) {
#if BUILDFLAG(IS_WIN)
        if (file.DeleteOnClose(true)) {
          return;
        }
#endif
        base::DeleteFile(path);
      }
    };

    delete_file(db_file_path, std::move(pending_file_set.db_file));
    delete_file(journal_file_path, std::move(pending_file_set.journal_file));
    delete_file(wal_file_path, std::move(pending_file_set.wal_file));
    delete_file(wal_index_file_path,
                std::move(pending_file_set.wal_index_file));
  };

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

  // The main database file.
  pending_file_set.db_file = base::File(db_file_path, create_flags);
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "CreateResult", FileType::kMainDb),
      -pending_file_set.db_file.error_details(), -base::File::FILE_ERROR_MAX);
  if (!pending_file_set.db_file.IsValid()) {
    return std::nullopt;
  }

  // The rollback journal file is always needed; even if write-ahead logging is
  // desired.
  pending_file_set.journal_file = base::File(journal_file_path, create_flags);
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "CreateResult", FileType::kMainJournal),
      -pending_file_set.journal_file.error_details(),
      -base::File::FILE_ERROR_MAX);
  if (!pending_file_set.journal_file.IsValid()) {
    return std::nullopt;
  }

  // The write-ahead log file must be created if WAL-mode is requested. An
  // existing one must be opened even if WAL-mode is not requested to enable
  // migration from WAL back to use of a rollback journal.
  pending_file_set.wal_file = base::File(
      wal_file_path,
      (journal_mode_wal ? create_flags
                        : ((create_flags & ~base::File::FLAG_OPEN_ALWAYS) |
                           base::File::FLAG_OPEN)));
  base::UmaHistogramExactLinear(
      GetHistogramName(client, "CreateResult", FileType::kWal),
      -pending_file_set.wal_file.error_details(), -base::File::FILE_ERROR_MAX);
  if (!pending_file_set.wal_file.IsValid() &&
      (journal_mode_wal || pending_file_set.wal_file.error_details() !=
                               base::File::FILE_ERROR_NOT_FOUND)) {
    return std::nullopt;
  }

  if (!single_connection && pending_file_set.wal_file.IsValid()) {
    // Shared WAL-mode connections use a -shm file to hold the WAL-index. This
    // file is mapped into the address space of all processes connecting to the
    // database. Upon first connecting to a database, any contents in the -shm
    // file left behind from dirty closure of the database must be discarded.
    // The contents of the WAL-index are rebuilt by the first writer of the
    // database. SQLite's default VFS implementations use a so-called "deadman
    // switch lock" (DMS) to determine which connection is responsible for
    // erasing stale contents of the WAL-index and rebuilding it.
    //
    // sqlite_vfs has a hard requirement that MakePendingFileSet must be used to
    // make the initial connection to a database, and that ShareConnection must
    // be used to create additional connections. This requirement is enforced on
    // Windows by opening the -shm file for exclusive read/write access -- this
    // will fail with a sharing violation if the file is already in use.
    //
    // It is tempting to use base::File::Lock() on POSIX systems to detect an
    // attempt to make a pending file set for a database that is in-use. This
    // will fail due to the nature of locks on POSIX -- the lock will not be
    // transferred to another process along with the pending file set, and the
    // lock will be released if a second attempt is made to create a pending
    // file set while the files are open.
    //
    // The task of discarding any stale data left behind is handled by
    // unconditionally using a new -shm file for each file set. Handling differs
    // by platform:
    //
    // - On Windows, FLAG_DELETE_ON_CLOSE is used so that the file is
    //   automatically deleted once all handles are closed. Since this file is
    //   mapped into the address spaces of all processes connection to a
    //   database with the generated file set, it is not reliable to delete the
    //   file via DeleteFile. Additionally, FLAG_WIN_TEMPORARY is used as a hint
    //   to the OS that the data does not need to be written to disk. This will
    //   avoid I/O provided that there is sufficient cache to hold the index.
    //
    // - On POSIX systems, a second read-only handle to the file is opened
    //   immediately and then the file is unlinked. The read-only handle is kept
    //   in the file set and duplicated when shared for read-only access.
#if BUILDFLAG(IS_WIN)
    pending_file_set.wal_index_file = base::File(
        wal_index_file_path, (create_flags & ~base::File::FLAG_OPEN_ALWAYS) |
                                 base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_WIN_EXCLUSIVE_READ |
                                 base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                 base::File::FLAG_WIN_TEMPORARY |
                                 base::File::FLAG_DELETE_ON_CLOSE);
#else
    pending_file_set.wal_index_file = base::File(
        wal_index_file_path, (create_flags & ~base::File::FLAG_OPEN_ALWAYS) |
                                 base::File::FLAG_CREATE_ALWAYS);

    if (pending_file_set.wal_index_file.IsValid()) {
      // It's not possible to get a new file descriptor with reduced permissions
      // to the same file description, so open the file anew with read-only
      // access before deleting it.
      pending_file_set.wal_index_file_read_only = base::File(
          wal_index_file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

      // The file no longer needs to be accessible.
      base::File::Error delete_result = base::DeleteFile(wal_index_file_path)
                                            ? base::File::FILE_OK
                                            : base::File::GetLastFileError();
      base::UmaHistogramExactLinear(
          GetHistogramName(client, "DeleteResult", FileType::kWalIndex),
          -delete_result, -base::File::FILE_ERROR_MAX);

      base::UmaHistogramExactLinear(
          GetHistogramName(client, "CreateResult", FileType::kWalIndexReadOnly),
          -pending_file_set.wal_index_file_read_only.error_details(),
          -base::File::FILE_ERROR_MAX);
      if (!pending_file_set.wal_index_file_read_only.IsValid()) {
        return std::nullopt;
      }
    }
#endif

    base::UmaHistogramExactLinear(
        GetHistogramName(client, "CreateResult", FileType::kWalIndex),
        -pending_file_set.wal_index_file.error_details(),
        -base::File::FILE_ERROR_MAX);
    if (!pending_file_set.wal_index_file.IsValid()) {
      return std::nullopt;
    }
  }

  if (!single_connection) {
    // The shared lock is only needed if multiple connections are permitted.
    pending_file_set.shared_lock = SharedLocks::CreateRegion(
        /*wal_mode=*/pending_file_set.wal_file.IsValid());
    if (!pending_file_set.shared_lock.IsValid()) {
      return std::nullopt;
    }
  }

  pending_file_set.read_write = true;
  pending_file_set.wal_mode = journal_mode_wal;

  // Creation succeeded, so cancel the error handler.
  std::move(clean_on_error).Cancel();

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

  if (file_set.has_wal_file()) {
    pending_file_set.wal_file = DuplicateFile(
        file_set.GetWalJournalFile(),
        directory.Append(base_name).AddExtension(kWalJournalFileExtension),
        !file_set.read_only(), read_write);
    if (!pending_file_set.wal_file.IsValid()) {
      return std::nullopt;
    }

#if BUILDFLAG(IS_WIN)
    pending_file_set.wal_index_file = DuplicateFile(
        file_set.GetWalIndexFile(),
        /*source_file_path=*/{}, !file_set.read_only(), read_write);
#else
    if (!read_write && !file_set.read_only()) {
      // Sharing a read-only connection from a read/write file set. Use the
      // read-only handle to the WAL-index.
      pending_file_set.wal_index_file =
          file_set.GetWalIndexFileReadOnly().Duplicate();
    } else {
      // Can't upgrade from read-only to read-write.
      CHECK(!read_write || !file_set.read_only());
      pending_file_set.wal_index_file = file_set.GetWalIndexFile().Duplicate();
      if (read_write) {
        pending_file_set.wal_index_file_read_only =
            file_set.GetWalIndexFileReadOnly().Duplicate();
      }
    }
#endif
    if (!pending_file_set.wal_index_file.IsValid()) {
      return std::nullopt;
    }
  }

  pending_file_set.read_write = read_write;
  pending_file_set.wal_mode = file_set.wal_mode();

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
