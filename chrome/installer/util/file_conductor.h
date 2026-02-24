// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_FILE_CONDUCTOR_H_
#define CHROME_INSTALLER_UTIL_FILE_CONDUCTOR_H_

#include <variant>

#include "base/containers/stack.h"
#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/win/windows_types.h"

namespace installer {

// A helper for conducting a sequence of operations on files and/or directories.
// Existing items in the filesystem that are replaced by operation(s) are moved
// into a backup directory so that they can be reversed (via `Undo()`) if
// needed. On destruction, items placed in the backup directory that have not
// been consumed via `Undo()` are permanently deleted.
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
  // exists). Returns true if `source` is fully moved to `destination` (i.e.,
  // `source` no longer exists) or if `lenient_deletion` is true and some or all
  // of `source` could not be deleted following moving/copying it to
  // `destination`.
  bool MoveEntry(const base::FilePath& source,
                 const base::FilePath& destination,
                 bool lenient_deletion = false);

  // Copies `source` to `destination` non-destructively (fails if `destination`
  // exists).
  bool CopyEntry(const base::FilePath& source,
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
  struct CopiedAndDeletedState {
    base::FilePath source;
    base::FilePath destination;
  };
  struct CopiedState {
    base::FilePath destination;
  };

  // Creates a new directory within the instance's backup directory and returns
  // its path. An item is added to the undo stack on success. The directory will
  // be deleted during the cleanup phase at destruction. Returns an empty path
  // on error.
  base::FilePath CreateBackupDirectory();

  // Creates a directory and adds an item to the undo stack. If `cleanup` is
  // true, `destination` will be deleted during the cleanup phase at
  // destruction.
  bool CreateDirectory(const base::FilePath& path, bool cleanup);

  enum class MoveResult { kSucceeded, kNoSource, kFailed };
  // Moves `source` to `destination` by the first of the following strategies
  // that succeeds:
  // 1) A simple move. This is efficient, but may fail for various reasons
  //    (e.g., `source` and `destination` are on different volumes or `source`
  //    is a directory and a process has one of its files open).
  // 2) If `source` names a directory, it is moved by creating the directory
  //    `destination`, moving each item in `source` into it, then deleting
  //    `source.
  // 3) If `source` does not name a directory, it is copied to `destination` and
  //    deleted.
  // Returns `kNoSource` if `source` does not exist; `kSucceeded` if `source`
  // was entirely moved to `destination` or if `lenient_deletion` is true and
  // some or all of `source` could not be deleted; or `kFailed` otherwise.
  MoveResult RobustMove(const base::FilePath& source,
                        const base::FilePath& destination,
                        bool lenient_deletion,
                        bool cleanup);

  // Moves `source` to `destination` using a copy-and-delete approach. If
  // `cleanup` is true, `destination` will be deleted during the cleanup phase
  // at destruction.
  bool CopyAndDelete(const base::FilePath& source,
                     const base::FilePath& destination,
                     bool lenient_deletion,
                     bool cleanup);

  // Copies `source` to `destination`
  bool RobustCopy(const base::FilePath& source,
                  const base::FilePath& destination);

  // Runs `operation` on each file and directory in `source`; providing it with
  // the full path to the entry, the full path to its location under
  // `destination`, and `cleanup`. A directory with the same base name as
  // `source` is created within `destination` if `source` is a
  // directory. Returns false if `source` is not a directory or if any operation
  // fails (creating the destination, doing the enumeration, or performing the
  // operation). Returns true if `source` is a directory and all operations
  // succeed.
  enum class ProcessDirectoryResult { kSucceeded, kCantEnumerate, kFailed };
  ProcessDirectoryResult ProcessDirectory(
      const base::FilePath& source,
      const base::FilePath& destination,
      bool cleanup,
      base::FunctionRef<
          bool(const base::FilePath&, const base::FilePath&, bool)> operation);

  // Adds state to the undo stack following an operation. `cleanup`, if true,
  // indicates that the preceding argument (`destination` or `directory`) should
  // be deleted at destruction during the cleanup phase.
  void EntryMoved(const base::FilePath& source,
                  const base::FilePath& destination,
                  bool cleanup);
  void DirectoryCreated(const base::FilePath& directory, bool cleanup);
  void DirectoryRemoved(const base::FilePath& directory);
  void EntryCopied(const base::FilePath& destination, bool cleanup);
  void EntryCopiedAndDeleted(const base::FilePath& source,
                             const base::FilePath& destination,
                             bool cleanup);

  // Helper functions to reverse each state transition in the undo stack.
  static void Undo(const MovedState& state);
  static void Undo(const CreatedState& state);
  static void Undo(const RemovedState& state);
  static void Undo(const CopiedState& state);
  static void Undo(const CopiedAndDeletedState& state);

  // Recursively deletes `path`.
  static void Cleanup(const base::FilePath& path);

  // A directory into which arbitrary files/directories will be placed as-needed
  // in support of Undo.
  const base::FilePath backup_;

  // The undo stack, holding state for operations that will be reversed in case
  // of Undo.
  std::stack<std::variant<MovedState,
                          CreatedState,
                          RemovedState,
                          CopiedState,
                          CopiedAndDeletedState>>
      undo_items_;

  // The cleanup stack, holding paths that will be deleted during the cleanup
  // phase at destruction.
  base::stack<base::FilePath> cleanup_paths_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_FILE_CONDUCTOR_H_
