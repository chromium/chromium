// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser.h"

#include <stddef.h>

#include <array>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
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

TEST(ContentBookmarkParser, CanImportURLAsSearchEngineTest) {
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

namespace {

class ContentBookmarkParserWithData : public testing::Test {
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

  user_data_importer::ContentBookmarkParser* bookmark_parser() {
    return static_cast<user_data_importer::ContentBookmarkParser*>(
        bookmark_parser_.get());
  }

  base::FilePath test_data_path_;
  std::unique_ptr<BookmarkParser> bookmark_parser_ = MakeBookmarkParser();
  base::ScopedMockClockOverride clock;
};

void ContentBookmarkParserWithData::SetUp() {
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_path_));
  test_data_path_ = test_data_path_.AppendASCII(
      "components/test/data/content_bookmark_parser");
  CHECK(base::PathExists(test_data_path_));
}

void ContentBookmarkParserWithData::ExpectFirstFirefox2Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Empty", entry.title);
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(u"Empty's Parent", entry.path.front());
  }
}

void ContentBookmarkParserWithData::ExpectSecondFirefox2Bookmark(
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

void ContentBookmarkParserWithData::ExpectThirdFirefox2Bookmark(
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

void ContentBookmarkParserWithData::ExpectFirstEpiphanyBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
  EXPECT_EQ(0U, entry.path.size());
}

void ContentBookmarkParserWithData::ExpectSecondEpiphanyBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
  EXPECT_EQ(0U, entry.path.size());
}

void ContentBookmarkParserWithData::ExpectFirstFirefox23Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102167), entry.creation_time);
  EXPECT_EQ(0U, entry.path.size());
  EXPECT_EQ("https://www.google.com/", entry.url.spec());
}

void ContentBookmarkParserWithData::ExpectSecondFirefox23Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Issues", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102304), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Chromium", entry.path.front());
  EXPECT_EQ("https://code.google.com/p/chromium/issues/list", entry.url.spec());
}

void ContentBookmarkParserWithData::ExpectThirdFirefox23Bookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"CodeSearch", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102224), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Chromium", entry.path.front());
  EXPECT_EQ("http://code.google.com/p/chromium/codesearch", entry.url.spec());
}

void ContentBookmarkParserWithData::ExpectFirstFirefoxBookmarkWithKeyword(
    const user_data_importer::SearchEngineInfo& info) {
  EXPECT_EQ(u"http://example.{searchTerms}.com/", info.url);
  EXPECT_EQ(u"keyword", info.keyword);
  EXPECT_EQ(u"Bookmark Keyword", info.display_name);
}

void ContentBookmarkParserWithData::ExpectSecondFirefoxBookmarkWithKeyword(
    const user_data_importer::SearchEngineInfo& info) {
  EXPECT_EQ(u"http://example.com/?q={searchTerms}", info.url);
  EXPECT_EQ(u"keyword", info.keyword);
  EXPECT_EQ(u"BookmarkName", info.display_name);
}

void ContentBookmarkParserWithData::ExpectFirstEmptyFolderBookmark(
    const user_data_importer::ImportedBookmarkEntry& entry) {
  EXPECT_EQ(std::u16string(), entry.title);
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1) {
    EXPECT_EQ(u"Empty's Parent", entry.path.front());
  }
}

void ContentBookmarkParserWithData::ExpectSecondEmptyFolderBookmark(
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

}  // namespace

TEST_F(ContentBookmarkParserWithData, Firefox2BookmarkFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  ExpectFirstFirefox2Bookmark(bookmarks[0]);
  ExpectSecondFirefox2Bookmark(bookmarks[1]);
  ExpectThirdFirefox2Bookmark(bookmarks[2]);
}

