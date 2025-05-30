// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SANDBOXED_VFS_DELEGATE_H_
#define COMPONENTS_SERVICES_STORAGE_SANDBOXED_VFS_DELEGATE_H_

#include <memory>

#include "sql/sandboxed_vfs.h"
#include "sql/sandboxed_vfs_file.h"

namespace storage {

class FilesystemProxy;

class SandboxedVfsDelegate : public sql::SandboxedVfs::Delegate {
 public:
  explicit SandboxedVfsDelegate(std::unique_ptr<FilesystemProxy> filesystem);
  ~SandboxedVfsDelegate() override;

  // sql::SandboxedVfs::Delegate implementation:
  sql::SandboxedVfsFile* RetrieveSandboxedVfsFile(
      base::File file,
      base::FilePath file_path,
      sql::SandboxedVfsFileType file_type,
      sql::SandboxedVfs* vfs) override;

  base::File OpenFile(const base::FilePath& file_path,
                      int sqlite_requested_flags) override;
  std::optional<sql::SandboxedVfs::PathAccessInfo> GetPathAccess(
      const base::FilePath& file_path) override;
  int DeleteFile(const base::FilePath& file_path, bool sync_dir) override;

 private:
  const std::unique_ptr<FilesystemProxy> filesystem_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SANDBOXED_VFS_DELEGATE_H_
