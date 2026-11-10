// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_data_store_disk.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

using base::ScopedTempDir;
using testing::Mock;

namespace image_fetcher {

namespace {
constexpr char kImageKey[] = "key";
constexpr char kImageData[] = "data";
constexpr char kCacheResultHistogram[] = "ImageFetcher.CacheResult";
}  // namespace

class CachedImageFetcherImageDataStoreDiskTest : public testing::Test {
 public:
  CachedImageFetcherImageDataStoreDiskTest() = default;

  CachedImageFetcherImageDataStoreDiskTest(
      const CachedImageFetcherImageDataStoreDiskTest&) = delete;
  CachedImageFetcherImageDataStoreDiskTest& operator=(
      const CachedImageFetcherImageDataStoreDiskTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateDataStore() {
    data_store_.reset();
    data_store_ = std::make_unique<ImageDataStoreDisk>(
        temp_dir_.GetPath(), base::SequencedTaskRunner::GetCurrentDefault());
  }

  void InitializeDataStore() {
    EXPECT_CALL(*this, OnInitialized());
    data_store()->Initialize(
        base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::OnInitialized,
                       base::Unretained(this)));
    RunUntilIdle();
  }

  void PrepareDataStore(bool initialize, bool setup_for_needs_transcoding) {
    CreateDataStore();
    InitializeDataStore();
    SaveData(kImageKey, setup_for_needs_transcoding);

    if (!initialize) {
      CreateDataStore();
    }
  }

  void AssertDataPresent(const std::string& key, bool needs_transcoding) {
    AssertDataPresent(key, kImageData, needs_transcoding);
  }

  void AssertDataPresent(const std::string& key,
                         const std::string& data,
                         bool needs_transcoding) {
    EXPECT_CALL(*this, DataCallback(needs_transcoding, data));
    data_store()->LoadImage(
        key, needs_transcoding,
        base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::DataCallback,
                       base::Unretained(this)));
    RunUntilIdle();
  }

  void SaveData(const std::string& key, bool needs_transcoding) {
    data_store()->SaveImage(key, kImageData, needs_transcoding);
    RunUntilIdle();
  }

  void DeleteData(const std::string& key) {
    data_store()->DeleteImage(key);
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }
  ImageDataStore* data_store() { return data_store_.get(); }
  const base::FilePath& temp_dir_path() const { return temp_dir_.GetPath(); }
  base::FilePath storage_path() const {
    return temp_dir_.GetPath().AppendASCII("image_data_storage");
  }

  MOCK_METHOD(void, OnInitialized, (), ());
  MOCK_METHOD(void, DataCallback, (bool, std::string), ());
  MOCK_METHOD(void,
              KeysCallback,
              (std::optional<std::vector<std::string>>),
              ());

