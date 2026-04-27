// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
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

base::FilePath GetTestEncryptedBookmarksFileNameInNewTempDir() {
  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  return temp_dir.Append(FILE_PATH_LITERAL("TestEncryptedBookmarks"));
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

std::string DictValueToJsonString(const base::DictValue& dict_value) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(
      dict_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);
  return json_string;
}

}  // namespace

class BookmarkStorageWithSingleFileTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  BookmarkStorageWithSingleFileTest()
      : clear_text_file_path_(GetTestBookmarksFileNameInNewTempDir()),
        encrypted_file_path_(GetTestEncryptedBookmarksFileNameInNewTempDir()) {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
    if (IsEncryptedFilePrimary()) {
      encryptor_ = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
    }
  }

  bool IsEncryptedFilePrimary() {
    return GetParam() ==
           BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted;
  }

  const base::FilePath& GetPrimaryFilepath() {
    if (IsEncryptedFilePrimary()) {
      return encrypted_file_path_;
    }
    return clear_text_file_path_;
  }

  const base::FilePath& GetSecondaryFilepath() {
    if (IsEncryptedFilePrimary()) {
      return clear_text_file_path_;
    }
    return encrypted_file_path_;
  }

  std::optional<base::DictValue> GetPrimaryFileContent() {
    if (IsEncryptedFilePrimary()) {
      return ReadEncryptedFileToDict(encrypted_file_path_, encryptor_->data);
    }
    return ReadFileToDict(clear_text_file_path_);
  }

  std::string GetHistogramImportantFileSuffix() {
    if (IsEncryptedFilePrimary()) {
      return ".BookmarkStorageEncrypted";
    }
    return ".BookmarkStorage";
  }

  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor_;
  const base::FilePath clear_text_file_path_;
  const base::FilePath encrypted_file_path_;
};

TEST_P(BookmarkStorageWithSingleFileTest, ShouldSaveFileToDiskAfterDelay) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor_,
      clear_text_file_path_, encrypted_file_path_);

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(GetPrimaryFilepath()));
  ASSERT_FALSE(base::PathExists(GetSecondaryFilepath()));

  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(GetPrimaryFilepath()));
  EXPECT_FALSE(base::PathExists(GetSecondaryFilepath()));

  // Advance clock until immediately before saving takes place.
  task_environment.FastForwardBy(BookmarkStorage::kSaveDelay -
                                 base::Milliseconds(10));
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(GetPrimaryFilepath()));
  EXPECT_FALSE(base::PathExists(GetSecondaryFilepath()));

  // Advance clock past the saving moment.
  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(GetPrimaryFilepath()));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {"ImportantFile.WriteDuration", GetHistogramImportantFileSuffix()}),
      1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Bookmarks.Storage.TimeToSerialize",
                    GetHistogramImportantFileSuffix()}),
      1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Bookmarks.BookmarksSerializationResult",
                    GetHistogramImportantFileSuffix()}),
      metrics::BookmarksSerializationResult::kSuccess, 1);
  EXPECT_FALSE(base::PathExists(GetSecondaryFilepath()));
}

TEST_P(BookmarkStorageWithSingleFileTest,
       ShouldSaveFileDespiteShutdownWhileScheduled) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  {
    base::test::TaskEnvironment task_environment{
        base::test::TaskEnvironment::TimeSource::MOCK_TIME};
    BookmarkStorage storage(
        model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor_,
        clear_text_file_path_, encrypted_file_path_);

    storage.ScheduleSave();
    ASSERT_TRUE(storage.HasScheduledSaveForTesting());
    ASSERT_FALSE(base::PathExists(GetPrimaryFilepath()));
    ASSERT_FALSE(base::PathExists(GetSecondaryFilepath()));
  }

  // TaskEnvironment and BookmarkStorage both have been destroyed, mimic-ing a
  // browser shutdown.
  EXPECT_TRUE(base::PathExists(GetPrimaryFilepath()));
  EXPECT_FALSE(base::PathExists(GetSecondaryFilepath()));
}

TEST_P(BookmarkStorageWithSingleFileTest,
       ShouldGenerateBackupFileUponFirstSave) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath backup_file_path =
      GetPrimaryFilepath().ReplaceExtension(FILE_PATH_LITERAL("bak"));

  // Create a dummy JSON file, to verify backups are created.
  ASSERT_TRUE(base::WriteFile(GetPrimaryFilepath(), "{}"));

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor_,
      clear_text_file_path_, encrypted_file_path_);

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

