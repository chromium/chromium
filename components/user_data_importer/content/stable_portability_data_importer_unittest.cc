// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/stable_portability_data_importer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_test_util.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/bookmarks/test/test_matchers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_data_importer/content/content_bookmark_parser.h"
#include "components/user_data_importer/content/content_bookmark_parser_in_utility_process.h"
#include "components/user_data_importer/content/fake_bookmark_html_parser.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"
#include "components/user_data_importer/utility/parsing_ffi/lib.rs.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::test::IsFolder;
using bookmarks::test::IsUrlBookmark;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace user_data_importer {

const std::string kHistogramPrefix = "UserDataImporter.StablePortabilityData.";

MATCHER_P(URLResultEq, expected, "") {
  if (arg.url() != expected.url()) {
    *result_listener << "which has url " << arg.url();
    return false;
  }
  if (arg.title() != expected.title()) {
    *result_listener << "which has title " << arg.title();
    return false;
  }
  if (arg.visit_count() != expected.visit_count()) {
    *result_listener << "which has visit_count " << arg.visit_count();
    return false;
  }
  if (arg.typed_count() != expected.typed_count()) {
    *result_listener << "which has typed_count " << arg.typed_count();
    return false;
  }
  if (arg.last_visit() != expected.last_visit()) {
    *result_listener << "which has last_visit " << arg.last_visit();
    return false;
  }
  return true;
}

MATCHER_P4(IsUrlBookmarkWithinTimeRange, title, url, min_time, max_time, "") {
  if (!testing::ExplainMatchResult(IsUrlBookmark(title, url), arg,
                                   result_listener)) {
    return false;
  }
  if (arg->date_added() < min_time || arg->date_added() > max_time) {
    *result_listener << "which has creation time " << arg->date_added()
                     << " not in range [" << min_time << ", " << max_time
                     << "]";
    return false;
  }
  return true;
}

MATCHER_P3(IsReadingListEntry, title, url, creation_time, "") {
  if (arg->Title() != title) {
    *result_listener << "which has title " << arg->Title();
    return false;
  }
  if (arg->URL() != url) {
    *result_listener << "which has url " << arg->URL();
    return false;
  }
  base::Time actual_creation_time =
      base::Time::UnixEpoch() + base::Microseconds(arg->CreationTime());
  if (actual_creation_time != creation_time) {
    *result_listener << "which has creation time " << actual_creation_time;
    return false;
  }
  return true;
}

MATCHER_P4(IsReadingListEntryWithinTimeRange,
           title,
           url,
           min_time,
           max_time,
           "") {
  if (arg->Title() != title) {
    *result_listener << "which has title " << arg->Title();
    return false;
  }
  if (arg->URL() != url) {
    *result_listener << "which has url " << arg->URL();
    return false;
  }
  base::Time creation_time =
      base::Time::UnixEpoch() + base::Microseconds(arg->CreationTime());
  if (creation_time < min_time || creation_time > max_time) {
    *result_listener << "which has creation time " << creation_time
                     << " not in range [" << min_time << ", " << max_time
                     << "]";
    return false;
  }
  return true;
}

class StablePortabilityDataImporterTest : public testing::Test {
 public:
  StablePortabilityDataImporterTest() : receiver_(&fake_utility_parser_) {}

 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    auto parser = std::make_unique<ContentBookmarkParser>();
    parser->SetServiceForTesting(receiver_.BindNewPipeAndPassRemote());

    auto storage = std::make_unique<FakeReadingListModelStorage>();
    auto* storage_raw = storage.get();
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever,
        base::DefaultClock::GetInstance());
    storage_raw->TriggerLoadCompletion();

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);

    importer_ = std::make_unique<StablePortabilityDataImporter>(
        history_service_.get(), bookmark_model_.get(),
        reading_list_model_.get(), std::move(parser));
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  history::HistoryService* history_service() const {
    return history_service_.get();
  }

  int GetNumberOfBookmarksImported() const {
    return number_bookmarks_imported_;
  }

  int GetNumberOfReadingListImported() const {
    return number_reading_list_imported_;
  }

  int GetNumberOfHistoryImported() const { return number_history_imported_; }

  const bookmarks::BookmarkNode* GetOtherBookmarkNode() {
    return bookmark_model_->other_node();
  }

  const ReadingListModel& GetReadingListModel() { return *reading_list_model_; }

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

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  void ImportHistory(const std::string& json_data) {
    history_callback_called_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("history.json");
    ASSERT_TRUE(base::WriteFile(path, json_data));
    ImportHistoryFile(path);
  }
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

  void ImportBookmarksFile(const base::FilePath& bookmarks_file) {
    PrepareCallbacks();

    // Note: Some tests use an invalid (non-existent) file name, so `file` might
    // be invalid here.
    base::File file(bookmarks_file,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);

    importer_->ImportBookmarks(
        std::move(file),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&StablePortabilityDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)));

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return bookmarks_callback_called_; }));
  }

  void ImportReadingListFile(const base::FilePath& reading_list_file) {
    PrepareCallbacks();

    // Note: Some tests use an invalid (non-existent) file name, so `file` might
    // be invalid here.
    base::File file(reading_list_file,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);

    importer_->ImportReadingList(
        std::move(file),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(
            &StablePortabilityDataImporterTest::OnReadingListConsumed,
            base::Unretained(this)));

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return reading_list_callback_called_; }));
  }

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  void ImportHistoryFile(const base::FilePath& history_file) {
    PrepareCallbacks();

    // Note: Some tests use an invalid (non-existent) file name, so `file` might
    // be invalid here.
    base::File file(history_file,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);

    const size_t default_batch_size = 10;
    importer_->ImportHistory(
        std::move(file),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&StablePortabilityDataImporterTest::OnHistoryConsumed,
                       base::Unretained(this)),
        default_batch_size);

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return history_callback_called_; }));
  }
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

  history::QueryResults QueryAllHistory() {
    // Wait for any pending tasks to complete.
    base::RunLoop run_loop;
    history_service()->FlushForTest(run_loop.QuitClosure());
    run_loop.Run();

    base::test::TestFuture<history::QueryResults> future;
    base::CancelableTaskTracker tracker;
    history::QueryOptions options;
    history_service()->QueryHistory(std::u16string(), options,
                                    future.GetCallback(), &tracker);
    return future.Take();
  }

 private:
  void OnBookmarksConsumed(int number_imported) {
    bookmarks_callback_called_ = true;
    number_bookmarks_imported_ = number_imported;
  }

  void OnReadingListConsumed(int number_imported) {
    reading_list_callback_called_ = true;
    number_reading_list_imported_ = number_imported;
  }

  void OnHistoryConsumed(int number_imported) {
    history_callback_called_ = true;
    number_history_imported_ = number_imported;
  }

  void PrepareCallbacks() {
    bookmarks_callback_called_ = false;
    reading_list_callback_called_ = false;
    history_callback_called_ = false;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeBookmarkHtmlParser fake_utility_parser_;
  mojo::Receiver<user_data_importer::mojom::BookmarkHtmlParser> receiver_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<ReadingListModel> reading_list_model_;
  bool bookmarks_callback_called_ = false;
  bool reading_list_callback_called_ = false;
  bool history_callback_called_ = false;
  int number_bookmarks_imported_ = -1;
  int number_reading_list_imported_ = -1;
  int number_history_imported_ = -1;
  std::unique_ptr<StablePortabilityDataImporter> importer_;
};