 private:
  std::unique_ptr<ImageDataStoreDisk> data_store_;

  ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(CachedImageFetcherImageDataStoreDiskTest, SanityTest) {
  CreateDataStore();
  InitializeDataStore();

  SaveData(kImageKey, /* needs_transcoding */ false);
  AssertDataPresent(kImageKey, /* needs_transcoding */ false);

  DeleteData(kImageKey);
  AssertDataPresent(kImageKey, "", /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, Init) {
  CreateDataStore();
  ASSERT_FALSE(data_store()->IsInitialized());

  InitializeDataStore();
  ASSERT_TRUE(data_store()->IsInitialized());
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, InitWithExistingDirectory) {
  PrepareDataStore(/* initialize */ true, /* needs_transcoding */ false);

  // Recreating the data store shouldn't wipe the directory.
  CreateDataStore();
  InitializeDataStore();

  AssertDataPresent(kImageKey, /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, SaveBeforeInit) {
  PrepareDataStore(/* initialize */ false, /* needs_transcoding */ false);
  // No data should be present (empty string).
  AssertDataPresent(kImageKey, "", /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, Save) {
  PrepareDataStore(/* initialize */ true, /* needs_transcoding */ false);
  AssertDataPresent(kImageKey, /* needs_transcoding */ false);

  PrepareDataStore(/* initialize */ true, /* needs_transcoding */ true);
  AssertDataPresent(kImageKey, /* needs_transcoding */ true);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, DeleteBeforeInit) {
  PrepareDataStore(/* initialize */ false, /* needs_transcoding */ false);

  DeleteData(kImageKey);

  InitializeDataStore();
  // Delete should have failed, data still there.
  AssertDataPresent(kImageKey, /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, Delete) {
  PrepareDataStore(/* initialize */ true, /* needs_transcoding */ false);

  DeleteData(kImageKey);

  // Should be empty string (not present).
  AssertDataPresent(kImageKey, "", /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, RejectsInvalidKeys) {
  CreateDataStore();
  InitializeDataStore();

  const base::FilePath outside_file = temp_dir_path().AppendASCII("outside");
  ASSERT_TRUE(base::WriteFile(outside_file, "original"));
  auto expect_original_contents = [](const base::FilePath& path) {
    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, "original");
  };

  // Absolute and parent-relative keys must not modify or expose this file
  // outside the cache directory.
  const std::string absolute_key = outside_file.AsUTF8Unsafe();
  SaveData(absolute_key, /* needs_transcoding */ false);
  expect_original_contents(outside_file);
  AssertDataPresent(absolute_key, "", /* needs_transcoding */ false);
  DeleteData(absolute_key);
  expect_original_contents(outside_file);

  constexpr char kParentKey[] = "../outside";

  SaveData(kParentKey, /* needs_transcoding */ false);
  expect_original_contents(outside_file);
  SaveData(kParentKey, /* needs_transcoding */ true);
  expect_original_contents(outside_file);
  AssertDataPresent(kParentKey, "", /* needs_transcoding */ false);
  DeleteData(kParentKey);
  expect_original_contents(outside_file);

  const base::FilePath nested_dir = storage_path().AppendASCII("nested");
  const base::FilePath transcoding_dir =
      storage_path().AppendASCII("ntr_nested");
  ASSERT_TRUE(base::CreateDirectory(nested_dir));
  ASSERT_TRUE(base::CreateDirectory(transcoding_dir));
  constexpr char kNestedKey[] = "nested/file";
  SaveData(kNestedKey, /* needs_transcoding */ false);
  SaveData(kNestedKey, /* needs_transcoding */ true);
  EXPECT_FALSE(base::PathExists(nested_dir.AppendASCII("file")));
  EXPECT_FALSE(base::PathExists(transcoding_dir.AppendASCII("file")));

  // Seed files directly so loads and deletion do not depend on rejected saves.
  ASSERT_TRUE(base::WriteFile(nested_dir.AppendASCII("file"), "original"));
  ASSERT_TRUE(base::WriteFile(transcoding_dir.AppendASCII("file"), "original"));
  AssertDataPresent(kNestedKey, "", /* needs_transcoding */ false);
  AssertDataPresent(kNestedKey, "", /* needs_transcoding */ true);
  DeleteData(kNestedKey);
  expect_original_contents(nested_dir.AppendASCII("file"));
  expect_original_contents(transcoding_dir.AppendASCII("file"));

  // Invalid keys must also leave the cache directory and valid entries intact.
  SaveData(".", /* needs_transcoding */ false);
  EXPECT_TRUE(base::DirectoryExists(storage_path()));
  DeleteData(".");
  EXPECT_TRUE(base::DirectoryExists(storage_path()));

  SaveData(kImageKey, /* needs_transcoding */ false);
  DeleteData("*");
  AssertDataPresent(kImageKey, /* needs_transcoding */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, GetAllKeysBeforeInit) {
  PrepareDataStore(/* initialize */ false, /* needs_transcoding */ false);

  // Cannot return a key list before initialization.
  EXPECT_CALL(*this, KeysCallback(testing::Eq(std::nullopt)));
  data_store()->GetAllKeys(
      base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::KeysCallback,
                     base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, GetAllKeys) {
  PrepareDataStore(/* initialize */ true, /* needs_transcoding */ false);

  EXPECT_CALL(
      *this,
      KeysCallback(std::make_optional(std::vector<std::string>{kImageKey})));
  data_store()->GetAllKeys(
      base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::KeysCallback,
                     base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       QueuedLoadIsServedBeforeDelete) {
  CreateDataStore();
  InitializeDataStore();

  SaveData(kImageKey, /* needs_transcoding */ false);
  EXPECT_CALL(*this, DataCallback(false, kImageData));
  data_store()->LoadImage(
      kImageKey,
      /* needs_transcoding */ false,
      base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::DataCallback,
                     base::Unretained(this)));
  DeleteData(kImageKey);

  RunUntilIdle();
}

#if BUILDFLAG(IS_POSIX)
TEST_F(CachedImageFetcherImageDataStoreDiskTest, SaveDoesNotFollowSymlink) {
  CreateDataStore();
  InitializeDataStore();

  ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());

  base::FilePath storage_dir =
      temp_dir_path().Append(FILE_PATH_LITERAL("image_data_storage"));

  base::FilePath existing_target =
      external_dir.GetPath().Append(FILE_PATH_LITERAL("existing_target"));
  ASSERT_TRUE(base::WriteFile(existing_target, "original"));
  ASSERT_TRUE(base::CreateSymbolicLink(existing_target,
                                       storage_dir.AppendASCII(kImageKey)));

  SaveData(kImageKey, /* needs_transcoding= */ false);

  std::string target_contents;
  ASSERT_TRUE(base::ReadFileToString(existing_target, &target_contents));
  EXPECT_EQ("original", target_contents);

  constexpr char kSecondKey[] = "key2";
  base::FilePath non_existent_target =
      external_dir.GetPath().Append(FILE_PATH_LITERAL("non_existent_target"));
  ASSERT_TRUE(base::CreateSymbolicLink(non_existent_target,
                                       storage_dir.AppendASCII(kSecondKey)));

  SaveData(kSecondKey, /* needs_transcoding= */ false);
  EXPECT_FALSE(base::PathExists(non_existent_target));
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, LoadDoesNotFollowSymlink) {
  CreateDataStore();
  InitializeDataStore();

  ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());
  base::FilePath secret_file =
      external_dir.GetPath().Append(FILE_PATH_LITERAL("secret_file"));
  ASSERT_TRUE(base::WriteFile(secret_file, "secret"));

  base::FilePath symlink_path =
      temp_dir_path()
          .Append(FILE_PATH_LITERAL("image_data_storage"))
          .AppendASCII(kImageKey);
  ASSERT_TRUE(base::CreateSymbolicLink(secret_file, symlink_path));

  AssertDataPresent(kImageKey, "", /* needs_transcoding= */ false);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       RejectsSymlinkedStorageDirectory) {
  ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());
  base::FilePath external_existing =
      external_dir.GetPath().AppendASCII(kImageKey);
  ASSERT_TRUE(base::WriteFile(external_existing, "secret"));

  base::FilePath storage_dir =
      temp_dir_path().Append(FILE_PATH_LITERAL("image_data_storage"));
  ASSERT_TRUE(base::CreateSymbolicLink(external_dir.GetPath(), storage_dir));

  CreateDataStore();
  InitializeDataStore();
  EXPECT_FALSE(data_store()->IsInitialized());

  // Also verify SaveImage, LoadImage, GetAllKeys, and DeleteImage reject a
  // storage directory that is replaced with a symlink after initialization.
  ASSERT_TRUE(base::DeleteFile(storage_dir));
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(data_store()->IsInitialized());

  ASSERT_TRUE(base::DeletePathRecursively(storage_dir));
  ASSERT_TRUE(base::CreateSymbolicLink(external_dir.GetPath(), storage_dir));

  constexpr char kNewKey[] = "new_key";
  SaveData(kNewKey, /* needs_transcoding= */ false);
  EXPECT_FALSE(base::PathExists(external_dir.GetPath().AppendASCII(kNewKey)));

  AssertDataPresent(kImageKey, "", /* needs_transcoding= */ false);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, KeysCallback(testing::Eq(std::nullopt)));
  data_store()->GetAllKeys(
      base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::KeysCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  DeleteData(kImageKey);
  EXPECT_TRUE(base::PathExists(external_existing));
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(CachedImageFetcherImageDataStoreDiskTest, ReportsInitializationFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  ASSERT_TRUE(base::WriteFile(storage_path(), kImageData));

  InitializeDataStore();

  EXPECT_FALSE(data_store()->IsInitialized());
  histograms.ExpectBucketCount(
      kCacheResultHistogram,
      ImageFetcherCacheResult::kDataStoreInitializationFailed, 1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, ReportsWriteFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(base::CreateDirectory(storage_path().AppendASCII(kImageKey)));

  SaveData(kImageKey, /*needs_transcoding=*/false);

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreWriteFailed, 1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, ReportsReadFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(base::CreateDirectory(storage_path().AppendASCII(kImageKey)));

  AssertDataPresent(kImageKey, "", /*needs_transcoding=*/false);

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       ReportsMissingStorageDirectoryAsNotFound) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(base::DeleteFile(storage_path()));

  AssertDataPresent(kImageKey, "", /*needs_transcoding=*/false);

  histograms.ExpectBucketCount(kCacheResultHistogram,
                               ImageFetcherCacheResult::kDataStoreReadNotFound,
                               1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, ReportsDeleteFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  base::FilePath image_path = storage_path().AppendASCII(kImageKey);
  ASSERT_TRUE(base::CreateDirectory(image_path));
  ASSERT_TRUE(base::WriteFile(image_path.AppendASCII("child"), kImageData));

  DeleteData(kImageKey);

  histograms.ExpectBucketCount(kCacheResultHistogram,
                               ImageFetcherCacheResult::kDataStoreDeleteFailed,
                               1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, ReportsKeyEnumerationFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(base::DeleteFile(storage_path()));
  ASSERT_TRUE(base::WriteFile(storage_path(), kImageData));
  base::test::TestFuture<std::optional<std::vector<std::string>>> keys;
  data_store()->GetAllKeys(keys.GetCallback());
  EXPECT_FALSE(keys.Take().has_value());

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       ReportsMissingStorageDirectoryDuringEnumeration) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(base::DeleteFile(storage_path()));

  base::test::TestFuture<std::optional<std::vector<std::string>>> keys;
  data_store()->GetAllKeys(keys.GetCallback());
  EXPECT_FALSE(keys.Take().has_value());

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       ReportsEmptyDirectoryEnumerationSuccess) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();

  base::test::TestFuture<std::optional<std::vector<std::string>>> keys;
  data_store()->GetAllKeys(keys.GetCallback());
  ASSERT_TRUE(keys.Get().has_value());
  EXPECT_TRUE(keys.Take()->empty());

  histograms.ExpectBucketCount(kCacheResultHistogram,
                               ImageFetcherCacheResult::kDataStoreReadSucceeded,
                               1);
  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 0);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       ReportsUntranscodedFileCleanupFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  base::FilePath untranscoded_path = storage_path().AppendASCII("ntr_key");
  ASSERT_TRUE(base::CreateDirectory(untranscoded_path));
  ASSERT_TRUE(
      base::WriteFile(untranscoded_path.AppendASCII("child"), kImageData));

  SaveData(kImageKey, /*needs_transcoding=*/false);

  AssertDataPresent(kImageKey, /*needs_transcoding=*/false);
  histograms.ExpectBucketCount(kCacheResultHistogram,
                               ImageFetcherCacheResult::kDataStoreDeleteFailed,
                               1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       ReportsUntranscodedFileDeleteFailure) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  base::FilePath untranscoded_path = storage_path().AppendASCII("ntr_key");
  ASSERT_TRUE(base::CreateDirectory(untranscoded_path));
  ASSERT_TRUE(
      base::WriteFile(untranscoded_path.AppendASCII("child"), kImageData));

  DeleteData(kImageKey);

  histograms.ExpectBucketCount(kCacheResultHistogram,
                               ImageFetcherCacheResult::kDataStoreDeleteFailed,
                               1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       RecordsSuccessesAndNotFoundPaths) {
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  SaveData(kImageKey, /*needs_transcoding=*/true);
  SaveData(kImageKey, /*needs_transcoding=*/false);
  AssertDataPresent(kImageKey, /*needs_transcoding=*/false);
  DeleteData(kImageKey);
  DeleteData(kImageKey);
  AssertDataPresent(kImageKey, "", /*needs_transcoding=*/false);
  AssertDataPresent(kImageKey, "", /*needs_transcoding=*/true);
  base::test::TestFuture<std::optional<std::vector<std::string>>> keys;
  data_store()->GetAllKeys(keys.GetCallback());
  ASSERT_TRUE(keys.Get().has_value());
  EXPECT_TRUE(keys.Take()->empty());

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreWriteSucceeded,
      2);
  histograms.ExpectBucketCount(kCacheResultHistogram,
                               ImageFetcherCacheResult::kDataStoreReadNotFound,
                               2);
  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 0);
}

#if BUILDFLAG(IS_POSIX)
TEST_F(CachedImageFetcherImageDataStoreDiskTest, ReportsReadAccessDenied) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Root can read files without filesystem permissions.";
  }
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  SaveData(kImageKey, /*needs_transcoding=*/false);
  ASSERT_TRUE(base::SetPosixFilePermissions(storage_path(), 0));
  base::ScopedClosureRunner restore_permissions(base::BindOnce(
      [](base::FilePath path) {
        EXPECT_TRUE(base::SetPosixFilePermissions(path, 0700));
      },
      storage_path()));

  AssertDataPresent(kImageKey, "", /*needs_transcoding=*/false);

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 1);
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest,
       ReportsKeyEnumerationAccessDenied) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Root can enumerate directories without permissions.";
  }
  base::HistogramTester histograms;
  CreateDataStore();
  InitializeDataStore();
  ASSERT_TRUE(base::SetPosixFilePermissions(storage_path(), 0));
  base::ScopedClosureRunner restore_permissions(base::BindOnce(
      [](base::FilePath path) {
        EXPECT_TRUE(base::SetPosixFilePermissions(path, 0700));
      },
      storage_path()));
  base::test::TestFuture<std::optional<std::vector<std::string>>> keys;
  data_store()->GetAllKeys(keys.GetCallback());
  EXPECT_FALSE(keys.Take().has_value());

  histograms.ExpectBucketCount(
      kCacheResultHistogram, ImageFetcherCacheResult::kDataStoreReadFailed, 1);
}
#endif

}  // namespace image_fetcher
