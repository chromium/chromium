// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILESYSTEM_PROXY_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILESYSTEM_PROXY_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace storage {

// A FilesystemProxy performs common filesystem operations on the caller's
// behalf, either directly (if the calling process environment permits) or via a
// remote storage.mojom.Directory implementation if one is provided.
//
// This class is thread-safe, but must only be called from threads that allow
// blocking operations.
class COMPONENT_EXPORT(STORAGE_SERVICE_FILESYSTEM_SUPPORT) FilesystemProxy {
 public:
  // Tags used to clarify the meaning of each constructor so that call sites can
  // be more easily evaluated.
  enum { UNRESTRICTED };
  enum { RESTRICTED };

  // Constructs a new FilesystemProxy which performs privileged, in-process
  // file system operations. Not suitable for use within a restricted sandbox
  // environment, as it may ultimately attempt to work that will be blocked by
  // the sandbox boundary.
  //
  // If relative paths are given to methods on this object, they are interpreted
  // relative to |root|. Objects constructed in this way can process paths
  // outside of |root| and in general suffer no restrictions on what paths are
  // allowed.
  //
  // If |root| is empty, relative paths are used as-is and effectively
  // interpreted as relative to the current working directory.
  explicit FilesystemProxy(decltype(UNRESTRICTED), const base::FilePath& root);

  // Constructs a new FilesystemProxy for |root|, using |directory| to invoke
  // privileged operations. Suitable for use within a sandboxed/ environment,
  // assuming |directory| is connected to a remote implementation in a more
  // privileged process.
  //
  // Objects constructed in this way can ONLY access contents within |root|;
  // relative paths are interpreted as relative to |root|, and absolute paths
  // must fall within |root|.
  //
  // |ipc_task_runner| is a task runner suitable for internally binding the
  // Directory IPC endpoint; this is the task runner used to bind the internal
  // SharedRemote, and so this is where all outgoing IPCs will hop before being
  // transmitted. As such it must be a thread which is never blocked. Typically
  // in a Content process environment, this should be the "IO thread".
  FilesystemProxy(decltype(RESTRICTED),
                  const base::FilePath& root,
                  mojo::PendingRemote<mojom::Directory> directory,
                  scoped_refptr<base::SequencedTaskRunner> ipc_task_runner);

  FilesystemProxy(const FilesystemProxy&) = delete;
  FilesystemProxy& operator=(const FilesystemProxy&) = delete;
  ~FilesystemProxy();

  // Returns true iff |path| refers to an existing file or directory.
  bool PathExists(const base::FilePath& path);

  // Enumerates all files and/or directories within |path|, a directory. Not
  // recursive.
  enum class DirectoryEntryType {
    kFilesOnly,
    kFilesAndDirectories,
  };
  base::FileErrorOr<std::vector<base::FilePath>> GetDirectoryEntries(
      const base::FilePath& path,
      DirectoryEntryType type);

  // Opens a file at |path| with the given |flags|. If successful, the newly
  // opened file is returned. |flags| may be any bitwise union of
  // base::File::Flags values.
  base::FileErrorOr<base::File> OpenFile(const base::FilePath& path, int flags);

  // Creates a new directory at |path|. Any needed parent directories above
  // |path| are also created if they don't already exist.
  base::File::Error CreateDirectory(const base::FilePath& path);

  // Deletes the file or directory at |path| if it exists and returns true iff
  // successful.  Not recursive.  Will fail if there are subdirectories.  This
  // will return true if |path| does not exist.
  bool DeleteFile(const base::FilePath& path);

  // Retrieves information about a file or directory at |path|. Returns a valid
  // base::File::Info value on success, or null on failure.
  std::optional<base::File::Info> GetFileInfo(const base::FilePath& path);

  // Retrieves information about access rights for a path in the filesystem.
  // Returns a valid PathAccessInfo on success, or null on failure.
  struct PathAccessInfo {
    bool can_read = false;
    bool can_write = false;
  };
  std::optional<PathAccessInfo> GetPathAccess(const base::FilePath& path);

  // Renames a file from |old_path| to |new_path|. Must be atomic.
  base::File::Error RenameFile(const base::FilePath& old_path,
                               const base::FilePath& new_path);

  // Acquires an exclusive lock on the file at |path| if possible, returning a
  // FileLock object to hold the lock if successful. The lock remains held as
  // long as the returned FileLock object remains alive. Destroying the FileLock
  // releases the lock.
  class FileLock {
   public:
    virtual ~FileLock() = default;

    // Explicitly releases the lock. This only has side effects the first time
    // it's called, and once this is called, FileLock destruction also will be a
    // no-op.
    virtual base::File::Error Release() = 0;
  };
  // `same_process_failure`, if non-null, will be set to true iff acquiring the
  // lock failed due to the lookup in `LockTable()`. TODO(crbug.com/340398745):
  // remove this parameter.
  base::FileErrorOr<std::unique_ptr<FileLock>> LockFile(
      const base::FilePath& path,
      bool* same_process_failure = nullptr);

 private:
  // For restricted FilesystemProxy instances, this returns a FilePath
  // equivalent to |path| which is strictly relative to |root_|. It is an error
  // to call with a |path| for which this is impossible.
  //
  // Not called by unrestricted FilesystemProxy instances.
  base::FilePath MakeRelative(const base::FilePath& path) const;

  // For unrestricted FilesystemProxy instances with a non-empty root, this
  // returns a FilePath that is always absolute. If |path| is absolute, it is
  // returned unmodified. If |path| is relative AND |root_| is non-empty, the
  // path is resolved against |root_| and the resulting absolute path is
  // returned. Finally, if |path| is relative and |root_| is empty, this returns
  // |path| unmodified.
  base::FilePath MaybeMakeAbsolute(const base::FilePath& path) const;

  const base::FilePath root_;

  // If |remote_directory_| is set this is a restricted proxy, otherwise
  // it is unrestricted and will perform filesystem operations directly.
  const mojo::SharedRemote<mojom::Directory> remote_directory_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILESYSTEM_PROXY_H_
