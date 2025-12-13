// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser_utils.h"

#include <stddef.h>

#include <array>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF16ToWide;

namespace user_data_importer {

TEST(ContentBookmarkParserUtilsTest, CanImportURLAsSearchEngineTest) {
  struct TestCase {
    const std::string url;
    const bool can_be_imported_as_search_engine;
    const std::string expected_search_engine_url;
  };
  auto test_cases = std::to_array<TestCase>({
      {"http://www.example.%s.com", true,
       "http://www.example.{searchTerms}.com/"},
      {"http://www.example.%S.com", true,
       "http://www.example.{searchTerms}.com/"},
      {"http://www.example.%x.com", false, "http://www.example.%x.com/"},
      {"http://www.example.com", false, ""},
      {"http://%s.example.com", true, "http://{searchTerms}.example.com/"},
      {"http://www.example.%s.test.%s.com", true,
       "http://www.example.{searchTerms}.test.{searchTerms}.com/"},
      // Illegal characters in the host get escaped.
      {"http://www.test&test.%s.com", true,
       "http://www.test&test.{searchTerms}.com/"},
      {"http://www.example.com?q=%s&foo=bar", true,
       "http://www.example.com/?q={searchTerms}&foo=bar"},
      {"http://www.example.com/%s/?q=%s&foo=bar", true,
       "http://www.example.com/{searchTerms}/?q={searchTerms}&foo=bar"},
      {"http//google.com", false, ""},
      {"path", false, ""},
      {"http:/path/%s/", true, "http://path/{searchTerms}/"},
      {"path", false, ""},
      {"", false, ""},
      // Cases with other percent-encoded characters.
      {"http://www.example.com/search%3Fpage?q=%s&v=foo%26bar", true,
       "http://www.example.com/search%3Fpage?q={searchTerms}&v=foo%26bar"},
      // Encoded percent symbol.
      {"http://www.example.com/search?q=%s&v=foo%25bar", true,
       "http://www.example.com/search?q={searchTerms}&v=foo%25bar"},
      // Literal "%s", escaped so as to not represent a replacement slot.
      // Note: This is buggy due to the fact that the GURL constructor doesn't
      // distinguish "%s" from "%25s", and can't be fixed without changing the
      // interface to this function. https://crbug.com/868214.
      {"http://www.example.com/search?q=%s&v=pepper%25salt", true,
       "http://www.example.com/"
       "search?q={searchTerms}&v=pepper{searchTerms}alt"},
      // Encoded Unicode character (U+2014).
      {"http://www.example.com/search?q=%s&v=foo%E2%80%94bar", true,
       "http://www.example.com/search?q={searchTerms}&v=foo%E2%80%94bar"},
      // Non-encoded Unicode character (U+2014) (should be auto-encoded).
      {"http://www.example.com/search?q=%s&v=foo—bar", true,
       "http://www.example.com/search?q={searchTerms}&v=foo%E2%80%94bar"},
      // Invalid characters that should be auto-encoded.
      {"http://www.example.com/{search}?q=%s", true,
       "http://www.example.com/%7Bsearch%7D?q={searchTerms}"},
  });

  std::string search_engine_url;
  for (TestCase test_case : test_cases) {
    EXPECT_EQ(
        test_case.can_be_imported_as_search_engine,
        CanImportURLAsSearchEngine(GURL(test_case.url), &search_engine_url));
    if (test_case.can_be_imported_as_search_engine) {
      EXPECT_EQ(test_case.expected_search_engine_url, search_engine_url);
    }
  }
}

class ContentBookmarkParserUtilsWithDataTest : public testing::Test {
 public:
  void SetUp() override;

 protected:
  void ExpectFirstFirefox2Bookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectSecondFirefox2Bookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectThirdFirefox2Bookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectFirstEpiphanyBookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectSecondEpiphanyBookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectFirstFirefox23Bookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectSecondFirefox23Bookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectThirdFirefox23Bookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectFirstFirefoxBookmarkWithKeyword(
      const user_data_importer::SearchEngineInfo& info);
  void ExpectSecondFirefoxBookmarkWithKeyword(
      const user_data_importer::SearchEngineInfo& info);
  void ExpectFirstEmptyFolderBookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);
  void ExpectSecondEmptyFolderBookmark(
      const user_data_importer::ImportedBookmarkEntry& entry);

  base::FilePath test_data_path_;
  base::ScopedMockClockOverride clock;
};

void ContentBookmarkParserUtilsWithDataTest::SetUp() {
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_path_));
  test_data_path_ = test_data_path_.AppendASCII(
      "components/test/data/content_bookmark_parser");
  CHECK(base::PathExists(test_data_path_));
}

