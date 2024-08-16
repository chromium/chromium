// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILESYSTEM_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILESYSTEM_IMPL_H_

#include <vector>

#include "base/component_export.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"

namespace storage {

class FilesystemProxy;

// This is a concrete implementation of the |storage.mojom.Directory| interface
// consumed via FilesystemProxy. If running the Storage Service in a restricted
// sandbox environment, a privileged client (i.e. the browser) must provide a
// working remote Directory implementation. This implementation can generally be
// used as-is in such cases.
//
// Note that this must be constructed on a thread that allows blocking file I/O
// operations.
class COMPONENT_EXPORT(STORAGE_SERVICE_FILESYSTEM_SUPPORT) FilesystemImpl
    : public mojom::Directory {
 public:
  enum class ClientType {
    // No additional restrictions are placed on file objects returned by this
    // implementation.
    kTrusted,

    // The client is not trusted to handle certain types of file objects,
    // so this object must not return such objects to the client. In
    // particular, such clients are not trusted with file handles that
    // are both executable and writable. See
    // base::File::AddFlagsForPassingToUntrustedProcess.
    kUntrusted,
  };

  // |root| must be an absolute path. Operations performed by this object
  // will be contained within |root| or a transitive subdirectory thereof. All
  // relative paths given to methods of this object are interpreted as relative
  // to |root|.
  // |client_type| specifies the level of trust the client has in handling
  // certain types of file objects.
  FilesystemImpl(const base::FilePath& root, ClientType client_type);

  FilesystemImpl(const FilesystemImpl&) = delete;
  FilesystemImpl& operator=(const FilesystemImpl) = delete;
  ~FilesystemImpl() override;

  // mojom::Directory:
  void Clone(mojo::PendingReceiver<mojom::Directory> receiver) override;
  void PathExists(const base::FilePath& path,
                  PathExistsCallback callback) override;
  void GetEntries(const base::FilePath& path,
                  mojom::GetEntriesMode mode,
                  GetEntriesCallback callback) override;
  void OpenFile(const base::FilePath& path,
                mojom::FileOpenMode mode,
                mojom::FileReadAccess read_access,
                mojom::FileWriteAccess write_access,
                OpenFileCallback callback) override;
  void CreateDirectory(const base::FilePath& path,
                       CreateDirectoryCallback callback) override;
  void DeleteFile(const base::FilePath& path,
                  DeleteFileCallback callback) override;
  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override;
  void GetPathAccess(const base::FilePath& path,
                     GetPathAccessCallback callback) override;
  void RenameFile(const base::FilePath& old_path,
                  const base::FilePath& new_path,
                  RenameFileCallback callback) override;
  void LockFile(const base::FilePath& path, LockFileCallback callback) override;

  // Helper used by `LockFile()` and `FilesystemProxy::LockFile()` for in
  // unrestricted mode. See `FileSystemProxy::LockFile()` for
  // `same_process_failure` docs.
  static base::FileErrorOr<base::File> LockFileLocal(
      const base::FilePath& path,
      bool* same_process_failure);
  static void UnlockFileLocal(const base::FilePath& path);

  // Helper used by GetPathAccess() and FilesystemProxy::GetPathAccess.
  static mojom::PathAccessInfoPtr GetPathAccessLocal(
      const base::FilePath& path);

  // Helper used by GetEntries() and FilesystemProxy::GetDirectoryEntries.
  static base::FileErrorOr<std::vector<base::FilePath>> GetDirectoryEntries(
      const base::FilePath& path,
      mojom::GetEntriesMode mode);

 private:
  base::FilePath MakeAbsolute(const base::FilePath& path) const;

  const base::FilePath root_;
  const ClientType client_type_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_FILESYSTEM_FILESYSTEM_IMPL_H_