// Tests parsing a simple HTML file with two bookmarks.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_Basic) {
  base::HistogramTester histogram_tester;

  const base::Time import_start_time = base::Time::Now();
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
  const base::Time import_end_time = base::Time::Now();

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER),
          ElementsAre(
              IsUrlBookmark(u"Google", GURL("https://www.google.com/"),
                            base::Time::FromSecondsSinceUnixEpoch(904914000)),
              // No timestamp maps to current time, within the import time.
              IsUrlBookmarkWithinTimeRange(
                  u"Chromium", GURL("https://www.chromium.org/"),
                  import_start_time, import_end_time)))));
  EXPECT_EQ(GetReadingListModel().size(), 0u);

  // Check that the histograms are recorded correctly.
  histogram_tester.ExpectUniqueSample(kHistogramPrefix + "Bookmarks.Outcome",
                                      DataTypeMetrics::ImportOutcome::kSuccess,
                                      1);
  histogram_tester.ExpectBucketCount(
      kHistogramPrefix + "Bookmarks.PreparedCount", 2, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramPrefix + "Bookmarks.ImportedCount", 2, 1);
}

// Identical to the above test, but without the top-level <DL> tag enclosing it.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_NoTopLevelDL) {
  const base::Time import_start_time = base::Time::Now();
  ImportBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>)");
  const base::Time import_end_time = base::Time::Now();
  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER),
          ElementsAre(
              IsUrlBookmark(u"Google", GURL("https://www.google.com/"),
                            base::Time::FromSecondsSinceUnixEpoch(904914000)),
              IsUrlBookmarkWithinTimeRange(
                  u"Chromium", GURL("https://www.chromium.org/"),
                  import_start_time, import_end_time)))));

  EXPECT_EQ(GetReadingListModel().size(), 0u);
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
  EXPECT_EQ(GetNumberOfBookmarksImported(), 3);

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER),
          ElementsAre(
              IsUrlBookmark(u"Google", GURL("https://www.google.com/"),
                            base::Time::FromSecondsSinceUnixEpoch(904914000)),
              IsFolder(
                  u"Folder 1",
                  ElementsAre(
                      IsUrlBookmark(
                          u"Example", GURL("https://www.example.com/"),
                          base::Time::FromSecondsSinceUnixEpoch(915181200)),
                      IsFolder(
                          u"Folder 1.1",
                          ElementsAre(IsUrlBookmark(
                              u"Kitsune",
                              GURL("https://en.wikipedia.org/wiki/Kitsune"),
                              base::Time::FromSecondsSinceUnixEpoch(
                                  1674205200)))))),
              IsFolder(u"Empty Folder", ElementsAre())))));

  EXPECT_EQ(GetReadingListModel().size(), 0u);
}

