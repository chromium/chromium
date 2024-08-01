// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/utility/importer/bookmark_html_reader.h"

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF16ToWide;

namespace bookmark_html_reader {

TEST(BookmarkHTMLReaderTest, ParseTests) {
  bool result;

  // Tests charset.
  std::string charset;
  result = internal::ParseCharsetFromLine(
      "<META HTTP-EQUIV=\"Content-Type\" "
      "CONTENT=\"text/html; charset=UTF-8\">",
      &charset);
  EXPECT_TRUE(result);
  EXPECT_EQ("UTF-8", charset);

  // Escaped characters in name.
  std::u16string folder_name;
  bool is_toolbar_folder;
  base::Time folder_add_date;
  result = internal::ParseFolderNameFromLine(
      "<DT><H3 ADD_DATE=\"1207558707\" >&lt; &gt;"
      " &amp; &quot; &#39; \\ /</H3>",
      charset, &folder_name, &is_toolbar_folder, &folder_add_date);
  EXPECT_TRUE(result);
  EXPECT_EQ(u"< > & \" ' \\ /", folder_name);
  EXPECT_FALSE(is_toolbar_folder);
  EXPECT_TRUE(base::Time::FromTimeT(1207558707) == folder_add_date);

  // Empty name and toolbar folder attribute.
  result = internal::ParseFolderNameFromLine(
      "<DT><H3 PERSONAL_TOOLBAR_FOLDER=\"true\"></H3>",
      charset, &folder_name, &is_toolbar_folder, &folder_add_date);
  EXPECT_TRUE(result);
  EXPECT_EQ(std::u16string(), folder_name);
  EXPECT_TRUE(is_toolbar_folder);

  // Unicode characters in title and shortcut.
  std::u16string title;
  GURL url, favicon;
  std::u16string shortcut;
  std::u16string post_data;
  base::Time add_date;
  result = internal::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://chinese.site.cn/path?query=1#ref\" "
      "SHORTCUTURL=\"\xE4\xB8\xAD\">\xE4\xB8\xAD\xE6\x96\x87</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(L"\x4E2D\x6587", UTF16ToWide(title));
  EXPECT_EQ("http://chinese.site.cn/path?query=1#ref", url.spec());
  EXPECT_EQ(L"\x4E2D", UTF16ToWide(shortcut));
  EXPECT_EQ(std::u16string(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // No shortcut, and url contains %22 ('"' character).
  result = internal::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?q=%22<>%22\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(u"name", title);
  EXPECT_EQ("http://domain.com/?q=%22%3C%3E%22", url.spec());
  EXPECT_EQ(std::u16string(), shortcut);
  EXPECT_EQ(std::u16string(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  result = internal::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?g=&quot;\"\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(u"name", title);
  EXPECT_EQ("http://domain.com/?g=%22", url.spec());
  EXPECT_EQ(std::u16string(), shortcut);
  EXPECT_EQ(std::u16string(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // Creation date.
  result = internal::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://site/\" ADD_DATE=\"1121301154\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(u"name", title);
  EXPECT_EQ(GURL("http://site/"), url);
  EXPECT_EQ(std::u16string(), shortcut);
  EXPECT_EQ(std::u16string(), post_data);
  EXPECT_TRUE(base::Time::FromTimeT(1121301154) == add_date);

  // Post-data
  result = internal::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://localhost:8080/test/hello.html\" ADD_DATE=\""
      "1212447159\" LAST_VISIT=\"1212447251\" LAST_MODIFIED=\"1212447248\""
      "SHORTCUTURL=\"post\" ICON=\"data:\" POST_DATA=\"lname%3D%25s\""
      "LAST_CHARSET=\"UTF-8\" ID=\"rdf:#$weKaR3\">Test Post keyword</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(u"Test Post keyword", title);
  EXPECT_EQ("http://localhost:8080/test/hello.html", url.spec());
  EXPECT_EQ(u"post", shortcut);
  EXPECT_EQ(u"lname%3D%25s", post_data);
  EXPECT_TRUE(base::Time::FromTimeT(1212447159) == add_date);

  // Invalid case.
  result = internal::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?q=%22",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(std::u16string(), title);
  EXPECT_EQ("", url.spec());
  EXPECT_EQ(std::u16string(), shortcut);
  EXPECT_EQ(std::u16string(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // Epiphany format.
  result = internal::ParseMinimumBookmarkFromLine(
      "<dt><a href=\"http://www.google.com/\">Google</a></dt>",
      charset, &title, &url);
  EXPECT_TRUE(result);
  EXPECT_EQ(u"Google", title);
  EXPECT_EQ("http://www.google.com/", url.spec());
}

TEST(BookmarkHTMLReaderTest, CanImportURLAsSearchEngineTest) {
  struct TestCase {
    const std::string url;
    const bool can_be_imported_as_search_engine;
    const std::string expected_search_engine_url;
  } test_cases[] = {
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
  };

  std::string search_engine_url;
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    EXPECT_EQ(test_cases[i].can_be_imported_as_search_engine,
        CanImportURLAsSearchEngine(GURL(test_cases[i].url),
                                   &search_engine_url));
    if (test_cases[i].can_be_imported_as_search_engine)
      EXPECT_EQ(test_cases[i].expected_search_engine_url, search_engine_url);
  }
}

namespace {

class BookmarkHTMLReaderTestWithData : public testing::Test {
 public:
  void SetUp() override;

 protected:
  void ExpectFirstFirefox2Bookmark(const ImportedBookmarkEntry& entry);
  void ExpectSecondFirefox2Bookmark(const ImportedBookmarkEntry& entry);
  void ExpectThirdFirefox2Bookmark(const ImportedBookmarkEntry& entry);
  void ExpectFirstEpiphanyBookmark(const ImportedBookmarkEntry& entry);
  void ExpectSecondEpiphanyBookmark(const ImportedBookmarkEntry& entry);
  void ExpectFirstFirefox23Bookmark(const ImportedBookmarkEntry& entry);
  void ExpectSecondFirefox23Bookmark(const ImportedBookmarkEntry& entry);
  void ExpectThirdFirefox23Bookmark(const ImportedBookmarkEntry& entry);
  void ExpectFirstFirefoxBookmarkWithKeyword(
      const importer::SearchEngineInfo& info);
  void ExpectSecondFirefoxBookmarkWithKeyword(
      const importer::SearchEngineInfo& info);
  void ExpectFirstEmptyFolderBookmark(const ImportedBookmarkEntry& entry);
  void ExpectSecondEmptyFolderBookmark(const ImportedBookmarkEntry& entry);

  base::FilePath test_data_path_;
};

void BookmarkHTMLReaderTestWithData::SetUp() {
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path_));
  test_data_path_ = test_data_path_.AppendASCII("bookmark_html_reader");
}

void BookmarkHTMLReaderTestWithData::ExpectFirstFirefox2Bookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Empty", entry.title);
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1)
    EXPECT_EQ(u"Empty's Parent", entry.path.front());
}

void BookmarkHTMLReaderTestWithData::ExpectSecondFirefox2Bookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1)
    EXPECT_EQ(u"Not Empty", entry.path.front());
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
}

void BookmarkHTMLReaderTestWithData::ExpectThirdFirefox2Bookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(0000000000), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1)
    EXPECT_EQ(u"Not Empty But Default", entry.path.front());
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
}

void BookmarkHTMLReaderTestWithData::ExpectFirstEpiphanyBookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
  EXPECT_EQ(0U, entry.path.size());
}

