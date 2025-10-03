// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/test_utils.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

namespace persistent_cache::test_utils {

TestHelper::TestHelper() = default;
TestHelper::~TestHelper() = default;

std::optional<SqliteVfsFileSet> TestHelper::CreateFilesAndBuildVfsFileSet() {
  base::FilePath temporary_subdir = CreateTemporaryDir();

  // Note: Specifically give nonsensical names to the files here to examplify
  // that using a vfs allows for their use not through their actual names.
  return SqliteVfsFileSet::Create(temporary_subdir.AppendASCII("FIRST"),
                                  temporary_subdir.AppendASCII("SECOND"));
}

std::unique_ptr<Backend> TestHelper::CreateBackendWithFiles(BackendType type) {
  switch (type) {
    case BackendType::kMock:
      return nullptr;

    case BackendType::kSqlite:
      if (auto file_set = CreateFilesAndBuildVfsFileSet(); file_set) {
        return std::make_unique<SqliteBackendImpl>(*std::move(file_set));
      }
      return nullptr;
  }
}

base::FilePath TestHelper::CreateTemporaryDir() {
  scoped_temp_dirs_.emplace_back();
  CHECK(scoped_temp_dirs_.back().CreateUniqueTempDir());
  return scoped_temp_dirs_.back().GetPath();
}

}  // namespace persistent_cache::test_utils
