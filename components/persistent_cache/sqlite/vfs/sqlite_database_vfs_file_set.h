// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_DATABASE_VFS_FILE_SET_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_DATABASE_VFS_FILE_SET_H_

#include <array>
#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

namespace persistent_cache {

// Contains `SanboxedFile` representations of the files necessary to the use of
// an `sql::Database`.
//
// This class owns the `SandboxedFile` files and must outlive any use of them.
class COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteVfsFileSet {
 public:
  SqliteVfsFileSet(std::unique_ptr<SandboxedFile> db_file,
                   std::unique_ptr<SandboxedFile> journal_file);
  SqliteVfsFileSet(SqliteVfsFileSet& other) = delete;
  SqliteVfsFileSet& operator=(const SqliteVfsFileSet& other) = delete;
  SqliteVfsFileSet(SqliteVfsFileSet&& other);
  SqliteVfsFileSet& operator=(SqliteVfsFileSet&& other);
  ~SqliteVfsFileSet();

  // Returns sandboxed files along with the virtual file paths through which
  // `SqliteSandboxedVfsDelegate` will expose them to `sql::Database`.
  std::array<std::pair<base::FilePath, raw_ptr<SandboxedFile>>, 2> GetFiles()
      const;

  // Generates a valid name that can be passed to `sql::database`'s constructor.
  base::FilePath GetDbVirtualFilePath() const;

 private:
  base::FilePath GetJournalVirtualFilePath() const;

  std::unique_ptr<SandboxedFile> db_file_;
  std::unique_ptr<SandboxedFile> journal_file_;

  // SQLite databases use standard naming for their files. Since the vfs might
  // register files for many databases at once it needs some way to
  // differentiate them. This is guaranteed to be unique because it is based on
  // a monotonically increasing integer.
  std::string virtual_fs_path_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SQLITE_DATABASE_VFS_FILE_SET_H_
