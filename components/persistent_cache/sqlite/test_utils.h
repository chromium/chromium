// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_TEST_UTILS_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/sqlite/vfs/sqlite_database_vfs_file_set.h"

namespace persistent_cache {

class Backend;

namespace test_utils {

// Use TestHelper from tests to generate valid initialization
// structures for backends and PersistentCache. This class takes care of owning
// the backing files and the directories they live in. They are deleted on
// destruction.
//
// Example:
//    TestHelper provider;
//    BackendParams backend_params =
//      provider.CreateBackendFilesAndBuildParams(BackendType::kSqlite);
//
//    auto PersistentCache persistent_cache =
//      PersistentCache::Open(backend_params);
//
class TestHelper {
 public:
  TestHelper();
  ~TestHelper();

  // Non-copyable and non-moveable since it's a test support class to be used in
  // fixtures.
  TestHelper(const TestHelper&) = delete;
  TestHelper(TestHelper&&) = delete;
  TestHelper& operator=(const TestHelper&) = delete;
  TestHelper& operator=(TestHelper&&) = delete;

  std::optional<SqliteVfsFileSet> CreateFilesAndBuildVfsFileSet();
  std::unique_ptr<Backend> CreateBackendWithFiles(BackendType type);

 private:
  base::FilePath CreateTemporaryDir();

  std::vector<base::ScopedTempDir> scoped_temp_dirs_;
};

}  // namespace test_utils

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_TEST_UTILS_H_
