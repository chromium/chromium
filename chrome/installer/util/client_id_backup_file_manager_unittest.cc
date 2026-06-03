// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/client_id_backup_file_manager.h"

#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Eq;
using ::testing::Optional;

constexpr char kValidUuid1[] = "c10aa2bf-4d89-4e22-870d-f0b881d9de22";
constexpr char kValidUuid2[] = "587cf40a-756d-637a-83ff-5edc31a83103";

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

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheOrDisk_WithValidFile) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, kValidUuid1));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(std::string(kValidUuid1)));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 0, 1);
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheOrDisk_EmptyFile) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, ""));

  // Empty file is a valid placeholder indicating consent with no ID yet.
  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(std::string("")));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 2, 1);
}

TEST_F(ClientIdBackupFileManagerTest,
       ClientIdFromCacheOrDisk_WhitespaceOnlyFile) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  std::string file_content = "   \n  \r\n ";
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(file_content));
  EXPECT_TRUE(base::PathExists(file_path));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 3, 1);
}

TEST_F(ClientIdBackupFileManagerTest,
       ClientIdFromCacheOrDisk_ValidWithWhitespace) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  std::string file_content = "   \n ";
  file_content += kValidUuid1;
  file_content += " \r\n ";
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(file_content));
  EXPECT_TRUE(base::PathExists(file_path));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 3, 1);
}

TEST_F(ClientIdBackupFileManagerTest,
       ClientIdFromCacheOrDisk_InvalidUuidFormat) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  std::string file_content = "c10aa2bf4d894e22870df0b881d9de22";
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(file_content));
  EXPECT_TRUE(base::PathExists(file_path));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 3, 1);
}

TEST_F(ClientIdBackupFileManagerTest,
       ClientIdFromCacheOrDisk_InvalidCharacters) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  std::string file_content = "c10aa2bf-4d89-4e22-870d-f0b881d9de2z";
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(file_content));
  EXPECT_TRUE(base::PathExists(file_path));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 3, 1);
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheOrDisk_UppercaseUuid) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  // Uuids containing uppercase characters are now accepted, but log a different
  // sample.
  std::string uppercase_uuid = kValidUuid1;
  for (char& c : uppercase_uuid) {
    c = toupper(c);
  }
  ASSERT_TRUE(base::WriteFile(file_path, uppercase_uuid));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(uppercase_uuid));
  EXPECT_TRUE(base::PathExists(file_path));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 1, 1);
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheOrDisk_FileTooLarge) {
  base::HistogramTester histogram_tester;
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  std::string file_content(65, 'a');
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  EXPECT_THAT(
      ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk(),
      Optional(file_content));
  EXPECT_TRUE(base::PathExists(file_path));

  histogram_tester.ExpectUniqueSample("UMA.ClientIdBackupValidation", 3, 1);
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCache) {
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, kValidUuid1));

  ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk();
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(std::string(kValidUuid1)));
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheBeforeReading) {
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();
  ASSERT_TRUE(base::WriteFile(file_path, kValidUuid1));

  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Eq(std::nullopt));

  // Read it from the file to make sure that it was actually there.
  ClientIdBackupFileManager::GetInstance().ClientIdFromCacheOrDisk();
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(std::string(kValidUuid1)));
}

TEST_F(ClientIdBackupFileManagerTest, ClientIdFromCacheBeforeInit) {
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Eq(std::nullopt));
}

TEST_F(ClientIdBackupFileManagerTest, SetClientId) {
  EXPECT_TRUE(ClientIdBackupFileManager::GetInstance().SetClientIdForTesting(
      kValidUuid1));

  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();

  // Check the file on disk.
  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_content));
  EXPECT_EQ(file_content, kValidUuid1);

  // Check the cache.
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(std::string(kValidUuid1)));
}

TEST_F(ClientIdBackupFileManagerTest, SetClientId_NullOpt) {
  const base::FilePath file_path =
      ClientIdBackupFileManager::GetBackupFilePathForTesting();

  ASSERT_TRUE(base::WriteFile(file_path, kValidUuid1));
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
  ClientIdBackupFileManager::GetInstance().SetClientIdForTesting(kValidUuid1);
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(std::string(kValidUuid1)));

  ClientIdBackupFileManager::GetInstance().SetClientIdForTesting(kValidUuid2);
  EXPECT_THAT(ClientIdBackupFileManager::GetInstance().ClientIdFromCache(),
              Optional(std::string(kValidUuid2)));
}