TEST_P(BookmarkStorageWithSingleFileTest, RecordTimeSinceLastScheduledSave) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor_,
      clear_text_file_path_, encrypted_file_path_);

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(GetPrimaryFilepath()));

  storage.ScheduleSave();

  base::TimeDelta delay_ms = base::Milliseconds(10);
  // Advance clock until immediately before saving takes place.
  task_environment.FastForwardBy(delay_ms);
  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(GetPrimaryFilepath()));

  // Advance clock past the saving moment.
  task_environment.FastForwardBy(BookmarkStorage::kSaveDelay + delay_ms);
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(GetPrimaryFilepath()));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", 2);
  histogram_tester.ExpectTimeBucketCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", delay_ms, 1);
}

TEST_P(BookmarkStorageWithSingleFileTest, ShouldSaveAccountNodes) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  model->CreateAccountPermanentFolders();
  ASSERT_NE(nullptr, model->account_bookmark_bar_node());

  const base::FilePath backup_file_path =
      GetPrimaryFilepath().ReplaceExtension(FILE_PATH_LITERAL("bak"));

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), BookmarkStorage::kSelectAccountNodes,
                          encryptor_, clear_text_file_path_,
                          encrypted_file_path_);

  ASSERT_FALSE(base::PathExists(GetPrimaryFilepath()));
  ASSERT_FALSE(base::PathExists(backup_file_path));

  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(base::PathExists(GetPrimaryFilepath()));
  EXPECT_FALSE(base::PathExists(backup_file_path));

  std::optional<base::DictValue> file_content = GetPrimaryFileContent();
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(file_content->empty());
}

TEST_P(BookmarkStorageWithSingleFileTest,
       ShouldSaveDespiteAccountBookmarksEmpty) {
  base::test::ScopedFeatureList features{
      switches::kSyncEnableBookmarksInTransportMode};

  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  ASSERT_EQ(nullptr, model->account_bookmark_bar_node());

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), BookmarkStorage::kSelectAccountNodes,
                          encryptor_, clear_text_file_path_,
                          encrypted_file_path_);

  ASSERT_EQ(GetPrimaryFileContent(), std::nullopt);

  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();

  std::optional<base::DictValue> file_content = GetPrimaryFileContent();
  ASSERT_TRUE(file_content.has_value());
  EXPECT_FALSE(file_content->empty());
}

INSTANTIATE_TEST_SUITE_P(
    BookmarkStorageWithSingleFileTest,
    BookmarkStorageWithSingleFileTest,
    ::testing::Values(
        BookmarkEncryptionStage::kDisabled,
        BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted));

class BookmarkStorageWithSecondayFileTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  BookmarkStorageWithSecondayFileTest() {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
  }

  std::string GetHistogramImportantFileSuffix() {
    if (GetParam() == BookmarkEncryptionStage::kWriteBothReadPreferEncrypted) {
      return ".BookmarkStorageEncrypted";
    }
    return ".BookmarkStorage";
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(BookmarkStorageWithSecondayFileTest,
       ShouldSaveUnencryptedAndEncryptedBookmarks) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();

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
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Bookmarks.Storage.TimeToSerialize",
                    GetHistogramImportantFileSuffix()}),
      1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Bookmarks.BookmarksSerializationResult",
                    GetHistogramImportantFileSuffix()}),
      metrics::BookmarksSerializationResult::kSuccess, 1);
}

TEST_P(BookmarkStorageWithSecondayFileTest,
       ShouldGenerateTwoBackupFilesUponFirstSave) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath backup_file_path =
      bookmarks_file_path.ReplaceExtension(FILE_PATH_LITERAL("bak"));
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();
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

TEST_P(BookmarkStorageWithSecondayFileTest,
       ShouldSaveUnencryptedEvenIfEncryptionFails) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place,
          os_crypt_async::GetTestEncryptorWithoutKeysForTesting());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);

  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  task_environment.FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Bookmarks.BookmarksSerializationResult",
                    GetHistogramImportantFileSuffix()}),
      metrics::BookmarksSerializationResult::kEncryptionFailed, 1);

  // No encryption file should have been written.
  ASSERT_FALSE(base::PathExists(encrypted_bookmarks_file_path));
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageEncrypted", 0);

  // Clear text file should have been written.
  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  BookmarkCodec codec;
  base::DictValue expected_file_content = codec.Encode(
      model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
      model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());
  EXPECT_EQ(expected_file_content, *file_content);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorage", 1);
}

