// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/persistent_cache_sandboxed_file_factory.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

// RAII class to save and restore the current directory.
class ScopedCurrentDirectory {
 public:
  explicit ScopedCurrentDirectory(const base::FilePath& new_current_dir) {
    CHECK(base::GetCurrentDirectory(&original_cwd_));
    CHECK(base::SetCurrentDirectory(new_current_dir));
  }

  ~ScopedCurrentDirectory() { CHECK(base::SetCurrentDirectory(original_cwd_)); }

 private:
  base::FilePath original_cwd_;
};

const base::FilePath::StringType kCacheId = FILE_PATH_LITERAL("123");
const std::string kProduct = "Chrome/1.0.0.0";

std::vector<base::FilePath> GetDirsInDir(const base::FilePath& dir) {
  std::vector<base::FilePath> dirs;
  base::FileEnumerator e(dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    dirs.push_back(name);
  }
  return dirs;
}

// Derive PersistentCacheSandboxedFileFactory and make ctor public.
class TestFactory : public PersistentCacheSandboxedFileFactory {
 public:
  TestFactory(const base::FilePath& cache_root_dir,
              scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : PersistentCacheSandboxedFileFactory(cache_root_dir,
                                            std::move(background_task_runner)) {
  }

 private:
  friend class base::RefCountedThreadSafe<TestFactory>;

  ~TestFactory() override = default;
};

}  // namespace

class PersistentCacheSandboxedFileFactoryTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& cache_root_path() const { return temp_dir_.GetPath(); }

 protected:
  void FlushMainThreadTasks() {
    base::RunLoop run_loop;
    main_thread_task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner() {
    CHECK(base::SingleThreadTaskRunner::GetCurrentDefault());
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  base::ScopedTempDir temp_dir_;
};

// Test that PersistentCacheSandboxedFileFactory auto delete old cache files
// after a version change.
TEST_F(PersistentCacheSandboxedFileFactoryTest, DeletesStaleFiles) {
  const std::string kOldProduct = kProduct;
  const std::string kNewProduct = "Chrome/2.0.0.0";
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);

  // Create old, stale cache files.
  auto factory_old = base::MakeRefCounted<TestFactory>(
      cache_root_path(), main_thread_task_runner());
  auto files_old = factory_old->CreateFiles(kCacheId, kOldProduct);
  ASSERT_TRUE(files_old);
  files_old.reset();  // Close the files.

  // There should be one version directory for the old product.
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());
  const base::FilePath old_version_dir = GetDirsInDir(cache_dir)[0];
  EXPECT_TRUE(base::PathExists(old_version_dir));

  // Create a file for a different client ID that should not be deleted.
  const base::FilePath::StringType kOtherCacheId = FILE_PATH_LITERAL("456");
  const base::FilePath other_cache_dir =
      cache_root_path().Append(kOtherCacheId);
  auto factory_other = base::MakeRefCounted<TestFactory>(
      cache_root_path(), main_thread_task_runner());
  auto files_other = factory_other->CreateFiles(kOtherCacheId, kOldProduct);
  ASSERT_TRUE(files_other);
  files_other.reset();  // Close the file.
  ASSERT_EQ(1u, GetDirsInDir(other_cache_dir).size());

  // Wait for any async tasks to complete.
  FlushMainThreadTasks();

  // The old version directory for the first client should still exist.
  EXPECT_TRUE(base::PathExists(old_version_dir));

  // Now, create a factory for the new version. This should trigger the async
  // deletion of the stale files for kCacheId.
  auto factory_new = base::MakeRefCounted<TestFactory>(
      cache_root_path(), main_thread_task_runner());
  auto files_new = factory_new->CreateFiles(kCacheId, kNewProduct);
  ASSERT_TRUE(files_new);

  // Wait for the async deletion task to complete.
  FlushMainThreadTasks();

  // Verify the stale version for kCacheId is gone.
  EXPECT_FALSE(base::PathExists(old_version_dir));
  EXPECT_EQ(1u, GetDirsInDir(cache_dir).size());

  // Verify the other client's directory is untouched.
  EXPECT_EQ(1u, GetDirsInDir(other_cache_dir).size());

  // Verify the root cache directory still contains two client directories.
  EXPECT_EQ(2u, GetDirsInDir(cache_root_path()).size());
}

TEST_F(PersistentCacheSandboxedFileFactoryTest, ClearFiles) {
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);

  // Create cache files.
  auto factory = base::MakeRefCounted<TestFactory>(cache_root_path(),
                                                   main_thread_task_runner());
  auto files = factory->CreateFiles(kCacheId, kProduct);
  ASSERT_TRUE(files);
  files.reset();  // Close the files.

  // Verify a version directory exists.
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());

  // Clear the files.
  EXPECT_TRUE(factory->ClearFiles(kCacheId, kProduct));

  // Verify the version directory is gone.
  EXPECT_EQ(0u, GetDirsInDir(cache_dir).size());
}

TEST_F(PersistentCacheSandboxedFileFactoryTest, ClearFilesAsync) {
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);

  // Create cache files.
  auto factory = base::MakeRefCounted<TestFactory>(cache_root_path(),
                                                   main_thread_task_runner());
  auto files = factory->CreateFiles(kCacheId, kProduct);
  ASSERT_TRUE(files);
  files.reset();  // Close the files.

  // Verify a version directory exists.
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());

  // Clear the files asynchronously.
  bool callback_result = false;
  base::RunLoop run_loop;
  factory->ClearFilesAsync(
      kCacheId, kProduct,
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &callback_result)
          .Then(run_loop.QuitClosure()));

  // Wait for the async deletion task to complete.
  run_loop.Run();

  EXPECT_TRUE(callback_result);

  // Verify the version directory is gone.
  EXPECT_EQ(0u, GetDirsInDir(cache_dir).size());
}

}  // namespace viz
