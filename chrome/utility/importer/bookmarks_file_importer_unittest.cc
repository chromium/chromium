// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/bookmarks_file_importer.h"

#include <stddef.h>

#include <array>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace internal {

bool CanImportURL(const GURL& url);

}  // namespace internal

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;

class MockImporterBridge : public ImporterBridge {
 public:
  MockImporterBridge() {
    ON_CALL(*this, GetLocalizedString(_)).WillByDefault(testing::Return(u""));
  }

  MOCK_METHOD(void, NotifyStarted, (), (override));
  MOCK_METHOD(void,
              NotifyItemStarted,
              (user_data_importer::ImportItem item),
              (override));
  MOCK_METHOD(void,
              NotifyItemEnded,
              (user_data_importer::ImportItem item),
              (override));
  MOCK_METHOD(void, NotifyEnded, (), (override));
  MOCK_METHOD(std::u16string, GetLocalizedString, (int message_id), (override));
  MOCK_METHOD(void,
              AddBookmarks,
              (const std::vector<user_data_importer::ImportedBookmarkEntry>&,
               const std::u16string& top_level_folder_name),

              (override));
  MOCK_METHOD(void,
              SetKeywords,
              (const std::vector<user_data_importer::SearchEngineInfo>&, bool),
              (override));
  MOCK_METHOD(void,
              SetFavicons,
              (const favicon_base::FaviconUsageDataList&),
              (override));
  MOCK_METHOD(void, AddHomePage, (const GURL& home_page), (override));
  MOCK_METHOD(void,
              SetHistoryItems,
              (const std::vector<user_data_importer::ImporterURLRow>&,
               user_data_importer::VisitSource),
              (override));
  MOCK_METHOD(void,
              SetPasswordForm,
              (const user_data_importer::ImportedPasswordForm&),
              (override));
  MOCK_METHOD(void,
              SetAutofillFormData,
              (const std::vector<ImporterAutofillFormDataEntry>&),
              (override));

 protected:
  ~MockImporterBridge() override = default;
};


TEST(BookmarksFileImporterTest, CanImportURL) {
  struct TestCase {
    const std::string url;
    const bool can_be_imported;
  };
  auto test_cases = std::to_array<TestCase>({
      {"http://www.example.com", true},
      {"https://www.example.com", true},
      {"ftp://www.example.com", true},
      {"aim:GoIm?screenname=myscreenname&message=hello", true},
      {"chrome://version", true},
      {"chrome://chrome-urls", true},
      {"chrome://kill", true},
      {"chrome://about", true},
      {"about:version", true},
      {"about:blank", true},
      {"about:credits", true},
      {"wyciwyg://example.com", false},
      {"place://google.com", false},
      {"about:config", false},
      {"about:moon", false},
  });

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    EXPECT_EQ(test_cases[i].can_be_imported,
              internal::CanImportURL(GURL(test_cases[i].url)));
  }
}

TEST(BookmarksFileImporterTest, ImportBookmarks) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("bookmarks.html");

  std::string bookmarks_html = R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
  <META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">
  <TITLE>Bookmarks</TITLE>
  <H1>Bookmarks</H1>
  <DL><p>
      <DT><H3 ADD_DATE="123">Folder</H3>
      <DL><p>
          <DT><A HREF="https://www.google.com/" ADD_DATE="456">Google</A>
      </DL><p>
  </DL><p>
  )";
  ASSERT_TRUE(base::WriteFile(file_path, bookmarks_html));
  scoped_refptr<BookmarksFileImporter> importer = new BookmarksFileImporter();
  auto bridge = base::MakeRefCounted<MockImporterBridge>();

  user_data_importer::SourceProfile source_profile;
  source_profile.source_path = file_path;

  std::vector<user_data_importer::ImportedBookmarkEntry> expected_bookmarks;
  user_data_importer::ImportedBookmarkEntry bookmark;
  bookmark.path = {u"Folder"};
  bookmark.title = u"Google";
  bookmark.url = GURL("https://www.google.com/");
  bookmark.creation_time = base::Time::UnixEpoch() + base::Seconds(456);
  expected_bookmarks.push_back(bookmark);

  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyEnded());
  EXPECT_CALL(*bridge, AddBookmarks(expected_bookmarks, _));

  importer->StartImport(source_profile, user_data_importer::FAVORITES,
                        bridge.get());
}