// Tests parsing a simple HTML file with three reading list items.
TEST_F(StablePortabilityDataImporterTest, ReadingList) {
  base::HistogramTester histogram_tester;
  const base::Time import_start_time = base::Time::Now();
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
  const base::Time import_end_time = base::Time::Now();
  EXPECT_EQ(GetNumberOfReadingListImported(), 3);

  EXPECT_EQ(GetReadingListModel().size(), 3u);

  // "Imported" bookmarks folder should *not* have been created.
  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(other_node->children(), ElementsAre());

  std::vector<const ReadingListEntry*> entries;
  for (const auto& gurl : GetReadingListModel().GetKeys()) {
    entries.push_back(GetReadingListModel().GetEntryByURL(gurl).get());
  }

  EXPECT_THAT(
      entries,
      UnorderedElementsAre(
          IsReadingListEntry("Google", GURL("https://www.google.com/"),
                             base::Time::FromSecondsSinceUnixEpoch(904914000)),
          IsReadingListEntryWithinTimeRange(
              "The Beach Boys",
              GURL("https://en.wikipedia.org/wiki/The_Beach_Boys"),
              import_start_time, import_end_time),
          IsReadingListEntryWithinTimeRange(
              "Brian Wilson",
              GURL("https://en.wikipedia.org/wiki/Brian_Wilson"),
              import_start_time, import_end_time)));

  // Check that the histograms are recorded correctly.
  histogram_tester.ExpectUniqueSample(kHistogramPrefix + "ReadingList.Outcome",
                                      DataTypeMetrics::ImportOutcome::kSuccess,
                                      1);
  histogram_tester.ExpectBucketCount(
      kHistogramPrefix + "ReadingList.PreparedCount", 3, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramPrefix + "ReadingList.ImportedCount", 3, 1);
}

// Tests parsing an HTML with several not valid formats. The parser should still
// try to parse as many items as possible.
TEST_F(StablePortabilityDataImporterTest, Bookmarks_MiscJunk) {
  const base::Time import_start_time = base::Time::Now();
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
  const base::Time import_end_time = base::Time::Now();
  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER),
          ElementsAre(
              // <A>Google</A> was skipped for lack of URL.
              IsFolder(u"Folder 1",
                       // The folder contains a mix of invalid and valid
                       // entries. Ensure the valid ones are preserved.
                       ElementsAre(
                           IsUrlBookmarkWithinTimeRange(
                               u"Chromium", GURL("https://www.chromium.org/"),
                               import_start_time, import_end_time),
                           IsUrlBookmarkWithinTimeRange(
                               u"Example", GURL("https://www.example.org/"),
                               import_start_time, import_end_time)

                           // <A>Google Reader</A> was skipped for lack of URL.
                           ))))));
}

TEST_F(StablePortabilityDataImporterTest, Bookmarks_EmptyInput) {
  base::HistogramTester histogram_tester;

  ImportBookmarks("");
  // Empty input data is considered an error. (In practice, this could also
  // happen if the file read fails for some reason.)
  EXPECT_EQ(GetNumberOfBookmarksImported(), -1);

  // Check that the histograms are recorded correctly.
  histogram_tester.ExpectUniqueSample(kHistogramPrefix + "Bookmarks.Outcome",
                                      DataTypeMetrics::ImportOutcome::kFailure,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kHistogramPrefix + "Bookmarks.Error",
      user_data_importer::BookmarksImportError::kFailedToRead, 1);
}