void ContentBookmarkParserUtilsWithDataTest::ExpectFirstFirefox2Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Empty", entry.title);
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(u"Empty's Parent", entry.path.front());
  }
}

void ContentBookmarkParserUtilsWithDataTest::ExpectSecondFirefox2Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(u"Not Empty", entry.path.front());
  }
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
}

void ContentBookmarkParserUtilsWithDataTest::ExpectThirdFirefox2Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.creation_time, clock.Now());
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(u"Not Empty But Default", entry.path.front());
  }
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
}

void ContentBookmarkParserUtilsWithDataTest::ExpectFirstEpiphanyBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
  EXPECT_EQ(0U, entry.path.size());
}

void ContentBookmarkParserUtilsWithDataTest::ExpectSecondEpiphanyBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
  EXPECT_EQ(0U, entry.path.size());
}

void ContentBookmarkParserUtilsWithDataTest::ExpectFirstFirefox23Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102167), entry.creation_time);
  EXPECT_FALSE(entry.last_visit_time.has_value());
  EXPECT_EQ(0U, entry.path.size());
  EXPECT_EQ("https://www.google.com/", entry.url.spec());
}

void ContentBookmarkParserUtilsWithDataTest::ExpectSecondFirefox23Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Issues", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102304), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Chromium", entry.path.front());
  EXPECT_EQ("https://code.google.com/p/chromium/issues/list", entry.url.spec());
}

void ContentBookmarkParserUtilsWithDataTest::ExpectThirdFirefox23Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"CodeSearch", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102224), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Chromium", entry.path.front());
  EXPECT_EQ("http://code.google.com/p/chromium/codesearch", entry.url.spec());
}

void ContentBookmarkParserUtilsWithDataTest::
    ExpectFirstFirefoxBookmarkWithKeyword(
        const user_data_importer::SearchEngineInfo& info) {
  EXPECT_EQ(u"http://example.{searchTerms}.com/", info.url);
  EXPECT_EQ(u"keyword", info.keyword);
  EXPECT_EQ(u"Bookmark Keyword", info.display_name);
}

void ContentBookmarkParserUtilsWithDataTest::
    ExpectSecondFirefoxBookmarkWithKeyword(
        const user_data_importer::SearchEngineInfo& info) {
  EXPECT_EQ(u"http://example.com/?q={searchTerms}", info.url);
  EXPECT_EQ(u"keyword", info.keyword);
  EXPECT_EQ(u"BookmarkName", info.display_name);
}

void ContentBookmarkParserUtilsWithDataTest::ExpectFirstEmptyFolderBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(std::u16string(), entry.title);
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(u"Empty's Parent", entry.path.front());
  }
}

