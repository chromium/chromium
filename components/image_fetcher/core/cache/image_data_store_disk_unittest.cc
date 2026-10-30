// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_data_store_disk.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ScopedTempDir;
using testing::Mock;

namespace image_fetcher {

namespace {
constexpr char kImageKey[] = "key";
constexpr char kImageData[] = "data";
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

  MOCK_METHOD(void, OnInitialized, (), ());
  MOCK_METHOD(void, DataCallback, (bool, std::string), ());
  MOCK_METHOD(void, KeysCallback, (std::vector<std::string>), ());

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

TEST_F(CachedImageFetcherImageDataStoreDiskTest, GetAllKeysBeforeInit) {
  PrepareDataStore(/* initialize */ false, /* needs_transcoding */ false);

  // Should return empty vector even though there is a file present.
  EXPECT_CALL(*this, KeysCallback(std::vector<std::string>()));
  data_store()->GetAllKeys(
      base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::KeysCallback,
                     base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(CachedImageFetcherImageDataStoreDiskTest, GetAllKeys) {
  PrepareDataStore(/* initialize */ true, /* needs_transcoding */ false);

  // Should return empty vector even though there is a file present.
  EXPECT_CALL(*this, KeysCallback(std::vector<std::string>({kImageKey})));
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
  EXPECT_CALL(*this, KeysCallback(std::vector<std::string>()));
  data_store()->GetAllKeys(
      base::BindOnce(&CachedImageFetcherImageDataStoreDiskTest::KeysCallback,
                     base::Unretained(this))
          .Then(run_loop.QuitClosure()));
  run_loop.Run();

  DeleteData(kImageKey);
  EXPECT_TRUE(base::PathExists(external_existing));
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace image_fetcher