TEST_F(StablePortabilityDataImporterTest, ReadingList_EmptyInput) {
  base::HistogramTester histogram_tester;

  ImportReadingList("");
  // Empty input data is considered an error. (In practice, this could also
  // happen if the file read fails for some reason.)
  EXPECT_EQ(GetNumberOfReadingListImported(), -1);

  // Check that the histograms are recorded correctly.
  histogram_tester.ExpectUniqueSample(kHistogramPrefix + "ReadingList.Outcome",
                                      DataTypeMetrics::ImportOutcome::kFailure,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kHistogramPrefix + "ReadingList.Error",
      user_data_importer::BookmarksImportError::kFailedToRead, 1);
}

// History parsing is only implemented on Posix systems for now, because the
// file is passed to the Rust parser in the form of a native "fd" (file
// descriptor).
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// Tests parsing a simple JSON file with two history entries.
TEST_F(StablePortabilityDataImporterTest, History_Basic) {
  base::HistogramTester histogram_tester;

  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "history_visits"
    },
    "history_visits": [
      {
        "url": "https://www.google.com/",
        "title": "Google",
        "visit_time_unix_epoch_usec": 1674205200000000,
        "visit_count": 5,
        "typed_count": 2,
        "synced": true
      },
      {
        "url": "https://www.chromium.org/",
        "title": "Chromium",
        "visit_time_unix_epoch_usec": 1674205260000000
      }
    ]
  })";
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), 2);

  history::QueryResults query_results = QueryAllHistory();
  std::vector<history::URLResult> results(query_results.begin(),
                                          query_results.end());

  history::URLResult expected_row1;
  expected_row1.set_url(GURL("https://www.google.com/"));
  expected_row1.set_title(u"Google");
  expected_row1.set_visit_count(5);
  expected_row1.set_typed_count(2);
  expected_row1.set_last_visit(base::Time::UnixEpoch() +
                               base::Microseconds(1674205200000000));

  history::URLResult expected_row2;
  expected_row2.set_url(GURL("https://www.chromium.org/"));
  expected_row2.set_title(u"Chromium");
  expected_row2.set_visit_count(1);
  expected_row2.set_typed_count(0);
  expected_row2.set_last_visit(base::Time::UnixEpoch() +
                               base::Microseconds(1674205260000000));

  EXPECT_THAT(results, UnorderedElementsAre(URLResultEq(expected_row1),
                                            URLResultEq(expected_row2)));

  // Check that the histograms are recorded correctly.
  histogram_tester.ExpectUniqueSample(kHistogramPrefix + "History.Outcome",
                                      DataTypeMetrics::ImportOutcome::kSuccess,
                                      1);
  histogram_tester.ExpectBucketCount(kHistogramPrefix + "History.ImportedCount",
                                     2, 1);
}

TEST_F(StablePortabilityDataImporterTest, History_InvalidJson_Malformed) {
  base::HistogramTester histogram_tester;

  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "history_visits"
    },
    "history_visits": [
      {
        "url": "https://www.google.com/",
        "title": "Google",
  })";  // Invalid JSON, missing closing brackets.
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);

  histogram_tester.ExpectUniqueSample(kHistogramPrefix + "History.Outcome",
                                      DataTypeMetrics::ImportOutcome::kFailure,
                                      1);
}

TEST_F(StablePortabilityDataImporterTest, History_InvalidJson_NotJson) {
  const char kHistoryData[] = "This is not a JSON file.";
  ImportHistory(kHistoryData);
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);
}

TEST_F(StablePortabilityDataImporterTest,
       History_InvalidJson_MissingHistoryVisits) {
  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "history_visits"
    }
  })";
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);
}

TEST_F(StablePortabilityDataImporterTest,
       History_InvalidJson_HistoryVisitsNotArray) {
  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "history_visits"
    },
    "history_visits": {}
  })";
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);
}

TEST_F(StablePortabilityDataImporterTest, History_InvalidJson_WrongDataType) {
  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "bookmarks"
    },
    "history_visits": []
  })";
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);
}

TEST_F(StablePortabilityDataImporterTest, History_EmptyInput) {
  ImportHistory("");
  // Empty input data is considered an error.
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);
}

// Tests parsing a valid JSON file with no history entries.
TEST_F(StablePortabilityDataImporterTest, History_NoEntries) {
  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "history_visits"
    },
    "history_visits": []
  })";
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), 0);
}