void BookmarkHTMLReaderTestWithData::ExpectSecondEpiphanyBookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_EQ("http://www.google.com/", entry.url.spec());
  EXPECT_EQ(0U, entry.path.size());
}

void BookmarkHTMLReaderTestWithData::ExpectFirstFirefox23Bookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Google", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102167), entry.creation_time);
  EXPECT_EQ(0U, entry.path.size());
  EXPECT_EQ("https://www.google.com/", entry.url.spec());
}

void BookmarkHTMLReaderTestWithData::ExpectSecondFirefox23Bookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"Issues", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102304), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Chromium", entry.path.front());
  EXPECT_EQ("https://code.google.com/p/chromium/issues/list", entry.url.spec());
}

void BookmarkHTMLReaderTestWithData::ExpectThirdFirefox23Bookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"CodeSearch", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1376102224), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  EXPECT_EQ(u"Chromium", entry.path.front());
  EXPECT_EQ("http://code.google.com/p/chromium/codesearch", entry.url.spec());
}

void BookmarkHTMLReaderTestWithData::ExpectFirstFirefoxBookmarkWithKeyword(
    const importer::SearchEngineInfo& info) {
  EXPECT_EQ(u"http://example.{searchTerms}.com/", info.url);
  EXPECT_EQ(u"keyword", info.keyword);
  EXPECT_EQ(u"Bookmark Keyword", info.display_name);
}

void BookmarkHTMLReaderTestWithData::ExpectSecondFirefoxBookmarkWithKeyword(
    const importer::SearchEngineInfo& info) {
  EXPECT_EQ(u"http://example.com/?q={searchTerms}", info.url);
  EXPECT_EQ(u"keyword", info.keyword);
  EXPECT_EQ(u"BookmarkName", info.display_name);
}

void BookmarkHTMLReaderTestWithData::ExpectFirstEmptyFolderBookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(std::u16string(), entry.title);
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1)
    EXPECT_EQ(u"Empty's Parent", entry.path.front());
}

