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

class StablePortabilityDataImporterTest : public testing::Test {
 public:
  StablePortabilityDataImporterTest() : receiver_(&fake_utility_parser_) {}

 protected:
  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    auto storage = std::make_unique<FakeReadingListModelStorage>();
    auto* storage_raw = storage.get();
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever,
        base::DefaultClock::GetInstance());
    storage_raw->TriggerLoadCompletion();
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

  void ImportHistory(const std::string& json_data) {
    history_callback_called_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("history.json");
    ASSERT_TRUE(base::WriteFile(path, json_data));
    ImportHistoryFile(path);
  }

  void ImportBookmarksFile(const base::FilePath& bookmarks_file) {
    PrepareCallbacks();

    // Note: Some tests use an invalid (non-existent) file name, so `file` might
    // be invalid here.
    base::File file(bookmarks_file,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);

    importer()->ImportBookmarks(
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

    importer()->ImportReadingList(
        std::move(file),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(
            &StablePortabilityDataImporterTest::OnReadingListConsumed,
            base::Unretained(this)));

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return reading_list_callback_called_; }));
  }

  void ImportHistoryFile(const base::FilePath& history_file) {
    PrepareCallbacks();

    const size_t default_batch_size = 10;
    importer()->ImportHistory(
        history_file,
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&StablePortabilityDataImporterTest::OnHistoryConsumed,
                       base::Unretained(this)),
        default_batch_size);

    ASSERT_TRUE(
        base::test::RunUntil([&]() { return history_callback_called_; }));
  }

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

  void InitializeHistoryService() { CreateImporterAndService(true); }

 private:
  StablePortabilityDataImporter* importer() {
    if (!importer_) {
      CreateImporterAndService(/*create_history_db=*/false);
    }
    return importer_.get();
  }

  // We need to perform a lazy initialization of the importer because the
  // history database is created lazily. This is necessary to ensure that each
  // test starts with a clean slate.
  void CreateImporterAndService(bool create_history_db) {
    history_service_ = history::CreateHistoryService(history_dir_.GetPath(),
                                                     create_history_db);
    mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser>
        pending_remote{receiver_.BindNewPipeAndPassRemote()};
    auto parser = base::MakeRefCounted<ContentBookmarkParser>();
    parser->SetServiceForTesting(std::move(pending_remote));
    importer_ = std::make_unique<StablePortabilityDataImporter>(
        history_service_.get(), bookmark_model_.get(),
        reading_list_model_.get(), std::move(parser));
  }

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

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER),
          ElementsAre(
              IsUrlBookmark(u"Google", GURL("https://www.google.com/"),
                            base::Time::FromSecondsSinceUnixEpoch(904914000)),
              // No timestamp maps to current time.
              IsUrlBookmark(u"Chromium", GURL("https://www.chromium.org/"),
                            base::Time::Now())))));

  EXPECT_EQ(GetReadingListModel().size(), 0u);
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

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          l10n_util::GetStringUTF16(IDS_IMPORTED_FOLDER),
          ElementsAre(
              IsUrlBookmark(u"Google", GURL("https://www.google.com/"),
                            base::Time::FromSecondsSinceUnixEpoch(904914000)),
              // No timestamp maps to current time.
              IsUrlBookmark(u"Chromium", GURL("https://www.chromium.org/"),
                            base::Time::Now())))));

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

  EXPECT_EQ(GetReadingListModel().size(), 3u);

  // "Imported" bookmarks folder should *not* have been created.
  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(other_node->children(), ElementsAre());

  EXPECT_THAT(
      GetReadingListModel().GetKeys(),
      UnorderedElementsAre(GURL("https://www.google.com/"),
                           GURL("https://en.wikipedia.org/wiki/The_Beach_Boys"),
                           GURL("https://en.wikipedia.org/wiki/Brian_Wilson")));

  const ReadingListEntry* entry1 =
      GetReadingListModel()
          .GetEntryByURL(GURL("https://www.google.com/"))
          .get();
  ASSERT_TRUE(entry1);
  EXPECT_EQ(entry1->Title(), "Google");
  // TODO(crbug.com/431203204): Implement actually importing the creation time.
  // Then the expectation should become
  // `base::Time::FromSecondsSinceUnixEpoch(904914000)`.
  EXPECT_EQ(
      base::Time::UnixEpoch() + base::Microseconds(entry1->CreationTime()),
      base::Time::Now());

  const ReadingListEntry* entry2 =
      GetReadingListModel()
          .GetEntryByURL(GURL("https://en.wikipedia.org/wiki/The_Beach_Boys"))
          .get();
  ASSERT_TRUE(entry2);
  EXPECT_EQ(entry2->Title(), "The Beach Boys");
  // No timestamp maps to current time.
  EXPECT_EQ(
      base::Time::UnixEpoch() + base::Microseconds(entry2->CreationTime()),
      base::Time::Now());

  const ReadingListEntry* entry3 =
      GetReadingListModel()
          .GetEntryByURL(GURL("https://en.wikipedia.org/wiki/Brian_Wilson"))
          .get();
  ASSERT_TRUE(entry3);
  EXPECT_EQ(entry3->Title(), "Brian Wilson");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(
      base::Time::UnixEpoch() + base::Microseconds(entry3->CreationTime()),
      base::Time::Now());
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
                           IsUrlBookmark(u"Chromium",
                                         GURL("https://www.chromium.org/"),
                                         // No timestamp maps to current time.
                                         base::Time::Now()),
                           IsUrlBookmark(
                               u"Example", GURL("https://www.example.org/"),
                               // Invalid timestamp maps to current time.
                               base::Time::Now())
                           // <A>Google Reader</A> was skipped for lack of URL.
                           ))))));
}

// Tests parsing a simple JSON file with two history entries.
TEST_F(StablePortabilityDataImporterTest, History_Basic) {
  InitializeHistoryService();
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

  history::QueryResults results = QueryAllHistory();
  ASSERT_EQ(results.size(), 2u);

  std::set<GURL> actual_urls;
  for (const auto& result : results) {
    actual_urls.insert(result.url());
  }
  EXPECT_THAT(actual_urls,
              UnorderedElementsAre(GURL("https://www.google.com/"),
                                   GURL("https://www.chromium.org/")));
}

// Tests parsing an invalid JSON file.
TEST_F(StablePortabilityDataImporterTest, History_InvalidJson) {
  InitializeHistoryService();
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
  EXPECT_EQ(GetNumberOfHistoryImported(), 0);
}

// Tests parsing a valid JSON file with no history entries.
TEST_F(StablePortabilityDataImporterTest, History_NoEntries) {
  InitializeHistoryService();
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
  InitializeHistoryService();
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

  history::QueryResults results = QueryAllHistory();
  ASSERT_EQ(results.size(), static_cast<size_t>(num_visits));

  std::set<GURL> actual_urls;
  for (int i = 0; i < num_visits; ++i) {
     actual_urls.insert(results[i].url());
  }

  std::set<GURL> expected_urls;
  for(int i = 0; i < num_visits; ++i){
    expected_urls.insert(GURL(absl::StrFormat("https://www.example.com/%d", i)));
  }
  EXPECT_EQ(actual_urls, expected_urls);
}

// Tests importing invalid files that do not exist.
TEST_F(StablePortabilityDataImporterTest, CallbacksAreCalled) {
  ImportBookmarksFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/bookmarks/file")));

  ImportReadingListFile(
      base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/reading_list/file")));
}

}  // namespace user_data_importer