TEST_F(ContentBookmarkParserWithData, BookmarkFileWithHrTagImport) {
  base::FilePath path = test_data_path_.AppendASCII("firefox23.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  ExpectFirstFirefox23Bookmark(bookmarks[0]);
  ExpectSecondFirefox23Bookmark(bookmarks[1]);
  ExpectThirdFirefox23Bookmark(bookmarks[2]);
}

TEST_F(ContentBookmarkParserWithData, EpiphanyBookmarkFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("epiphany.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           &bookmarks, nullptr, nullptr);

  ASSERT_EQ(2U, bookmarks.size());
  ExpectFirstEpiphanyBookmark(bookmarks[0]);
  ExpectSecondEpiphanyBookmark(bookmarks[1]);
}

TEST_F(ContentBookmarkParserWithData, FirefoxBookmarkFileWithKeywordImport) {
  base::FilePath path =
      test_data_path_.AppendASCII("firefox_bookmark_keyword.html");

  std::vector<user_data_importer::SearchEngineInfo> search_engines;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           nullptr, &search_engines, nullptr);

  ASSERT_EQ(2U, search_engines.size());
  ExpectFirstFirefoxBookmarkWithKeyword(search_engines[0]);
  ExpectSecondFirefoxBookmarkWithKeyword(search_engines[1]);
}

TEST_F(ContentBookmarkParserWithData, EmptyFolderImport) {
  base::FilePath path = test_data_path_.AppendASCII("empty_folder.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  ExpectFirstEmptyFolderBookmark(bookmarks[0]);
  ExpectSecondEmptyFolderBookmark(bookmarks[1]);
  ExpectThirdFirefox2Bookmark(bookmarks[2]);
}

TEST_F(ContentBookmarkParserWithData, RedditSaverFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("redditsaver.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           &bookmarks, nullptr, nullptr);

  ASSERT_EQ(2U, bookmarks.size());
  EXPECT_EQ(u"Google", bookmarks[0].title);
  EXPECT_EQ(u"YouTube", bookmarks[1].title);
}

// Verifies that importing a bookmarks file without a charset specified succeeds
// (by falling back to a default charset). Per [ http://crbug.com/460423 ], this
// sort of bookmarks file is generated by IE.
TEST_F(ContentBookmarkParserWithData,
       InternetExplorerBookmarkFileWithoutCharsetImport) {
  base::FilePath path = test_data_path_.AppendASCII("ie_sans_charset.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::RepeatingCallback<bool(const GURL&)>(), path,
                           &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  EXPECT_EQ(u"Google", bookmarks[0].title);
  EXPECT_EQ(u"Outlook", bookmarks[1].title);
  EXPECT_EQ(u"Speed Test", bookmarks[2].title);
}

namespace {

class CancelAfterFifteenCalls {
  int count = 0;

 public:
  bool ShouldCancel() { return ++count > 16; }
};

}  // namespace

TEST_F(ContentBookmarkParserWithData, CancellationCallback) {
  // Use a file for testing that has multiple bookmarks.
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");

  CancelAfterFifteenCalls cancel_fifteen;
  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(
      base::BindRepeating(&CancelAfterFifteenCalls::ShouldCancel,
                          base::Unretained(&cancel_fifteen)),
      base::RepeatingCallback<bool(const GURL&)>(), path, &bookmarks, nullptr,
      nullptr);

  // The cancellation callback is checked before each line is read, so fifteen
  // lines are imported. The first fifteen lines of firefox2.html include only
  // one bookmark.
  ASSERT_EQ(1U, bookmarks.size());
  ExpectFirstFirefox2Bookmark(bookmarks[0]);
}

namespace {

bool IsURLValid(const GURL& url) {
  // No offense to whomever owns this domain...
  return !url.DomainIs("tamurayukari.com");
}

}  // namespace

TEST_F(ContentBookmarkParserWithData, ValidURLCallback) {
  // Use a file for testing that has multiple bookmarks.
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::BindRepeating(&IsURLValid), path, &bookmarks,
                           nullptr, nullptr);

  ASSERT_EQ(2U, bookmarks.size());
  ExpectFirstFirefox2Bookmark(bookmarks[0]);
  ExpectThirdFirefox2Bookmark(bookmarks[1]);
}

TEST_F(ContentBookmarkParserWithData, ToolbarFolder) {
  base::FilePath path = test_data_path_.AppendASCII("toolbar_folder.html");

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  bookmark_parser()->Parse(base::RepeatingCallback<bool(void)>(),
                           base::BindRepeating(&IsURLValid), path, &bookmarks,
                           nullptr, nullptr);

  // Only one bookmark since bookmarks with post data are ignored.
  ASSERT_EQ(1U, bookmarks.size());

  const user_data_importer::ImportedBookmarkEntry& entry = bookmarks[0];
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1212447159), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Toolbar folder", entry.path.front());
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
}

}  // namespace user_data_importer
