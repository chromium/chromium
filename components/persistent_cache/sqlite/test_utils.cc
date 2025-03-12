// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/test_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

namespace persistent_cache::test_utils {

TestHelper::TestHelper() = default;
TestHelper::~TestHelper() = default;

base::File TestHelper::CreateFile(base::FilePath directory,
                                  const std::string& file_name) {
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE;
  return base::File(directory.AppendASCII(file_name), flags);
}

BackendParams TestHelper::CreateBackendFilesAndBuildParams(BackendType type) {
  base::FilePath temporary_subdir = CreateTemporaryDir();
  BackendParams backend_params;
  backend_params.db_file = CreateFile(temporary_subdir, "FIRST");
  backend_params.db_file_is_writable = true;
  backend_params.journal_file = CreateFile(temporary_subdir, "SECOND");
  backend_params.journal_file_is_writable = true;
  backend_params.type = type;
  return backend_params;
}

SqliteVfsFileSet TestHelper::CreateFilesAndBuildVfsFileSet() {
  base::FilePath temporary_subdir = CreateTemporaryDir();

  // Note: Specifically give nonsensical names to the files here to examplify
  // that using a vfs allows for their use not through their actual names.
  base::File db_file = CreateFile(temporary_subdir, "FIRST");
  base::File journal_file = CreateFile(temporary_subdir, "SECOND");

  return SqliteVfsFileSet(
      SandboxedFile(std::move(db_file),
                    SandboxedFile::AccessRights::kReadWrite),
      SandboxedFile(std::move(journal_file),
                    SandboxedFile::AccessRights::kReadWrite));
}

base::FilePath TestHelper::CreateTemporaryDir() {
  scoped_temp_dirs_.emplace_back();
  CHECK(scoped_temp_dirs_.back().CreateUniqueTempDir());
  return scoped_temp_dirs_.back().GetPath();
}

}  // namespace persistent_cache::test_utils