TEST(BookmarksFileImporterTest, ImportEmptyFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("bookmarks.html");
  ASSERT_TRUE(base::WriteFile(file_path, ""));

  scoped_refptr<BookmarksFileImporter> importer = new BookmarksFileImporter();
  auto bridge = base::MakeRefCounted<MockImporterBridge>();

  user_data_importer::SourceProfile source_profile;
  source_profile.source_path = file_path;

  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyEnded());
  EXPECT_CALL(*bridge, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*bridge, SetKeywords(_, _)).Times(0);
  EXPECT_CALL(*bridge, SetFavicons(_)).Times(0);

  importer->StartImport(source_profile, user_data_importer::FAVORITES,
                        bridge.get());
}

TEST(BookmarksFileImporterTest, ImportWithInvalidBookmarks) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("bookmarks.html");

  std::string bookmarks_html =
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
      "<DL><p>\n"
      "  <DT><A HREF=\"http://www.google.com/\" ADD_DATE=\"123\">Google</A>\n"
      "  <DT><A HREF=\"wyciwyg://invalid\" ADD_DATE=\"456\">Invalid</A>\n"
      "</DL><p>\n";
  ASSERT_TRUE(base::WriteFile(file_path, bookmarks_html));

  scoped_refptr<BookmarksFileImporter> importer = new BookmarksFileImporter();
  auto bridge = base::MakeRefCounted<MockImporterBridge>();

  user_data_importer::SourceProfile source_profile;
  source_profile.source_path = file_path;

  std::vector<user_data_importer::ImportedBookmarkEntry> expected_bookmarks;
  user_data_importer::ImportedBookmarkEntry bookmark;
  bookmark.title = u"Google";
  bookmark.url = GURL("http://www.google.com/");
  bookmark.creation_time = base::Time::UnixEpoch() + base::Seconds(123);
  expected_bookmarks.push_back(bookmark);

  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyEnded());
  EXPECT_CALL(*bridge, AddBookmarks(expected_bookmarks, _));

  importer->StartImport(source_profile, user_data_importer::FAVORITES,
                        bridge.get());
}

TEST(BookmarksFileImporterTest, ImportSearchEngine) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("bookmarks.html");

  std::string bookmarks_html =
      "<DT><A HREF=\"http://www.google.com/search?q=%s\" "
      "SHORTCUTURL=\"g\">Google Search</A>";
  ASSERT_TRUE(base::WriteFile(file_path, bookmarks_html));

  scoped_refptr<BookmarksFileImporter> importer = new BookmarksFileImporter();
  auto bridge = base::MakeRefCounted<MockImporterBridge>();

  user_data_importer::SourceProfile source_profile;
  source_profile.source_path = file_path;

  std::vector<user_data_importer::SearchEngineInfo> expected_search_engines;
  user_data_importer::SearchEngineInfo search_engine;
  search_engine.url = u"http://www.google.com/search?q={searchTerms}";
  search_engine.keyword = u"g";
  search_engine.display_name = u"Google Search";
  expected_search_engines.push_back(search_engine);

  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyEnded());
  EXPECT_CALL(*bridge, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*bridge, SetKeywords(expected_search_engines, false));

  importer->StartImport(source_profile, user_data_importer::FAVORITES,
                        bridge.get());
}

TEST(BookmarksFileImporterTest, ImportWithFavicon) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("bookmarks.html");

  std::string bookmarks_html =
      "<DT><A HREF=\"http://www.google.com/\" "
      "ICON=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFc"
      "SJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg==\">Google</A>";
  ASSERT_TRUE(base::WriteFile(file_path, bookmarks_html));

  scoped_refptr<BookmarksFileImporter> importer = new BookmarksFileImporter();
  auto bridge = base::MakeRefCounted<MockImporterBridge>();

  user_data_importer::SourceProfile source_profile;
  source_profile.source_path = file_path;

  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyEnded());
  EXPECT_CALL(*bridge,
              AddBookmarks(_, _));  // Expecting a bookmark to be added.
  EXPECT_CALL(
      *bridge,
      SetFavicons(ElementsAre(AllOf(
          Field(&favicon_base::FaviconUsageData::favicon_url,
                GURL("made-up-favicon:http://www.google.com/")),
          Field(&favicon_base::FaviconUsageData::urls,
                Contains(GURL("http://www.google.com/"))),
          Field(&favicon_base::FaviconUsageData::png_data, Not(IsEmpty()))))));

  importer->StartImport(source_profile, user_data_importer::FAVORITES,
                        bridge.get());
}
}  // namespace
