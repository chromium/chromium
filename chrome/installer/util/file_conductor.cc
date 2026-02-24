// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/file_conductor.h"

#include <windows.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace installer {

namespace {

enum class OperationResult {
  kSucceeded,
  kPersistentFailure,
  kTransientFailure,
};

// Calls `operation` up to three times with a 100ms pause between each while it
// returns `OperationResult::kTransientFailure`.
bool RetryOnFailure(base::FunctionRef<OperationResult()> operation) {
  static constexpr int kFileSystemAttempts = 3;

  for (int i = 0; i < kFileSystemAttempts; ++i) {
    switch (operation()) {
      case OperationResult::kSucceeded:
        return true;
      case OperationResult::kPersistentFailure:
        return false;
      case OperationResult::kTransientFailure:
        if (i != kFileSystemAttempts - 1) {
          base::PlatformThread::Sleep(base::Milliseconds(100));
        }
        break;
    }
  }
  return false;
}

bool CreateDirectoryWithRetry(const base::FilePath& path) {
  return RetryOnFailure([&path]() {
    return ::CreateDirectory(path.value().c_str(),
                             /*lpSecurityAttributes=*/nullptr) != 0
               ? OperationResult::kSucceeded
               : OperationResult::kTransientFailure;
  });
}

bool RemoveDirectoryWithRetry(const base::FilePath& path) {
  return RetryOnFailure([&path]() {
    return ::RemoveDirectory(path.value().c_str()) != 0
               ? OperationResult::kSucceeded
               : OperationResult::kTransientFailure;
  });
}

// Note: `source` may be left behind on success if `MoveFileEx` falls back to a
// copy-and-delete strategy for a single file. This fallback will never happen
// if `source` names a directory.
bool MoveFileWithRetry(const base::FilePath& source,
                       const base::FilePath& destination) {
  return RetryOnFailure([&source, &destination]() {
    return ::MoveFileEx(source.value().c_str(), destination.value().c_str(),
                        /*dwFlags=*/MOVEFILE_COPY_ALLOWED) != 0
               ? OperationResult::kSucceeded
               : OperationResult::kTransientFailure;
  });
}

bool CopyFileWithRetry(const base::FilePath& source,
                       const base::FilePath& destination) {
  return RetryOnFailure([&source, &destination]() {
    if (::CopyFile(source.value().c_str(), destination.value().c_str(),
                   /*bFailIfExists=*/TRUE) != 0) {
      return OperationResult::kSucceeded;
    }
    const auto error = ::GetLastError();
    return (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
               ? OperationResult::kPersistentFailure
               : OperationResult::kTransientFailure;
  });
}

// Returns true if `path` was successfully deleted; false if `path` did not
// exist or could not be deleted.
bool DeleteFileWithRetry(const base::FilePath& path) {
  return RetryOnFailure([&path]() {
    if (::DeleteFile(path.value().c_str()) != 0) {
      return OperationResult::kSucceeded;
    }
    const auto error = ::GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      return OperationResult::kPersistentFailure;
    }
    if (error != ERROR_ACCESS_DENIED) {
      return OperationResult::kTransientFailure;
    }
    // Deletion might have failed because the read-only attribute is set. Try to
    // clear it and delete again.
    const auto attributes = ::GetFileAttributes(path.value().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_READONLY) == 0) {
      // Failed to get attributes or not read-only. Report the original error.
      ::SetLastError(error);
      return OperationResult::kTransientFailure;
    }
    if (::SetFileAttributes(path.value().c_str(),
                            attributes & ~FILE_ATTRIBUTE_READONLY) == 0) {
      // Failed to set attributes. Leave the last-error code as-is to report
      // this error since the failure to clear the read-only attribute is likely
      // the thing that's preventing deletion.
      return OperationResult::kTransientFailure;
    }
    // Try to delete again now that the read-only attribute has been cleared.
    return ::DeleteFile(path.value().c_str()) != 0
               ? OperationResult::kSucceeded
               : OperationResult::kTransientFailure;
  });
}

}  // namespace

