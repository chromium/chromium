// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/file_conductor.h"

#include <windows.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace installer {

namespace {

// Calls `operation` up to three times with a 100ms pause between each until it
// returns true.
bool RetryOnFailure(base::FunctionRef<bool()> operation) {
  static constexpr int kFileSystemAttempts = 3;

  for (int i = 0; i < kFileSystemAttempts; ++i) {
    if (operation()) {
      return true;
    }
    if (i != kFileSystemAttempts - 1) {
      base::PlatformThread::Sleep(base::Milliseconds(100));
    }
  }
  return false;
}

bool CreateDirectoryWithRetry(const base::FilePath& path) {
  return RetryOnFailure([&path]() {
    return ::CreateDirectory(path.value().c_str(),
                             /*lpSecurityAttributes=*/nullptr);
  });
}

bool RemoveDirectoryWithRetry(const base::FilePath& path) {
  return RetryOnFailure(
      [&path]() { return ::RemoveDirectory(path.value().c_str()) != 0; });
}

bool MoveFileWithRetry(const base::FilePath& source,
                       const base::FilePath& destination) {
  return RetryOnFailure([&source, &destination]() {
    return ::MoveFileEx(source.value().c_str(), destination.value().c_str(),
                        /*dwFlags=*/0) != 0;
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

  // Proactively create a directory into which `path` will be moved.
  base::FilePath backup_dir = CreateBackupDirectory();
  if (backup_dir.empty()) {
    return false;
  }

  // Move `path` into the backup directory so that it will be deleted on success
  // (cleanup=true) but ready to be put back in case of `Undo()`.
  switch (
      RobustMove(path, backup_dir.Append(path.BaseName()), /*cleanup=*/true)) {
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
                              const base::FilePath& destination) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  return RobustMove(source, destination, /*cleanup=*/false) ==
         MoveResult::kSucceeded;
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
  std::stack<base::FilePath> empty;
  cleanup_paths_.swap(empty);
}

DWORD FileConductor::TrivialMove(const base::FilePath& source,
                                 const base::FilePath& destination,
                                 bool cleanup) {
  if (!MoveFileWithRetry(source, destination)) {
    const auto error = ::GetLastError();
    PLOG(ERROR) << "Failed to move " << source << " to " << destination;
    return error;
  }
  EntryMoved(source, destination, cleanup);
  VLOG(1) << "Moved " << source << " to " << destination;
  return ERROR_SUCCESS;
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

bool FileConductor::MoveDirectoryRecursive(const base::FilePath& source,
                                           const base::FilePath& destination,
                                           bool cleanup) {
  // Move each file/directory; creating the destination directory on demand.
  bool destination_created = false;
  base::FileEnumerator file_enumerator(
      source, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      /*pattern=*/{}, base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (auto entry = file_enumerator.Next(); !entry.empty();
       entry = file_enumerator.Next()) {
    if (!destination_created) {
      // Create the target `destination` directory now that a first item to move
      // has been found.
      if (!CreateDirectory(destination, cleanup)) {
        return false;
      }
      destination_created = true;
    }

    if (RobustMove(entry, destination.Append(entry.BaseName()), cleanup) ==
        MoveResult::kFailed) {
      return false;
    }
  }
  if (file_enumerator.GetError() != base::File::FILE_OK) {
    // `source` could not be fully enumerated; possibly because it is not a
    // directory.
    PLOG(ERROR) << "Failed to enumerate contents of directory " << source;
    return false;
  }

  // `destination_created` is false if `source` was an empty directory. Create
  // the destination now before trying to delete `source`.
  if (!destination_created && !CreateDirectory(destination, cleanup)) {
    return false;
  }

  // Delete the empty `source` directory.
  if (!RemoveDirectoryWithRetry(source)) {
    PLOG(ERROR) << "Failed to delete empty directory " << source;
    return false;
  }
  DirectoryRemoved(source);

  return true;
}

FileConductor::MoveResult FileConductor::RobustMove(
    const base::FilePath& source,
    const base::FilePath& destination,
    bool cleanup) {
  DWORD move_result = TrivialMove(source, destination, cleanup);
  if (move_result == ERROR_SUCCESS) {
    return MoveResult::kSucceeded;
  }
  if (move_result == ERROR_FILE_NOT_FOUND ||
      move_result == ERROR_PATH_NOT_FOUND) {
    VLOG(1) << "Could not move " << source << ", as it does not exist";
    return MoveResult::kNoSource;
  }

  if (move_result == ERROR_ALREADY_EXISTS) {
    // It is a programmer error if `destination` exists. Since the filesystem is
    // a shared resource, it's always possible that some other program has put
    // a file/directory in place, so crashing via CHECK is not the right way to
    // handle this.
    LOG(ERROR) << "Failed to move " << source << " to " << destination;
    return MoveResult::kFailed;
  }

  // `source` cannot be moved if it is a simple file that is open without
  // `FILE_SHARE_DELETE` (`ERROR_SHARING_VIOLATION`) or is a directory
  // containing an open file (`ERROR_ACCESS_DENIED`). Assume that it names a
  // directory and attempt to move all of its contents (recursively), deleting
  // its directories as their contents are moved.
  VLOG(1) << "Attempting manual move of directory " << source << " to "
          << destination;
  return MoveDirectoryRecursive(source, destination, cleanup)
             ? MoveResult::kSucceeded
             : MoveResult::kFailed;
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
void FileConductor::Cleanup(const base::FilePath& path) {
  // `path` may not be deletable on account of it or one of its contents being
  // opened without `FILE_SHARE_DELETE`.
  if (!base::DeletePathRecursively(path)) {
    PLOG(WARNING) << "Failed to fully delete " << path << " on cleanup.";
  }
}

}  // namespace installer