void ContentBookmarkParserUtilsWithDataTest::ExpectSecondEmptyFolderBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(std::u16string(), entry.path.front());
  }
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, Firefox2BookmarkFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(3U, result.bookmarks.size());
  ExpectFirstFirefox2Bookmark(result.bookmarks[0]);
  ExpectSecondFirefox2Bookmark(result.bookmarks[1]);
  ExpectThirdFirefox2Bookmark(result.bookmarks[2]);
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, BookmarkFileWithHrTagImport) {
  base::FilePath path = test_data_path_.AppendASCII("firefox23.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(3U, result.bookmarks.size());
  ExpectFirstFirefox23Bookmark(result.bookmarks[0]);
  ExpectSecondFirefox23Bookmark(result.bookmarks[1]);
  ExpectThirdFirefox23Bookmark(result.bookmarks[2]);
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, EpiphanyBookmarkFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("epiphany.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(2U, result.bookmarks.size());
  ExpectFirstEpiphanyBookmark(result.bookmarks[0]);
  ExpectSecondEpiphanyBookmark(result.bookmarks[1]);
}

TEST_F(ContentBookmarkParserUtilsWithDataTest,
       FirefoxBookmarkFileWithKeywordImport) {
  base::FilePath path =
      test_data_path_.AppendASCII("firefox_bookmark_keyword.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(2U, result.search_engines.size());
  ExpectFirstFirefoxBookmarkWithKeyword(result.search_engines[0]);
  ExpectSecondFirefoxBookmarkWithKeyword(result.search_engines[1]);
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, EmptyFolderImport) {
  base::FilePath path = test_data_path_.AppendASCII("empty_folder.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(3U, result.bookmarks.size());
  ExpectFirstEmptyFolderBookmark(result.bookmarks[0]);
  ExpectSecondEmptyFolderBookmark(result.bookmarks[1]);
  ExpectThirdFirefox2Bookmark(result.bookmarks[2]);
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, RedditSaverFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("redditsaver.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(2U, result.bookmarks.size());
  EXPECT_EQ(u"Google", result.bookmarks[0].title);
  EXPECT_EQ(u"YouTube", result.bookmarks[1].title);
}

// Verifies that importing a bookmarks file without a charset specified succeeds
// (by falling back to a default charset). Per [ http://crbug.com/460423 ], this
// sort of bookmarks file is generated by IE.
TEST_F(ContentBookmarkParserUtilsWithDataTest,
       InternetExplorerBookmarkFileWithoutCharsetImport) {
  base::FilePath path = test_data_path_.AppendASCII("ie_sans_charset.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(3U, result.bookmarks.size());
  EXPECT_EQ(u"Google", result.bookmarks[0].title);
  EXPECT_EQ(u"Outlook", result.bookmarks[1].title);
  EXPECT_EQ(u"Speed Test", result.bookmarks[2].title);
  EXPECT_EQ(base::Time::FromTimeT(1424779391),
            result.bookmarks[0].last_visit_time);
  EXPECT_EQ(base::Time::FromTimeT(1424779398),
            result.bookmarks[1].last_visit_time);
  EXPECT_EQ(base::Time::FromTimeT(1424779406),
            result.bookmarks[2].last_visit_time);
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, ToolbarFolder) {
  base::FilePath path = test_data_path_.AppendASCII("toolbar_folder.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  // Only one bookmark since bookmarks with post data are ignored.
  ASSERT_EQ(1U, result.bookmarks.size());

  const user_data_importer::ImportedBookmarkEntry& entry = result.bookmarks[0];
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1212447159), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Toolbar folder", entry.path.front());
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
}

TEST_F(ContentBookmarkParserUtilsWithDataTest, UuidAndSyncedImport) {
  base::FilePath path = test_data_path_.AppendASCII("uuid_and_synced.html");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(path, &content));
  BookmarkParser::ParsedBookmarks result = ParseBookmarksUnsafe(content);

  ASSERT_EQ(6U, result.bookmarks.size());

  // 1. UUID and SYNCED="1".
  const auto& bm1 = result.bookmarks[0];
  EXPECT_EQ(u"Google", bm1.title);
  EXPECT_TRUE(bm1.uuid.has_value());
  EXPECT_EQ(
      base::Uuid::ParseCaseInsensitive("B64522A7-222E-4553-986C-85F837E6B229"),
      bm1.uuid);
  EXPECT_TRUE(bm1.synced.has_value());
  EXPECT_TRUE(bm1.synced.value());

  // 2. UUID and SYNCED="0".
  const auto& bm2 = result.bookmarks[1];
  EXPECT_EQ(u"Chromium", bm2.title);
  EXPECT_TRUE(bm2.uuid.has_value());
  EXPECT_EQ(
      base::Uuid::ParseCaseInsensitive("A64522A7-222E-4553-986C-85F837E6B221"),
      bm2.uuid);
  EXPECT_TRUE(bm2.synced.has_value());
  EXPECT_FALSE(bm2.synced.value());

  // 3. No optional attributes.
  const auto& bm3 = result.bookmarks[2];
  EXPECT_EQ(u"Example", bm3.title);
  EXPECT_FALSE(bm3.uuid.has_value());
  EXPECT_FALSE(bm3.synced.has_value());

  // 4. Invalid UUID.
  const auto& bm4 = result.bookmarks[3];
  EXPECT_EQ(u"Invalid", bm4.title);
  EXPECT_FALSE(bm4.uuid.has_value());
  EXPECT_FALSE(bm4.synced.has_value());

  // 5. SYNCED with non-bool value.
  const auto& bm5 = result.bookmarks[4];
  EXPECT_EQ(u"Not a bool", bm5.title);
  EXPECT_FALSE(bm5.uuid.has_value());
  EXPECT_FALSE(bm5.synced.has_value());

  // 6. Folder with UUID and SYNCED="1".
  const auto& bm6 = result.bookmarks[5];
  EXPECT_TRUE(bm6.is_folder);
  EXPECT_EQ(u"Synced Folder", bm6.title);
  EXPECT_TRUE(bm6.uuid.has_value());
  EXPECT_EQ(
      base::Uuid::ParseCaseInsensitive("C64522A7-222E-4553-986C-85F837E6B229"),
      bm6.uuid);
  EXPECT_TRUE(bm6.synced.has_value());
  EXPECT_TRUE(bm6.synced.value());
}

}  // namespace user_data_importer