FileConductor::FileConductor(const base::FilePath& backup) : backup_(backup) {}

FileConductor::~FileConductor() {
  // If the instance is destroyed without having previously been undone,
  // anything put into the backup directory may now be deleted.
  while (!cleanup_paths_.empty()) {
    Cleanup(cleanup_paths_.top());
    cleanup_paths_.pop();
  }
}

bool FileConductor::DeleteEntry(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!path.IsAbsolute()) {
    return false;
  }

  // Proactively create a directory into which `path` will be moved.
  base::FilePath backup_dir = CreateBackupDirectory();
  if (backup_dir.empty()) {
    return false;
  }

  // Move `path` into the backup directory so that it will be deleted on success
  // (cleanup=true) but ready to be put back in case of `Undo()`.
  switch (RobustMove(path, backup_dir.Append(path.BaseName()),
                     /*lenient_deletion=*/false, /*cleanup=*/true)) {
    case MoveResult::kSucceeded:  // `path` was moved.
      return true;
    case MoveResult::kNoSource:  // `path` did not exist.
      // The backup directory wasn't needed after all. It will be deleted during
      // cleanup.
      return true;
    case MoveResult::kFailed:  // Some or all of `path` wasn't moved.
      return false;
  }
}

bool FileConductor::MoveEntry(const base::FilePath& source,
                              const base::FilePath& destination,
                              bool lenient_deletion) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!source.IsAbsolute() || !destination.IsAbsolute()) {
    return false;
  }
  return RobustMove(source, destination, lenient_deletion, /*cleanup=*/false) ==
         MoveResult::kSucceeded;
}

bool FileConductor::CopyEntry(const base::FilePath& source,
                              const base::FilePath& destination) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!source.IsAbsolute() || !destination.IsAbsolute()) {
    return false;
  }
  return RobustCopy(source, destination);
}

void FileConductor::Undo() {
  // Make a best-effort attempt to undo every operation; continuing in case of
  // any error along the way.
  while (!undo_items_.empty()) {
    auto& item = undo_items_.top();
    std::visit([](auto&& e) { Undo(e); }, item);
    undo_items_.pop();
  }
  // Do not clean up on destruction following undo.
  base::stack<base::FilePath> empty;
  cleanup_paths_.swap(empty);
}

base::FilePath FileConductor::CreateBackupDirectory() {
  base::FilePath this_backup;
  // Note: `CreateTemporaryDirInDir` performs its own retries, so there's no
  // need to use `RetryOnFailure` for this operation.
  if (!base::CreateTemporaryDirInDir(backup_, /*prefix=*/{}, &this_backup)) {
    PLOG(ERROR) << "Failed to create backup directory in " << backup_;
    return {};
  }
  DirectoryCreated(this_backup, /*cleanup=*/true);
  return this_backup;
}

bool FileConductor::CreateDirectory(const base::FilePath& path, bool cleanup) {
  if (!CreateDirectoryWithRetry(path)) {
    PLOG(ERROR) << "Failed to create directory " << path;
    return false;
  }
  DirectoryCreated(path, cleanup);
  return true;
}

