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
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

using ::base::test::TestFuture;

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
constexpr char kDiskSizeHistogram[] = "GPU.ShaderCache.DiskSize.Persistent123";

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

class MockFactory : public PersistentCacheSandboxedFileFactory {
 public:
  MockFactory(const base::FilePath& cache_root_dir,
              scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : PersistentCacheSandboxedFileFactory(cache_root_dir,
                                            std::move(background_task_runner)) {
  }

  MOCK_METHOD((base::expected<persistent_cache::PendingBackend,
                              persistent_cache::TransactionError>),
              CreateFiles,
              (const CacheIdString& cache_id, const std::string& product),
              (override));

 private:
  friend class base::RefCountedThreadSafe<MockFactory>;
  ~MockFactory() override = default;
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

  // Calls CreateFilesAsync() for `kCacheId` and waits for the result. Returns
  // whether the files were created.
  bool CreateFilesAsyncAndWait(PersistentCacheSandboxedFileFactory* factory) {
    TestFuture<std::optional<persistent_cache::PendingBackend>> future;
    factory->CreateFilesAsync(kCacheId, kProduct, future.GetCallback());
    return future.Get().has_value();
  }

  // Calls RecordDiskSizeAsync() for `kCacheId` and waits for the size to be
  // recorded. The factory must use main_thread_task_runner().
  void RecordDiskSizeAndWait(PersistentCacheSandboxedFileFactory* factory) {
    factory->RecordDiskSizeAsync(kCacheId);
    FlushMainThreadTasks();
    base::ThreadPoolInstance::Get()->FlushForTesting();
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
  ASSERT_OK(factory_old->CreateFiles(kCacheId, kOldProduct));

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
  ASSERT_OK(factory_other->CreateFiles(kOtherCacheId, kOldProduct));
  ASSERT_EQ(1u, GetDirsInDir(other_cache_dir).size());

  // Wait for any async tasks to complete.
  FlushMainThreadTasks();

  // The old version directory for the first client should still exist.
  EXPECT_TRUE(base::PathExists(old_version_dir));

  // Now, create a factory for the new version. This should trigger the async
  // deletion of the stale files for kCacheId.
  auto factory_new = base::MakeRefCounted<TestFactory>(
      cache_root_path(), main_thread_task_runner());
  ASSERT_OK_AND_ASSIGN(auto files_new,
                       factory_new->CreateFiles(kCacheId, kNewProduct));

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
  ASSERT_OK(factory->CreateFiles(kCacheId, kProduct));

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
  ASSERT_OK(factory->CreateFiles(kCacheId, kProduct));

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

#if BUILDFLAG(IS_WIN)
TEST_F(PersistentCacheSandboxedFileFactoryTest, RecordsDiskSizeOncePerCacheId) {
  base::HistogramTester histogram_tester;
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);
  auto factory = base::MakeRefCounted<TestFactory>(cache_root_path(),
                                                   main_thread_task_runner());

  // Create the cache files from a previous session and add some data.
  ASSERT_OK(factory->CreateFiles(kCacheId, kProduct));
  FlushMainThreadTasks();
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());
  const base::FilePath version_dir = GetDirsInDir(cache_dir)[0];
  ASSERT_TRUE(base::WriteFile(version_dir.AppendASCII("data"),
                              std::string(64 * 1024, 'x')));
  const int64_t expected_size_kb =
      base::ComputeDirectorySize(version_dir) / 1024;
  ASSERT_GE(expected_size_kb, 64);

  // The synchronous CreateFiles() call doesn't record the size, and neither
  // does CreateFilesAsync(), since the cache might not be used.
  ASSERT_TRUE(CreateFilesAsyncAndWait(factory.get()));
  base::ThreadPoolInstance::Get()->FlushForTesting();
  histogram_tester.ExpectTotalCount(kDiskSizeHistogram, 0);

  RecordDiskSizeAndWait(factory.get());
  histogram_tester.ExpectUniqueSample(kDiskSizeHistogram, expected_size_kb, 1);

  // Using the cache again, e.g. after the GPU process restarts, doesn't record
  // the size again.
  ASSERT_TRUE(CreateFilesAsyncAndWait(factory.get()));
  RecordDiskSizeAndWait(factory.get());
  histogram_tester.ExpectUniqueSample(kDiskSizeHistogram, expected_size_kb, 1);
}

TEST_F(PersistentCacheSandboxedFileFactoryTest,
       DoesNotRecordDiskSizeForNewCache) {
  base::HistogramTester histogram_tester;
  auto factory = base::MakeRefCounted<TestFactory>(cache_root_path(),
                                                   main_thread_task_runner());

  // CreateFilesAsync() creates the database, so the cache is new.
  ASSERT_TRUE(CreateFilesAsyncAndWait(factory.get()));
  RecordDiskSizeAndWait(factory.get());
  histogram_tester.ExpectTotalCount(kDiskSizeHistogram, 0);

  // Using the cache again, e.g. after the GPU process restarts, doesn't record
  // the size either, even though the database exists by then.
  ASSERT_TRUE(CreateFilesAsyncAndWait(factory.get()));
  RecordDiskSizeAndWait(factory.get());
  histogram_tester.ExpectTotalCount(kDiskSizeHistogram, 0);
}
#else
TEST_F(PersistentCacheSandboxedFileFactoryTest, DoesNotRecordDiskSize) {
  base::HistogramTester histogram_tester;
  auto factory = base::MakeRefCounted<TestFactory>(cache_root_path(),
                                                   main_thread_task_runner());

  // Create the cache files from a previous session.
  ASSERT_OK(factory->CreateFiles(kCacheId, kProduct));
  FlushMainThreadTasks();

  ASSERT_TRUE(CreateFilesAsyncAndWait(factory.get()));
  RecordDiskSizeAndWait(factory.get());

  // The sizes are only recorded on Windows.
  histogram_tester.ExpectTotalCount(kDiskSizeHistogram, 0);
}
#endif  // BUILDFLAG(IS_WIN)

// Tests for async retry logic.
using PersistentCacheSandboxedFileFactoryAsyncTest =
    PersistentCacheSandboxedFileFactoryTest;

TEST_F(PersistentCacheSandboxedFileFactoryAsyncTest, SuccessOnFirstTry) {
  auto factory = base::MakeRefCounted<MockFactory>(
      cache_root_path(), base::SingleThreadTaskRunner::GetCurrentDefault());

  EXPECT_CALL(*factory, CreateFiles(kCacheId, kProduct))
      .WillOnce([]() {
        return base::expected<persistent_cache::PendingBackend,
                              persistent_cache::TransactionError>(
            persistent_cache::PendingBackend());
      });

  TestFuture<std::optional<persistent_cache::PendingBackend>> future;
  factory->CreateFilesAsync(
      kCacheId, kProduct, future.GetCallback(),
      /*extra_opts=*/{.retries = 3, .retry_delay = base::TimeDelta()});

  EXPECT_NE(future.Get(), std::nullopt);
}

TEST_F(PersistentCacheSandboxedFileFactoryAsyncTest, RetryOnTransientError) {
  auto factory = base::MakeRefCounted<MockFactory>(
      cache_root_path(), base::SingleThreadTaskRunner::GetCurrentDefault());

  // Expect 1 transient failure, then success.
  testing::InSequence s;
  EXPECT_CALL(*factory, CreateFiles(kCacheId, kProduct))
      .WillOnce([]() {
        return base::expected<persistent_cache::PendingBackend,
                              persistent_cache::TransactionError>(
            base::unexpected(persistent_cache::TransactionError::kTransient));
      });
  EXPECT_CALL(*factory, CreateFiles(kCacheId, kProduct))
      .WillOnce([]() {
        return base::expected<persistent_cache::PendingBackend,
                              persistent_cache::TransactionError>(
            persistent_cache::PendingBackend());
      });

  TestFuture<std::optional<persistent_cache::PendingBackend>> future;
  factory->CreateFilesAsync(
      kCacheId, kProduct, future.GetCallback(),
      /*extra_opts=*/{.retries = 3, .retry_delay = base::TimeDelta()});

  EXPECT_NE(future.Get(), std::nullopt);
}

TEST_F(PersistentCacheSandboxedFileFactoryAsyncTest,
       ExhaustRetriesOnTransientError) {
  auto factory = base::MakeRefCounted<MockFactory>(
      cache_root_path(), base::SingleThreadTaskRunner::GetCurrentDefault());

  // Expect 4 transient failures (1 initial + 3 retries).
  EXPECT_CALL(*factory, CreateFiles(kCacheId, kProduct))
      .Times(4)
      .WillRepeatedly([]() {
        return base::expected<persistent_cache::PendingBackend,
                              persistent_cache::TransactionError>(
            base::unexpected(persistent_cache::TransactionError::kTransient));
      });

  TestFuture<std::optional<persistent_cache::PendingBackend>> future;
  factory->CreateFilesAsync(
      kCacheId, kProduct, future.GetCallback(),
      /*extra_opts=*/{.retries = 3, .retry_delay = base::TimeDelta()});

  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(PersistentCacheSandboxedFileFactoryAsyncTest,
       ImmediateFailureOnPermanentError) {
  auto factory = base::MakeRefCounted<MockFactory>(
      cache_root_path(), base::SingleThreadTaskRunner::GetCurrentDefault());

  // Expect 1 permanent failure, no retries.
  EXPECT_CALL(*factory, CreateFiles(kCacheId, kProduct))
      .WillOnce([]() {
        return base::expected<persistent_cache::PendingBackend,
                              persistent_cache::TransactionError>(
            base::unexpected(persistent_cache::TransactionError::kPermanent));
      });

  TestFuture<std::optional<persistent_cache::PendingBackend>> future;
  factory->CreateFilesAsync(
      kCacheId, kProduct, future.GetCallback(),
      /*extra_opts=*/{.retries = 3, .retry_delay = base::TimeDelta()});

  EXPECT_EQ(future.Get(), std::nullopt);
}

}  // namespace viz
