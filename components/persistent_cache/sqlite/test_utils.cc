// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/sqlite/test_utils.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"
#include "sql/sandboxed_vfs_file.h"

namespace {

base::File CreateFile(const base::FilePath& file_path) {
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE;
  return base::File(file_path, flags);
}

}  // namespace

namespace persistent_cache::test_utils {

TestHelper::TestHelper() = default;
TestHelper::~TestHelper() = default;

BackendParams TestHelper::CreateBackendFilesAndBuildParams(BackendType type) {
  base::FilePath temporary_subdir = CreateTemporaryDir();
  BackendParams backend_params;
  backend_params.db_file_path = temporary_subdir.AppendASCII("FIRST");
  backend_params.db_file = CreateFile(backend_params.db_file_path);
  backend_params.db_file_is_writable = true;
  backend_params.journal_file_path = temporary_subdir.AppendASCII("SECOND");
  backend_params.journal_file = CreateFile(backend_params.journal_file_path);
  backend_params.journal_file_is_writable = true;
  backend_params.shared_lock =
      base::UnsafeSharedMemoryRegion::Create(sizeof(LockState));
  backend_params.type = type;
  return backend_params;
}

SqliteVfsFileSet TestHelper::CreateFilesAndBuildVfsFileSet() {
  base::FilePath temporary_subdir = CreateTemporaryDir();

  base::UnsafeSharedMemoryRegion shared_lock =
      base::UnsafeSharedMemoryRegion::Create(sizeof(LockState));
  base::WritableSharedMemoryMapping mapped_shared_lock = shared_lock.Map();

  // Note: Specifically give nonsensical names to the files here to examplify
  // that using a vfs allows for their use not through their actual names.
  base::FilePath db_file_path = temporary_subdir.AppendASCII("FIRST");
  base::File db_file = CreateFile(db_file_path);
  base::FilePath journal_file_path = temporary_subdir.AppendASCII("SECOND");
  base::File journal_file = CreateFile(journal_file_path);

  return SqliteVfsFileSet(
      std::make_unique<SandboxedFile>(std::move(db_file),
                                      std::move(db_file_path),
                                      SandboxedFile::AccessRights::kReadWrite,
                                      std::move(mapped_shared_lock)),
      std::make_unique<SandboxedFile>(std::move(journal_file),
                                      std::move(journal_file_path),
                                      SandboxedFile::AccessRights::kReadWrite),
      std::move(shared_lock));
}

base::FilePath TestHelper::CreateTemporaryDir() {
  scoped_temp_dirs_.emplace_back();
  CHECK(scoped_temp_dirs_.back().CreateUniqueTempDir());
  return scoped_temp_dirs_.back().GetPath();
}

}  // namespace persistent_cache::test_utils
