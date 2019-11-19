// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_data_store_disk.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
  CachedImageFetcherImageDataStoreDiskTest() {}
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateDataStore() {
    data_store_.reset();
    data_store_ = std::make_unique<ImageDataStoreDisk>(
        temp_dir_.GetPath(), base::SequencedTaskRunnerHandle::Get());
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

  MOCK_METHOD0(OnInitialized, void());
  MOCK_METHOD2(DataCallback, void(bool, std::string));
  MOCK_METHOD1(KeysCallback, void(std::vector<std::string>));

 private:
  std::unique_ptr<ImageDataStoreDisk> data_store_;

  ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherImageDataStoreDiskTest);
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

}  // namespace image_fetcher
