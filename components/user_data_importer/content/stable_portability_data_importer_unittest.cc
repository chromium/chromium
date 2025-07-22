// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/stable_portability_data_importer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace user_data_importer {

class StablePortabilityDataImporterTest : public testing::Test {
 protected:
  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(history_dir_.GetPath(),
                                                     /*create_db=*/false);

    auto bookmark_client = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ =
        std::make_unique<bookmarks::BookmarkModel>(std::move(bookmark_client));

    auto storage = std::make_unique<FakeReadingListModelStorage>();
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever,
        base::DefaultClock::GetInstance());

    importer_ = std::make_unique<StablePortabilityDataImporter>(
        *history_service_, *bookmark_model_, *reading_list_model_);
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  int GetNumberOfBookmarksImported() const {
    return number_bookmarks_imported_;
  }

  int GetNumberOfReadingListImported() const {
    return number_reading_list_imported_;
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingBookmarks() const {
    return importer_->pending_bookmarks_;
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingReadingList() const {
    return importer_->pending_reading_list_;
  }

  void ImportBookmarks(const std::string& html_data) {
    bookmarks_callback_called_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("bookmarks.html");
    ASSERT_TRUE(base::WriteFile(path, html_data));
    ImportBookmarksFile(path);
  }

  void ImportReadingList(const std::string& html_data) {
    reading_list_callback_called_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("reading_list.html");
    ASSERT_TRUE(base::WriteFile(path, html_data));
    ImportReadingListFile(path);
  }

  void ImportBookmarksFile(const base::FilePath& bookmarks_file) {
    PrepareCallbacks();

    importer_->ImportBookmarks(
        bookmarks_file,
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&StablePortabilityDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)));

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return bookmarks_callback_called_; }));
  }

  void ImportReadingListFile(const base::FilePath& reading_list_file) {
    PrepareCallbacks();

    importer_->ImportReadingList(
        reading_list_file,
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(
            &StablePortabilityDataImporterTest::OnReadingListConsumed,
            base::Unretained(this)));

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return reading_list_callback_called_; }));
  }

  base::ScopedMockClockOverride clock_;

 private:
  void OnBookmarksConsumed(int number_imported) {
    bookmarks_callback_called_ = true;
    number_bookmarks_imported_ = number_imported;
  }

  void OnReadingListConsumed(int number_imported) {
    reading_list_callback_called_ = true;
    number_reading_list_imported_ = number_imported;
  }

  void PrepareCallbacks() {
    bookmarks_callback_called_ = false;
    reading_list_callback_called_ = false;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<ReadingListModel> reading_list_model_;
  bool bookmarks_callback_called_ = false;
  bool reading_list_callback_called_ = false;
  int number_bookmarks_imported_ = -1;
  int number_reading_list_imported_ = -1;
  std::unique_ptr<StablePortabilityDataImporter> importer_;
};

// Tests parsing a simple HTML file with two bookmarks.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_Basic) {
  ImportBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>
      </DL>)");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

// Identical to the above test, but without the top-level <DL> tag enclosing it.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_NoTopLevelDL) {
  ImportBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>)");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

// Tests parsing an HTML file with folders, both empty and with bookmarks.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_Folders) {
  ImportBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><H3>Folder 1</H3>
      <DL><p>
        <DT><A HREF="https://www.example.com/" ADD_DATE="915181200">Example</A>
        <DT><H3 ADD_DATE="1145523600">Folder 1.1</H3>
        <DL><p>
          <DT><A HREF="https://en.wikipedia.org/wiki/Kitsune" ADD_DATE="1674205200">Kitsune</A>
        </DL><p>
      </DL><p>
      <DT><H3>Empty Folder</H3>
      <DL><p>
      </DL>
      </DL>)");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 4);

  ASSERT_EQ(GetPendingBookmarks().size(), 4u);

  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(915181200));
  EXPECT_EQ(entry.url, GURL("https://www.example.com/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Kitsune");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(1674205200));
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/Kitsune"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1", u"Folder 1.1"));

  entry = GetPendingBookmarks()[3];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Empty Folder");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

// Tests parsing a simple HTML file with three reading list items.
TEST_F(StablePortabilityDataImporterTest, ReadingList) {
  ImportReadingList(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://en.wikipedia.org/wiki/The_Beach_Boys">The Beach Boys</A>
      <DT><A HREF="https://en.wikipedia.org/wiki/Brian_Wilson" ADD_DATE="-868878000">Brian Wilson</A>
      </DL><p>
      </DL>)");
  EXPECT_EQ(GetNumberOfReadingListImported(), 3);

  EXPECT_EQ(GetPendingReadingList().size(), 3u);

  ASSERT_EQ(GetPendingBookmarks().size(), 0u);

  ImportedBookmarkEntry entry = GetPendingReadingList()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingReadingList()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"The Beach Boys");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/The_Beach_Boys"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingReadingList()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Brian Wilson");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/Brian_Wilson"));
  EXPECT_THAT(entry.path, IsEmpty());
}

// Tests parsing an HTML with several not valid formats. The parser should still
// try to parse as many items as possible.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_MiscJunk) {
  ImportBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A>Google</A>
      <DT><H3>Folder 1</H3>
      <DL><p>
        <DT><A HREF="https://www.chromium.org/">Chromium</A>
        ICON_URI="https://www.chromium.org/favicon.ico"
        <DT><A HREF="https://www.example.org/" ADD_DATE="Last Tuesday">Example</A>
        <DT><A>Google Reader</A>
      </DL><p>
      <!-- Various unsupported types below -->
      FEED="true"
      FEEDURL="https://www.example.com"
      WEBSLICE="true"
      ISLIVEPREVIEW="true"
      PREVIEWSIZE="100 x 100"
      </DL>)");

  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);

  // <A>Google</A> was skipped for lack of URL.

  // The folder contains a mix of invalid and valid entries. Ensure the valid
  // ones are preserved.
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.example.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  // <A>Google Reader</A> was skipped for lack of URL.
}

// Tests importing invalid files that do not exist.
TEST_F(StablePortabilityDataImporterTest, CallbacksAreCalled) {
  ImportBookmarksFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/bookmarks/file")));

  ImportReadingListFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/reading_list/file")));
}

}  // namespace user_data_importer
