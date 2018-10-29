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
#include "base/test/scoped_task_environment.h"
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

class ImageDataStoreDiskTest : public testing::Test {
 public:
  ImageDataStoreDiskTest() {}
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateDataStore() {
    data_store_.reset();
    data_store_ = std::make_unique<ImageDataStoreDisk>(
        temp_dir_.GetPath(), base::SequencedTaskRunnerHandle::Get());
  }

  void InitializeDataStore() {
    EXPECT_CALL(*this, OnInitialized());
    data_store()->Initialize(base::BindOnce(
        &ImageDataStoreDiskTest::OnInitialized, base::Unretained(this)));
    RunUntilIdle();
  }

  void PrepareDataStore(bool initialize) {
    CreateDataStore();
    InitializeDataStore();
    SaveData(kImageKey);

    if (!initialize) {
      CreateDataStore();
    }
  }

  void AssertDataPresent(const std::string& key) {
    AssertDataPresent(key, kImageData);
  }

  void AssertDataPresent(const std::string& key, const std::string& data) {
    EXPECT_CALL(*this, DataCallback(data));
    data_store()->LoadImage(
        key, base::BindOnce(&ImageDataStoreDiskTest::DataCallback,
                            base::Unretained(this)));
    RunUntilIdle();
  }

  void SaveData(const std::string& key) {
    data_store()->SaveImage(key, kImageData);
    RunUntilIdle();
  }

  void DeleteData(const std::string& key) {
    data_store()->DeleteImage(key);
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }
  ImageDataStore* data_store() { return data_store_.get(); }

  MOCK_METHOD0(OnInitialized, void());
  MOCK_METHOD1(DataCallback, void(std::string));
  MOCK_METHOD1(KeysCallback, void(std::vector<std::string>));

 private:
  std::unique_ptr<ImageDataStoreDisk> data_store_;

  ScopedTempDir temp_dir_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ImageDataStoreDiskTest);
};

TEST_F(ImageDataStoreDiskTest, SanityTest) {
  CreateDataStore();
  InitializeDataStore();

  SaveData(kImageKey);
  AssertDataPresent(kImageKey);

  DeleteData(kImageKey);
  AssertDataPresent(kImageKey, "");
}

TEST_F(ImageDataStoreDiskTest, Init) {
  CreateDataStore();
  ASSERT_FALSE(data_store()->IsInitialized());

  InitializeDataStore();
  ASSERT_TRUE(data_store()->IsInitialized());
}

TEST_F(ImageDataStoreDiskTest, InitWithExistingDirectory) {
  PrepareDataStore(/* initialize */ true);

  // Recreating the data store shouldn't wipe the directory.
  CreateDataStore();
  InitializeDataStore();

  AssertDataPresent(kImageKey);
}

TEST_F(ImageDataStoreDiskTest, SaveBeforeInit) {
  PrepareDataStore(/* initialize */ false);
  // No data should be present (empty string).
  AssertDataPresent(kImageKey, "");
}

TEST_F(ImageDataStoreDiskTest, Save) {
  PrepareDataStore(/* initialize */ true);
  AssertDataPresent(kImageKey);
}

TEST_F(ImageDataStoreDiskTest, DeleteBeforeInit) {
  PrepareDataStore(/* initialize */ false);

  DeleteData(kImageKey);

  InitializeDataStore();
  // Delete should have failed, data still there.
  AssertDataPresent(kImageKey);
}

TEST_F(ImageDataStoreDiskTest, Delete) {
  PrepareDataStore(/* initialize */ true);

  DeleteData(kImageKey);

  // Should be empty string (not present).
  AssertDataPresent(kImageKey, "");
}

TEST_F(ImageDataStoreDiskTest, GetAllKeysBeforeInit) {
  PrepareDataStore(/* initialize */ false);

  // Should return empty vector even though there is a file present.
  EXPECT_CALL(*this, KeysCallback(std::vector<std::string>()));
  data_store()->GetAllKeys(base::BindOnce(&ImageDataStoreDiskTest::KeysCallback,
                                          base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageDataStoreDiskTest, GetAllKeys) {
  PrepareDataStore(/* initialize */ true);

  // Should return empty vector even though there is a file present.
  EXPECT_CALL(*this, KeysCallback(std::vector<std::string>({kImageKey})));
  data_store()->GetAllKeys(base::BindOnce(&ImageDataStoreDiskTest::KeysCallback,
                                          base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageDataStoreDiskTest, QueuedLoadIsServedBeforeDelete) {
  CreateDataStore();
  InitializeDataStore();

  SaveData(kImageKey);
  EXPECT_CALL(*this, DataCallback(kImageData));
  data_store()->LoadImage(kImageKey,
                          base::BindOnce(&ImageDataStoreDiskTest::DataCallback,
                                         base::Unretained(this)));
  DeleteData(kImageKey);

  RunUntilIdle();
}

}  // namespace image_fetcher