// Tests parsing a large JSON file that is processed in chunks.
TEST_F(StablePortabilityDataImporterTest, History_LargeFileInChunks) {
  const int num_visits = 15;
  std::vector<std::string> visits;
  for (int i = 0; i < num_visits; ++i) {
    // The chunk size is 10, so 15 items will require two chunks.
    visits.push_back(absl::StrFormat(
        R"({"url":"https://www.example.com/%d","title":"Title %d",)"
        R"("visit_time_unix_epoch_usec":%d})",
        i, i, 1674205200000000ULL + i));
  }
  std::string history_json =
      R"({"metadata":{"data_type":"history_visits"},"history_visits":[)" +
      base::JoinString(visits, ",") + "]}";

  ImportHistory(history_json);
  EXPECT_EQ(GetNumberOfHistoryImported(), num_visits);

  history::QueryResults query_results = QueryAllHistory();
  std::vector<history::URLResult> results(query_results.begin(),
                                          query_results.end());

  std::vector<testing::Matcher<history::URLResult>> matchers;
  for (int i = 0; i < num_visits; ++i) {
    history::URLResult expected_row;
    expected_row.set_url(
        GURL(absl::StrFormat("https://www.example.com/%d", i)));
    expected_row.set_title(base::UTF8ToUTF16(absl::StrFormat("Title %d", i)));
    expected_row.set_visit_count(1);
    expected_row.set_typed_count(0);
    expected_row.set_last_visit(base::Time::UnixEpoch() +
                                base::Microseconds(1674205200000000ULL + i));
    matchers.push_back(URLResultEq(expected_row));
  }
  EXPECT_THAT(results, UnorderedElementsAreArray(matchers));
}

// Tests parsing a JSON file with a mix of valid and invalid entries. Only
// valid entries should be imported.
TEST_F(StablePortabilityDataImporterTest, History_MixedValidAndInvalid) {
  const char kHistoryJson[] = R"({
    "metadata": {
      "data_type": "history_visits"
    },
    "history_visits": [
      {
        "url": "https://www.google.com/",
        "title": "Google",
        "visit_time_unix_epoch_usec": 1674205200000000
      },
      {
        "title": "Invalid Entry, no URL"
      },
      {
        "url": "https://www.chromium.org/",
        "title": "Chromium",
        "visit_time_unix_epoch_usec": 1674205260000000
      }
    ]
  })";
  ImportHistory(kHistoryJson);
  EXPECT_EQ(GetNumberOfHistoryImported(), 2);

  history::QueryResults query_results = QueryAllHistory();
  std::vector<history::URLResult> results(query_results.begin(),
                                          query_results.end());

  history::URLResult expected_row1;
  expected_row1.set_url(GURL("https://www.google.com/"));
  expected_row1.set_title(u"Google");
  expected_row1.set_visit_count(1);
  expected_row1.set_typed_count(0);
  expected_row1.set_last_visit(base::Time::UnixEpoch() +
                               base::Microseconds(1674205200000000));

  history::URLResult expected_row2;
  expected_row2.set_url(GURL("https://www.chromium.org/"));
  expected_row2.set_title(u"Chromium");
  expected_row2.set_visit_count(1);
  expected_row2.set_typed_count(0);
  expected_row2.set_last_visit(base::Time::UnixEpoch() +
                               base::Microseconds(1674205260000000));

  EXPECT_THAT(results, UnorderedElementsAre(URLResultEq(expected_row1),
                                            URLResultEq(expected_row2)));
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

// Tests importing invalid files that do not exist.
TEST_F(StablePortabilityDataImporterTest, CallbacksAreCalled) {
  base::HistogramTester histogram_tester;
  ImportBookmarksFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/bookmarks/file")));
  EXPECT_EQ(GetNumberOfBookmarksImported(), -1);
  histogram_tester.ExpectUniqueSample(
      kHistogramPrefix + "Bookmarks.Outcome",
      DataTypeMetrics::ImportOutcome::kNotPresent, 1);

  ImportReadingListFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/reading_list/file")));
  EXPECT_EQ(GetNumberOfReadingListImported(), -1);
  histogram_tester.ExpectUniqueSample(
      kHistogramPrefix + "ReadingList.Outcome",
      DataTypeMetrics::ImportOutcome::kNotPresent, 1);

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  ImportHistoryFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/history/file")));
  EXPECT_EQ(GetNumberOfHistoryImported(), -1);

  histogram_tester.ExpectUniqueSample(
      kHistogramPrefix + "History.Outcome",
      DataTypeMetrics::ImportOutcome::kNotPresent, 1);
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace user_data_importer