FileConductor::MoveResult FileConductor::RobustMove(
    const base::FilePath& source,
    const base::FilePath& destination,
    bool lenient_deletion,
    bool cleanup) {
  if (MoveFileWithRetry(source, destination)) {
    // It's possible that `MoveFileEx` copied a file but failed to delete it.
    if (!lenient_deletion) {
      // Only report success if `source` was/can be deleted.
      if (DeleteFileWithRetry(source)) {
        VLOG(1) << "Copied " << source << " to " << destination
                << " and deleted the source";
        EntryCopiedAndDeleted(source, destination, cleanup);
        return MoveResult::kSucceeded;
      }
      if (const auto error = ::GetLastError();
          error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
        VLOG(1) << "Moved " << source << " to " << destination;
        EntryMoved(source, destination, cleanup);
        return MoveResult::kSucceeded;
      }
      PLOG(ERROR) << "Failed to delete " << source << " after copying it to "
                  << destination;
      EntryCopied(destination, cleanup);
      return MoveResult::kFailed;
    }
    // Otherwise, report success regardless of whether or not `source` was
    // deleted.
    if (base::PathExists(source)) {
      VLOG(1) << "Copied " << source << " to " << destination
              << " instead of moving; proceeding due to lenient deletion";
      EntryCopied(destination, cleanup);
    } else {
      VLOG(1) << "Moved " << source << " to " << destination;
      EntryMoved(source, destination, cleanup);
    }
    return MoveResult::kSucceeded;
  }

  const DWORD error = ::GetLastError();
  if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
    VLOG(1) << "Could not move " << source << ", as it does not exist";
    return MoveResult::kNoSource;
  }

  if (error == ERROR_ALREADY_EXISTS) {
    // It is a programmer error if `destination` exists. Since the filesystem is
    // a shared resource, it's always possible that some other program has put
    // a file/directory in place, so crashing via CHECK is not the right way to
    // handle this.
    PLOG(ERROR) << "Failed to move " << source << " to " << destination;
    return MoveResult::kFailed;
  }

  // `source` cannot be moved if it is a simple file that is open without
  // `FILE_SHARE_DELETE` (`ERROR_SHARING_VIOLATION`), `source` is a directory
  // and `destination` is on a different volume (`ERROR_NOT_SAME_DEVICE`), or
  // `source` is a directory containing an open file (`ERROR_ACCESS_DENIED`).
  // Assume that it names a directory and attempt to move all of its contents
  // (recursively), deleting its directories as their contents are moved. Don't
  // unconditionally switch to `CopyAndDelete` at this point since it's possible
  // that moving each item in `source` to destination may succeed without
  // further recursion. This will attempt to move each item before falling back
  // to copy-n-delete in the degenerate cross-volume case, but both paths are
  // highly likely to reside on the same volume due to the way this class is
  // used in Chrome's installer.
  VLOG(1) << "Attempting manual move of directory " << source << " to "
          << destination;

  // Recursively move the contents of `source` to `destination` and delete
  // `source` once it is empty.
  switch (ProcessDirectory(
      source, destination, cleanup,
      [this, lenient_deletion](const base::FilePath& move_from,
                               const base::FilePath& move_to, bool cleanup) {
        return RobustMove(move_from, move_to, lenient_deletion, cleanup) !=
               MoveResult::kFailed;
      })) {
    case ProcessDirectoryResult::kSucceeded:
      // Delete the empty `source` directory now that everything within has been
      // moved.
      if (RemoveDirectoryWithRetry(source)) {
        DirectoryRemoved(source);
      } else if (lenient_deletion) {
        VPLOG(1) << "Failed to delete empty directory " << source
                 << "; proceeding due to lenient deletion";
      } else {
        PLOG(ERROR) << "Failed to delete empty directory " << source;
        return MoveResult::kFailed;
      }
      return MoveResult::kSucceeded;

    case ProcessDirectoryResult::kCantEnumerate:
      // Perhaps `source` isn't a directory after all. Try to copy it to
      // `destination` and delete it.
      return CopyAndDelete(source, destination, lenient_deletion, cleanup)
                 ? MoveResult::kSucceeded
                 : MoveResult::kFailed;

    case ProcessDirectoryResult::kFailed:
      return MoveResult::kFailed;
  }
}