void BookmarkHTMLReaderTestWithData::ExpectSecondEmptyFolderBookmark(
    const ImportedBookmarkEntry& entry) {
  EXPECT_EQ(u"[Tamura Yukari.com]", entry.title);
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
  EXPECT_EQ(1U, entry.path.size());
  if (entry.path.size() == 1)
    EXPECT_EQ(std::u16string(), entry.path.front());
  EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
}

}  // namespace

TEST_F(BookmarkHTMLReaderTestWithData, Firefox2BookmarkFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  ExpectFirstFirefox2Bookmark(bookmarks[0]);
  ExpectSecondFirefox2Bookmark(bookmarks[1]);
  ExpectThirdFirefox2Bookmark(bookmarks[2]);
}

TEST_F(BookmarkHTMLReaderTestWithData, BookmarkFileWithHrTagImport) {
  base::FilePath path = test_data_path_.AppendASCII("firefox23.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  ExpectFirstFirefox23Bookmark(bookmarks[0]);
  ExpectSecondFirefox23Bookmark(bookmarks[1]);
  ExpectThirdFirefox23Bookmark(bookmarks[2]);
}

TEST_F(BookmarkHTMLReaderTestWithData, EpiphanyBookmarkFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("epiphany.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      &bookmarks, nullptr, nullptr);

  ASSERT_EQ(2U, bookmarks.size());
  ExpectFirstEpiphanyBookmark(bookmarks[0]);
  ExpectSecondEpiphanyBookmark(bookmarks[1]);
}

TEST_F(BookmarkHTMLReaderTestWithData, FirefoxBookmarkFileWithKeywordImport) {
  base::FilePath path = test_data_path_.AppendASCII(
      "firefox_bookmark_keyword.html");

  std::vector<importer::SearchEngineInfo> search_engines;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      nullptr, &search_engines, nullptr);

  ASSERT_EQ(2U, search_engines.size());
  ExpectFirstFirefoxBookmarkWithKeyword(search_engines[0]);
  ExpectSecondFirefoxBookmarkWithKeyword(search_engines[1]);
}

TEST_F(BookmarkHTMLReaderTestWithData, EmptyFolderImport) {
  base::FilePath path = test_data_path_.AppendASCII("empty_folder.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  ExpectFirstEmptyFolderBookmark(bookmarks[0]);
  ExpectSecondEmptyFolderBookmark(bookmarks[1]);
  ExpectThirdFirefox2Bookmark(bookmarks[2]);
}

TEST_F(BookmarkHTMLReaderTestWithData,
       RedditSaverFileImport) {
  base::FilePath path = test_data_path_.AppendASCII("redditsaver.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      &bookmarks, nullptr, nullptr);

  ASSERT_EQ(2U, bookmarks.size());
  EXPECT_EQ(u"Google", bookmarks[0].title);
  EXPECT_EQ(u"YouTube", bookmarks[1].title);
}

// Verifies that importing a bookmarks file without a charset specified succeeds
// (by falling back to a default charset). Per [ http://crbug.com/460423 ], this
// sort of bookmarks file is generated by IE.
TEST_F(BookmarkHTMLReaderTestWithData,
       InternetExplorerBookmarkFileWithoutCharsetImport) {
  base::FilePath path = test_data_path_.AppendASCII("ie_sans_charset.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::RepeatingCallback<bool(const GURL&)>(), path,
                      &bookmarks, nullptr, nullptr);

  ASSERT_EQ(3U, bookmarks.size());
  EXPECT_EQ(u"Google", bookmarks[0].title);
  EXPECT_EQ(u"Outlook", bookmarks[1].title);
  EXPECT_EQ(u"Speed Test", bookmarks[2].title);
}

namespace {

class CancelAfterFifteenCalls {
  int count;
 public:
  CancelAfterFifteenCalls() : count(0) { }
  bool ShouldCancel() {
    return ++count > 16;
  }
};

}  // namespace

TEST_F(BookmarkHTMLReaderTestWithData, CancellationCallback) {
  // Use a file for testing that has multiple bookmarks.
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  CancelAfterFifteenCalls cancel_fifteen;
  ImportBookmarksFile(
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

TEST_F(BookmarkHTMLReaderTestWithData, ValidURLCallback) {
  // Use a file for testing that has multiple bookmarks.
  base::FilePath path = test_data_path_.AppendASCII("firefox2.html");

  std::vector<ImportedBookmarkEntry> bookmarks;
  ImportBookmarksFile(base::RepeatingCallback<bool(void)>(),
                      base::BindRepeating(&IsURLValid), path, &bookmarks,
                      nullptr, nullptr);

  ASSERT_EQ(2U, bookmarks.size());
  ExpectFirstFirefox2Bookmark(bookmarks[0]);
  ExpectThirdFirefox2Bookmark(bookmarks[1]);
}

}  // namespace bookmark_html_reader
