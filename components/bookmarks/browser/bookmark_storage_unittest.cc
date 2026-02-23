// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/test/bookmark_test_with_encryption_stages.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

namespace {

base::FilePath GetTestBookmarksFileNameInNewTempDir() {
  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  return temp_dir.Append(FILE_PATH_LITERAL("TestBookmarks"));
}

std::unique_ptr<BookmarkModel> CreateModelWithOneBookmark() {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  model->AddURL(bookmark_bar, 0, std::u16string(), GURL("http://url1.com"));
  return model;
}

std::optional<base::DictValue> ReadFileToDict(const base::FilePath& file_path) {
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    return std::nullopt;
  }
  return base::JSONReader::ReadDict(file_content,
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
}

std::optional<base::DictValue> ReadEncryptedFileToDict(
    const base::FilePath& file_path,
    const os_crypt_async::Encryptor& encryptor) {
  std::string encrypted_file_content;
  if (!base::ReadFileToString(file_path, &encrypted_file_content)) {
    return std::nullopt;
  }

  std::string decrypted_file_content;
  if (!encryptor.DecryptString(encrypted_file_content,
                               &decrypted_file_content)) {
    return std::nullopt;
  }

  return base::JSONReader::ReadDict(decrypted_file_content,
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS);
}

}  // namespace

TEST(BookmarkStorageTest, ShouldSaveFileToDiskAfterDelay) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(),
                          BookmarkStorage::kSelectLocalOrSyncableNodes,
                          /*encryptor=*/nullptr, bookmarks_file_path,
                          /*encrypted_file_path=*/std::nullopt);

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(bookmarks_file_path));

  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(bookmarks_file_path));

  // Advance clock until immediately before saving takes place.
  task_environment.FastForwardBy(BookmarkStorage::kSaveDelay -
                                 base::Milliseconds(10));
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(bookmarks_file_path));

  // Advance clock past the saving moment.
  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(bookmarks_file_path));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", 1);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorage", 1);
}

TEST(BookmarkStorageTest, ShouldSaveFileDespiteShutdownWhileScheduled) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();

  {
    base::test::TaskEnvironment task_environment{
        base::test::TaskEnvironment::TimeSource::MOCK_TIME};
    BookmarkStorage storage(model.get(),
                            BookmarkStorage::kSelectLocalOrSyncableNodes,
                            /*encryptor=*/nullptr, bookmarks_file_path,
                            /*encrypted_file_path=*/std::nullopt);

    storage.ScheduleSave();
    ASSERT_TRUE(storage.HasScheduledSaveForTesting());
    ASSERT_FALSE(base::PathExists(bookmarks_file_path));
  }

  // TaskEnvironment and BookmarkStorage both have been destroyed, mimic-ing a
  // browser shutdown.
  EXPECT_TRUE(base::PathExists(bookmarks_file_path));
}

TEST(BookmarkStorageTest, ShouldGenerateBackupFileUponFirstSave) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath backup_file_path =
      bookmarks_file_path.ReplaceExtension(FILE_PATH_LITERAL("bak"));

  // Create a dummy JSON file, to verify backups are created.
  ASSERT_TRUE(base::WriteFile(bookmarks_file_path, "{}"));

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(),
                          BookmarkStorage::kSelectLocalOrSyncableNodes,
                          /*encryptor=*/nullptr, bookmarks_file_path,
                          /*encrypted_file_path=*/std::nullopt);

  // The backup file should be created upon first save, not earlier.
  task_environment.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(backup_file_path));

  storage.ScheduleSave();
  task_environment.RunUntilIdle();

  ASSERT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(backup_file_path));

  // Delete the file to verify it doesn't get saved again.
  task_environment.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(base::DeleteFile(backup_file_path));

  // A second scheduled save should not generate another backup.
  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(backup_file_path));
}

TEST(BookmarkStorageTest, RecordTimeSinceLastScheduledSave) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(),
                          BookmarkStorage::kSelectLocalOrSyncableNodes,
                          /*encryptor=*/nullptr, bookmarks_file_path,
                          /*encrypted_file_path=*/std::nullopt);

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(bookmarks_file_path));

  storage.ScheduleSave();

  base::TimeDelta delay_ms = base::Milliseconds(10);
  // Advance clock until immediately before saving takes place.
  task_environment.FastForwardBy(delay_ms);
  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(bookmarks_file_path));

  // Advance clock past the saving moment.
  task_environment.FastForwardBy(BookmarkStorage::kSaveDelay + delay_ms);
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(bookmarks_file_path));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", 2);
  histogram_tester.ExpectTimeBucketCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", delay_ms, 1);
}