bool FileConductor::CopyAndDelete(const base::FilePath& source,
                                  const base::FilePath& destination,
                                  bool lenient_deletion,
                                  bool cleanup) {
  if (CopyFileWithRetry(source, destination)) {
    // `source` is positively a file rather than a directory at this point.
    if (DeleteFileWithRetry(source)) {
      VLOG(1) << "Copied " << source << " to " << destination
              << " and deleted the source";
      EntryCopiedAndDeleted(source, destination, cleanup);
      return true;
    }
    EntryCopied(destination, cleanup);
    if (lenient_deletion) {
      VPLOG(1) << "Copied " << source << " to " << destination
               << " but failed to delete it; proceeding due to lenient "
                  "deletion";
      return true;
    }
    PLOG(ERROR) << "Failed to delete " << source << " after copying it to "
                << destination;
    return false;
  }
  // Else `source` might be a directory or this process might lack permission to
  // copy it. Retain the error from the attempt to copy `source`.
  const DWORD copy_error = ::GetLastError();

  VLOG(1) << "Attempting manual copy of presumed directory " << source << " to "
          << destination;

  // Recursively copy the contents of `source` to `destination`, deleting each
  // item as it goes and deleting `source` once it is empty.
  switch (ProcessDirectory(
      source, destination, cleanup,
      [this, lenient_deletion](const base::FilePath& copy_from,
                               const base::FilePath& copy_to, bool cleanup) {
        return CopyAndDelete(copy_from, copy_to, lenient_deletion, cleanup);
      })) {
    case ProcessDirectoryResult::kSucceeded:
      // Delete the empty `source` directory.
      if (RemoveDirectoryWithRetry(source)) {
        DirectoryRemoved(source);
      } else if (lenient_deletion) {
        VPLOG(1) << "Failed to delete empty directory " << source
                 << "; proceeding due to lenient deletion";
      } else {
        PLOG(ERROR) << "Failed to delete empty directory " << source;
        return false;
      }
      return true;

    case ProcessDirectoryResult::kCantEnumerate:
      // Perhaps `source` isn't a directory or the process has insufficient
      // permission to enumerate its contents. Restore the error from the
      // attempt to copy it above so that it is included in the log.
      ::SetLastError(copy_error);
      PLOG(ERROR) << "Failed to copy " << source << " to " << destination
                  << " with the following error, then failed to enumerate its"
                     " contents to copy it recursively";
      return false;

    case ProcessDirectoryResult::kFailed:
      // Failed while recursing.
      return false;
  }
}

bool FileConductor::RobustCopy(const base::FilePath& source,
                               const base::FilePath& destination) {
  if (CopyFileWithRetry(source, destination)) {
    EntryCopied(destination, /*cleanup=*/false);
    VLOG(1) << "Copied " << source << " to " << destination;
    return true;
  }
  const auto error = ::GetLastError();
  if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
    VLOG(1) << "Could not copy " << source << ", as it does not exist";
    return false;
  }
  PLOG(ERROR) << "Failed to copy " << source << " to " << destination;
  VLOG(1) << "Attempting manual copy of directory " << source << " to "
          << destination;
  return ProcessDirectory(source, destination, /*cleanup=*/false,
                          [this](const base::FilePath& copy_from,
                                 const base::FilePath& copy_to, bool cleanup) {
                            return RobustCopy(copy_from, copy_to);
                          }) == ProcessDirectoryResult::kSucceeded;
}