TEST_P(BookmarkStorageWithSecondayFileTest,
       SaveSingleFileIfNoPreviousSave_OnlyEncryptedFileIsSavedRightAway) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);
  BookmarkCodec codec;
  base::DictValue expected_file_content = codec.Encode(
      model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
      model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());

  storage.SaveSingleFileIfNoPreviousSave(
      StorageFileEncryptionType::kEncrypted,
      DictValueToJsonString(expected_file_content));
  // No impact on the unencrypted bookmarks file.
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  task_environment.FastForwardUntilNoTasksRemain();

  ASSERT_FALSE(base::PathExists(bookmarks_file_path));
  std::optional<base::DictValue> decrypted_file_content =
      ReadEncryptedFileToDict(encrypted_bookmarks_file_path, encryptor->data);
  ASSERT_TRUE(decrypted_file_content.has_value());
  EXPECT_EQ(expected_file_content, *decrypted_file_content);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageEncryptedImmediate", 1);
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeToSerialize.BookmarkStorageEncryptedImmediate", 1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.BookmarksSerializationResult."
      "BookmarkStorageEncryptedImmediate",
      metrics::BookmarksSerializationResult::kSuccess, 1);
}

TEST_P(BookmarkStorageWithSecondayFileTest,
       SaveSingleFileIfNoPreviousSave_OnlyClearTextFileIsSavedRightAway) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkCodec codec;
  base::DictValue expected_file_content = codec.Encode(
      model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
      model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);

  storage.SaveSingleFileIfNoPreviousSave(
      StorageFileEncryptionType::kClearText,
      DictValueToJsonString(expected_file_content));
  task_environment.FastForwardUntilNoTasksRemain();

  ASSERT_FALSE(base::PathExists(encrypted_bookmarks_file_path));
  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  ASSERT_TRUE(file_content.has_value());
  EXPECT_EQ(expected_file_content, *file_content);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageImmediate", 1);
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeToSerialize.BookmarkStorageImmediate", 1);
  histogram_tester.ExpectUniqueSample(
      "Bookmarks.BookmarksSerializationResult.BookmarkStorageImmediate",
      metrics::BookmarksSerializationResult::kSuccess, 1);
}

TEST_P(BookmarkStorageWithSecondayFileTest,
       SaveSingleFileIfNoPreviousSave_SaveToBothFilesIfWriteAlreadyScheduled) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);
  BookmarkCodec codec;
  base::DictValue expected_file_content = codec.Encode(
      model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
      model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());

  storage.ScheduleSave();
  storage.SaveSingleFileIfNoPreviousSave(
      StorageFileEncryptionType::kEncrypted,
      DictValueToJsonString(expected_file_content));
  task_environment.FastForwardUntilNoTasksRemain();

  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  std::optional<base::DictValue> decrypted_file_content =
      ReadEncryptedFileToDict(encrypted_bookmarks_file_path, encryptor->data);
  ASSERT_TRUE(decrypted_file_content.has_value());
  EXPECT_EQ(expected_file_content, *file_content);
  EXPECT_EQ(expected_file_content, *decrypted_file_content);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorage", 1);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageEncrypted", 1);
}

TEST_P(BookmarkStorageWithSecondayFileTest,
       SaveSingleFileIfNoPreviousSave_NoImpactIfSaveAlreadyCompleted) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();

  const base::FilePath bookmarks_file_path =
      GetTestBookmarksFileNameInNewTempDir();
  const base::FilePath encrypted_bookmarks_file_path =
      GetTestEncryptedBookmarksFileNameInNewTempDir();

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(
      model.get(), BookmarkStorage::kSelectLocalOrSyncableNodes, encryptor,
      bookmarks_file_path, encrypted_bookmarks_file_path);
  BookmarkCodec codec;
  base::DictValue expected_file_content = codec.Encode(
      model->bookmark_bar_node(), model->other_node(), model->mobile_node(),
      model->client()->EncodeLocalOrSyncableBookmarkSyncMetadata());

  storage.ScheduleSave();
  task_environment.FastForwardUntilNoTasksRemain();
  // Try to write a different json content to the file.
  storage.SaveSingleFileIfNoPreviousSave(StorageFileEncryptionType::kEncrypted,
                                         "{}");
  task_environment.FastForwardUntilNoTasksRemain();

  std::optional<base::DictValue> file_content =
      ReadFileToDict(bookmarks_file_path);
  std::optional<base::DictValue> decrypted_file_content =
      ReadEncryptedFileToDict(encrypted_bookmarks_file_path, encryptor->data);
  ASSERT_TRUE(decrypted_file_content.has_value());
  // SaveToSingleFileNow was a no-op, file content still matches the original
  // save.
  EXPECT_EQ(expected_file_content, *file_content);
  EXPECT_EQ(expected_file_content, *decrypted_file_content);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorage", 1);
  histogram_tester.ExpectTotalCount(
      "ImportantFile.WriteDuration.BookmarkStorageEncrypted", 1);
}

INSTANTIATE_TEST_SUITE_P(
    BookmarkStorageWithSecondayFileTest,
    BookmarkStorageWithSecondayFileTest,
    ::testing::Values(BookmarkEncryptionStage::kWriteBothReadOnlyClear,
                      BookmarkEncryptionStage::kWriteBothReadPreferEncrypted));

}  // namespace bookmarks
