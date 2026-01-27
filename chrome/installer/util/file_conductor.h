// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_FILE_CONDUCTOR_H_
#define CHROME_INSTALLER_UTIL_FILE_CONDUCTOR_H_

#include <stack>
#include <variant>

#include "base/files/file_path.h"
#include "base/win/windows_types.h"

namespace installer {

// A helper for conducting a sequence of operations on files and/or directories.
// Existing items in the filesystem that are replaced by operation(s) are moved
// into a backup directory so that they can be reversed (via `Undo()`) if
// needed. On destruction, items placed the backup directory that have not been
// consumed via `Undo()` are permanently deleted.
//
// Cross-volume operations are not supported. In particular, the backup
// directory provided at construction must be on the same volume as all other
// paths provided.
class FileConductor {
 public:
  // `backup` names a directory into which files and/or directories will be
  // placed during operation.
  explicit FileConductor(const base::FilePath& backup);
  FileConductor(const FileConductor&) = delete;
  FileConductor& operator=(const FileConductor&) = delete;
  ~FileConductor();

  // Deletes a file or directory, preserving any existing item in the instance's
  // backup directory until destination in case of undo.
  bool DeleteEntry(const base::FilePath& path);

  // Moves `source` to `destination` non-destructively (fails if `destination`
  // exists).
  bool MoveEntry(const base::FilePath& source,
                 const base::FilePath& destination);

  // Makes a best-effort attempt to reverse all operations performed.
  void Undo();

 private:
  // Structs to hold state to be reversed in case of Undo.
  struct MovedState {
    base::FilePath source;
    base::FilePath destination;
  };
  struct CreatedState {
    base::FilePath directory;
  };
  struct RemovedState {
    base::FilePath directory;
  };

  // A simple wrapper around `MoveFileEx` that does not fall-back to performing
  // a copy and does not replace an existing file. An item is added to the undo
  // stack on success. If `cleanup` is true, `destination` will be deleted
  // during the cleanup phase at destruction. Returns `ERROR_SUCCESS` on success
  // or a Windows error code on failure. Some examples:
  // - ERROR_ALREADY_EXISTS: `destination` names an existing file or directory.
  // - ERROR_SHARING_VIOLATION: `source` is open without `FILE_SHARE_DELETE`.
  // - ERROR_ACCESS_DENIED: `source` is a directory containing open file(s).
  // - ERROR_NOT_SAME_DEVICE: `source` and `destination` reside on different
  // volumes.
  DWORD TrivialMove(const base::FilePath& source,
                    const base::FilePath& destination,
                    bool cleanup);

  // Creates a new directory within the instance's backup directory and returns
  // its path. An item is added to the undo stack on success. The directory will
  // be deleted during the cleanup phase at destruction. Returns an empty path
  // on error.
  base::FilePath CreateBackupDirectory();

  // Creates a directory and adds an item to the undo stack. If `cleanup` is
  // true, `destination` will be deleted during the cleanup phase at
  // destruction.
  bool CreateDirectory(const base::FilePath& path, bool cleanup);

  // Recursively moves the contents of `source` to `destination`, deleting
  // directories as they are emptied. Returns false if `source` does not name a
  // directory. On success, `source` will no longer be present. Items are added
  // to the undo stack for each directory created in `destination`, each item
  // moved from `source` to `destination`, and each directory removed from
  // `source`. If `cleanup` is true, `destination` will be deleted during the
  // cleanup phase at destruction.
  bool MoveDirectoryRecursive(const base::FilePath& source,
                              const base::FilePath& destination,
                              bool cleanup);

  enum class MoveResult { kSucceeded, kNoSource, kFailed };
  // Moves `source` to `destination`, using a recursive strategy if needed;
  // e.g., if one or more files in `source` are in-use. If `cleanup` is true,
  // `destination` will be deleted during the cleanup phase at destruction.
  MoveResult RobustMove(const base::FilePath& source,
                        const base::FilePath& destination,
                        bool cleanup);

  // Adds state to the undo stack following an operation. `cleanup`, if true,
  // indicates that the preceding argument (`destination` or `directory`) should
  // be deleted at destruction during the cleanup phase.
  void EntryMoved(const base::FilePath& source,
                  const base::FilePath& destination,
                  bool cleanup);
  void DirectoryCreated(const base::FilePath& directory, bool cleanup);
  void DirectoryRemoved(const base::FilePath& directory);

  // Helper functions to reverse each state transition in the undo stack.
  static void Undo(const MovedState& state);
  static void Undo(const CreatedState& state);
  static void Undo(const RemovedState& state);

  // Recursively deletes `path`.
  static void Cleanup(const base::FilePath& path);

  // A directory into which arbitrary files/directories will be placed as-needed
  // in support of Undo.
  const base::FilePath backup_;

  // The undo stack, holding state for operations that will be reversed in case
  // of Undo.
  std::stack<std::variant<MovedState, CreatedState, RemovedState>> undo_items_;

  // The cleanup stack, holding paths that will be deleted during the cleanup
  // phase at destruction.
  std::stack<base::FilePath> cleanup_paths_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_FILE_CONDUCTOR_H_