FileConductor::ProcessDirectoryResult FileConductor::ProcessDirectory(
    const base::FilePath& source,
    const base::FilePath& destination,
    bool cleanup,
    base::FunctionRef<bool(const base::FilePath&, const base::FilePath&, bool)>
        operation) {
  // Process each file/directory in `source`; creating the destination directory
  // on demand.
  bool destination_created = false;
  base::FileEnumerator file_enumerator(
      source, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      /*pattern=*/{}, base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (auto entry = file_enumerator.Next(); !entry.empty();
       entry = file_enumerator.Next()) {
    if (!destination_created) {
      // Create the target `destination` directory now that a first item has
      // been found.
      if (!CreateDirectory(destination, cleanup)) {
        return ProcessDirectoryResult::kFailed;
      }
      destination_created = true;
    }

    if (!operation(entry, destination.Append(entry.BaseName()), cleanup)) {
      return ProcessDirectoryResult::kFailed;
    }
  }

  if (file_enumerator.GetError() != base::File::FILE_OK) {
    // Enumeration stopped prematurely due to an error.
    if (!destination_created) {
      // This error originates from the call to FindFirstFileEx for `source`. It
      // likely means that `source` isn't a directory, but could also mean that
      // the process lacks permission or some other issue. Callers of
      // ProcessDirectory typically do so after some other operation fails
      // without knowing for certain whether `source` is a file or directory, so
      // do not emit a log message in this case.
      PLOG_IF(ERROR, ::GetLastError() != ERROR_DIRECTORY)
          << "Failed to enumerate contents of directory " << source;
      return ProcessDirectoryResult::kCantEnumerate;
    }
    // Else this error originates from FindNextFile.
    PLOG(ERROR) << "Failed while enumerating contents of directory " << source;
    return ProcessDirectoryResult::kFailed;
  }

  // `destination_created` is false if `source` was an empty directory. Create
  // the destination now to complete processing.
  if (!destination_created && !CreateDirectory(destination, cleanup)) {
    return ProcessDirectoryResult::kFailed;
  }

  return ProcessDirectoryResult::kSucceeded;
}

void FileConductor::EntryMoved(const base::FilePath& source,
                               const base::FilePath& destination,
                               bool cleanup) {
  undo_items_.emplace(MovedState{source, destination});
  if (cleanup) {
    cleanup_paths_.push(destination);
  }
}

void FileConductor::DirectoryCreated(const base::FilePath& directory,
                                     bool cleanup) {
  undo_items_.emplace(CreatedState{directory});
  if (cleanup) {
    cleanup_paths_.push(directory);
  }
}

void FileConductor::DirectoryRemoved(const base::FilePath& directory) {
  undo_items_.emplace(RemovedState{directory});
}

void FileConductor::EntryCopied(const base::FilePath& destination,
                                bool cleanup) {
  undo_items_.emplace(CopiedState{destination});
  if (cleanup) {
    cleanup_paths_.push(destination);
  }
}

void FileConductor::EntryCopiedAndDeleted(const base::FilePath& source,
                                          const base::FilePath& destination,
                                          bool cleanup) {
  undo_items_.emplace(CopiedAndDeletedState{source, destination});
  if (cleanup) {
    cleanup_paths_.push(destination);
  }
}

// static
void FileConductor::Undo(const MovedState& state) {
  if (!MoveFileWithRetry(state.destination, state.source)) {
    PLOG(ERROR) << "MoveFileEx failed while reversing movement of "
                << state.source << " to " << state.destination;
  }
}

// static
void FileConductor::Undo(const CreatedState& state) {
  if (!RemoveDirectoryWithRetry(state.directory)) {
    PLOG(ERROR) << "RemoveDirectory failed while reversing creation of "
                << state.directory;
  }
}

// static
void FileConductor::Undo(const RemovedState& state) {
  if (!::CreateDirectory(state.directory.value().c_str(),
                         /*lpSecurityAttributes=*/nullptr)) {
    PLOG(ERROR) << "CreateDirectory failed while reversing removal of "
                << state.directory;
  }
}

// static
void FileConductor::Undo(const CopiedState& state) {
  if (!DeleteFileWithRetry(state.destination)) {
    PLOG(ERROR) << "DeleteFile failed while reversing copy to "
                << state.destination;
  }
}

// static
void FileConductor::Undo(const CopiedAndDeletedState& state) {
  if (!CopyFileWithRetry(state.destination, state.source)) {
    PLOG(ERROR) << "CopyFile failed while reversing copy-and-move of "
                << state.source << " to " << state.destination;
  } else if (!DeleteFileWithRetry(state.destination)) {
    PLOG(ERROR) << "DeleteFile failed while reversing copy-and-move of "
                << state.source << " to " << state.destination;
  }
}

// static
void FileConductor::Cleanup(const base::FilePath& path) {
  // `path` may not be deletable on account of it or one of its contents being
  // opened without `FILE_SHARE_DELETE`.
  if (!base::DeletePathRecursively(path)) {
    PLOG(WARNING) << "Failed to fully delete " << path << " on cleanup.";
  }
}

}  // namespace installer
