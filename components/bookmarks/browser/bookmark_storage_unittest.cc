// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

namespace {

std::unique_ptr<BookmarkModel> CreateModelWithOneBookmark() {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->ClearStore();
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  model->AddURL(bookmark_bar, 0, std::u16string(), GURL("http://url1.com"));
  return model;
}

}  // namespace

TEST(BookmarkStorageTest, ShouldSaveFileToDiskAfterDelay) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), temp_dir.GetPath());

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));

  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));

  // Advance clock until immediately before saving takes place.
  task_environment.FastForwardBy(BookmarkStorage::kSaveDelay -
                                 base::Milliseconds(10));
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));

  // Advance clock past the saving moment.
  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", 1);
}

TEST(BookmarkStorageTest, ShouldSaveFileDespiteShutdownWhileScheduled) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    base::test::TaskEnvironment task_environment{
        base::test::TaskEnvironment::TimeSource::MOCK_TIME};
    BookmarkStorage storage(model.get(), temp_dir.GetPath());

    storage.ScheduleSave();
    ASSERT_TRUE(storage.HasScheduledSaveForTesting());
    ASSERT_FALSE(
        base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));
  }

  // TaskEnvironment and BookmarkStorage both have been destroyed, mimic-ing a
  // browser shutdown.
  EXPECT_TRUE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));
}

TEST(BookmarkStorageTest, ShouldGenerateBackupFileUponFirstSave) {
  std::unique_ptr<BookmarkModel> model = CreateModelWithOneBookmark();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath bookmarks_file_path =
      temp_dir.GetPath().Append(kBookmarksFileName);
  const base::FilePath backup_file_path =
      bookmarks_file_path.ReplaceExtension(FILE_PATH_LITERAL("bak"));

  // Create a dummy JSON file, to verify backups are created.
  ASSERT_NE(0, base::WriteFile(bookmarks_file_path, "{}", 2));

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), temp_dir.GetPath());

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
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BookmarkStorage storage(model.get(), temp_dir.GetPath());

  ASSERT_FALSE(storage.HasScheduledSaveForTesting());
  ASSERT_FALSE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));

  storage.ScheduleSave();

  base::TimeDelta delay_ms = base::Milliseconds(10);
  // Advance clock until immediately before saving takes place.
  task_environment.FastForwardBy(delay_ms);
  storage.ScheduleSave();
  EXPECT_TRUE(storage.HasScheduledSaveForTesting());
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));

  // Advance clock past the saving moment.
  task_environment.FastForwardBy(BookmarkStorage::kSaveDelay + delay_ms);
  EXPECT_FALSE(storage.HasScheduledSaveForTesting());
  EXPECT_TRUE(base::PathExists(temp_dir.GetPath().Append(kBookmarksFileName)));
  histogram_tester.ExpectTotalCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", 2);
  histogram_tester.ExpectTimeBucketCount(
      "Bookmarks.Storage.TimeSinceLastScheduledSave", delay_ms, 1);
}

}  // namespace bookmarks
