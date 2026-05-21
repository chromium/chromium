// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/client_id_backup_file_manager.h"

#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Eq;
using ::testing::Optional;

}  // namespace

class ClientIdBackupFileManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_data_dir_override_.emplace(chrome::DIR_USER_DATA, temp_dir_.GetPath());
  }

  void TearDown() override {
    ClientIdBackupFileManager::GetInstance().ResetForTesting();
  }

  base::ScopedTempDir temp_dir_;
  std::optional<base::ScopedPathOverride> user_data_dir_override_;
};

TEST_F(ClientIdBackupFileManagerTest, GetInstance) {
  ClientIdBackupFileManager& instance1 =
      ClientIdBackupFileManager::GetInstance();
  ClientIdBackupFileManager& instance2 =
      ClientIdBackupFileManager::GetInstance();
  EXPECT_EQ(&instance1, &instance2);
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheOrDisk_NoFile) {
  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Eq(std::nullopt));
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheOrDisk_WithFile) {
  const std::string kClientId = "test_client_id";
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, kClientId));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(kClientId));
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCache) {
  const std::string kClientId = "test_client_id_cache";
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, kClientId));

  ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk();
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(kClientId));
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheBeforeReading) {
  const std::string kClientId = "test_client_id_cache";
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, kClientId));

  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Eq(std::nullopt));

  // Read it from the file to make sure that it was actually there.
  ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk();
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(kClientId));
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheBeforeInit) {
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Eq(std::nullopt));
}

TEST_F(ClientIdBackupFileManagerTest, SetClientId) {
  const std::string kClientId = "new_client_id";
  EXPECT_TRUE(ClientIdBackupFileManager::GetInstance().SetClientIdForTesting(
      kClientId));

  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();

  // Check the file on disk.
  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_content));
  EXPECT_EQ(file_content, kClientId);

  // Check the cache.
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(kClientId));
}

TEST_F(ClientIdBackupFileManagerTest, SetClientId_NullOpt) {
  const std::string kClientId = "initial_id";
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();

  ASSERT_TRUE(base::WriteFile(file_path, kClientId));
  ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk();

  EXPECT_TRUE(
      ClientIdBackupFileManager::GetInstance().ClearClientIdForTesting());

  // File should be deleted.
  EXPECT_FALSE(base::PathExists(file_path));

  // Cache should be cleared.
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Eq(std::nullopt));
}

TEST_F(ClientIdBackupFileManagerTest, CacheUpdateOnSet) {
  const std::string kClientId1 = "id1";
  const std::string kClientId2 = "id2";

  ClientIdBackupFileManager::GetInstance().SetClientIdForTesting(kClientId1);
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(kClientId1));

  ClientIdBackupFileManager::GetInstance().SetClientIdForTesting(kClientId2);
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(kClientId2));
}