TEST(BookmarkStorageTest, ShouldSaveAccountNodes) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  model->CreateAccountPermanentFolders();
  ASSERT_NE(nullptr, model->account_bookmark_bar_node());

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath backup_file_path =
      bookmarks_file_path.ReplaceExtension(FILE_PATH_LITERAL("bak"));

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), BookmarkStorage::kSelectAccountNodes,
                          /*encryptor=*/nullptr, bookmarks_file_path,
                          /*encrypted_file_path=*/std::nullopt);

  ASSERT_FALSE(base::PathExists(bookmarks_file_path));
  ASSERT_FALSE(base::PathExists(backup_file_path));

  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(base::PathExists(bookmarks_file_path));
  EXPECT_FALSE(base::PathExists(backup_file_path));

  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(file_content->empty());
}

TEST(BookmarkStorageTest, ShouldSaveDespiteAccountBookmarksEmpty) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  ASSERT_EQ(nullptr, model->account_bookmark_bar_node());

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), BookmarkStorage::kSelectAccountNodes,
                          /*encryptor=*/nullptr, bookmarks_file_path,
                          /*encrypted_file_path=*/std::nullopt);

  ASSERT_EQ(ReadFileToDict(bookmarks_file_path), std::nullopt);

  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();

  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(file_content->empty());
}

TEST(BookmarkStorageTest, ShouldSaveUnencryptedAndEncryptedBookmarks) {
  base::test::ScopedFeatureList features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      features, BookmarkEncryptionStage::kWriteBothReadOnlyClear);
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  const base::FilePath bookmarks_file_path =
      temp_dir.Append(FILE_PATH_LITERAL("TestBookmarks"));
  const base::FilePath encrypted_bookmarks_file_path =
      temp_dir.Append(FILE_PATH_LITERAL("TestEncryptedBookmarks"));

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);

  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  task_environment.FastForwardUntilNoTasksRemain();

  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  std::optional<base::DictValue> decrypted_file_content =
      ReadEncryptedFileToDict(encrypted_bookmarks_file_path, encryptor->data);
  ASSERT_TRUE(decrypted_file_content.has_value());
  BookmarkCodec codec;
  base::DictValue expected_file_content = codec.Encode(
      model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
      model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());
  EXPECT_EQ(expected_file_content, *file_content);
  EXPECT_EQ(expected_file_content, *decrypted_file_content);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorage", 1);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageEncrypted", 1);
}

TEST(BookmarkStorageTest, ShouldGenerateTwoBackupFilesUponFirstSave) {
  base::test::ScopedFeatureList features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      features, BookmarkEncryptionStage::kWriteBothReadOnlyClear);
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  const base::FilePath bookmarks_file_path =
      temp_dir.Append(FILE_PATH_LITERAL("TestBookmarks"));
  const base::FilePath backup_file_path =
      bookmarks_file_path.ReplaceExtension(FILE_PATH_LITERAL("bak"));
  const base::FilePath encrypted_bookmarks_file_path =
      temp_dir.Append(FILE_PATH_LITERAL("TestEncryptedBookmarks"));
  const base::FilePath encrypted_backup_file_path =
      encrypted_bookmarks_file_path.ReplaceExtension(FILE_PATH_LITERAL("bak"));

  // Create a dummy JSON file, to verify backups are created.
  ASSERT_TRUE(base::WriteFile(bookmarks_file_path, "{}"));
  ASSERT_TRUE(base::WriteFile(encrypted_bookmarks_file_path, "{}"));

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);

  // The backup file should be created upon first save, not earlier.
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(backup_file_path));
  EXPECT_FALSE(base::PathExists(encrypted_backup_file_path));

  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(base::PathExists(backup_file_path));
  EXPECT_TRUE(base::PathExists(encrypted_backup_file_path));

  // Delete the file to verify it doesn't get saved again.
  ASSERT_TRUE(base::DeleteFile(backup_file_path));
  ASSERT_TRUE(base::DeleteFile(encrypted_backup_file_path));

  // A second scheduled save should not generate another backup.
  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(backup_file_path));
  EXPECT_FALSE(base::PathExists(encrypted_backup_file_path));
}

}  // namespace bookmarks
