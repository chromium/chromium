// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/utility/importer/safari_importer.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/safari_importer_utils.h"
#include "chrome/utility/importer/safari_importer.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "sql/database.h"
#include "testing/platform_test.h"

// In order to test the Safari import functionality effectively, we store a
// simulated Library directory containing dummy data files in the same
// structure as ~/Library in the Chrome test data directory.
// This function returns the path to that directory.
base::FilePath GetTestSafariLibraryPath(const std::string& suffix) {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);

  // Our simulated ~/Library directory
  return
      test_dir.AppendASCII("import").AppendASCII("safari").AppendASCII(suffix);
}

class SafariImporterTest : public PlatformTest {
 public:
  SafariImporter* GetSafariImporter() {
    return GetSafariImporterWithPathSuffix("default");
  }

  SafariImporter* GetSafariImporterWithPathSuffix(const std::string& suffix) {
    base::FilePath test_library_dir = GetTestSafariLibraryPath(suffix);
    CHECK(base::PathExists(test_library_dir));
    return new SafariImporter(test_library_dir);
  }
};

TEST_F(SafariImporterTest, BookmarkImport) {
  // Expected results
  const struct {
    bool in_toolbar;
    GURL url;
    // We store the path with levels of nesting delimited by forward slashes.
    std::u16string path;
    std::u16string title;
  } kImportedBookmarksData[] = {
      {true, GURL("http://www.apple.com/"), u"Toolbar/", u"Apple"},
      {true, GURL("http://www.yahoo.com/"), u"Toolbar/", u"Yahoo!"},
      {true, GURL("http://www.cnn.com/"), u"Toolbar/News", u"CNN"},
      {true, GURL("http://www.nytimes.com/"), u"Toolbar/News",
       u"The New York Times"},
      {false, GURL("http://www.reddit.com/"), std::u16string(),
       u"reddit.com: what's new online!"},
      {false, GURL(), std::u16string(), u"Empty Folder"},
      {false, GURL("http://www.webkit.org/blog/"), std::u16string(),
       u"Surfin' Safari - The WebKit Blog"},
  };

  scoped_refptr<SafariImporter> importer(GetSafariImporter());
  std::vector<ImportedBookmarkEntry> bookmarks;
  importer->ParseBookmarks(u"Toolbar", &bookmarks);
  size_t num_bookmarks = bookmarks.size();
  ASSERT_EQ(std::size(kImportedBookmarksData), num_bookmarks);

  for (size_t i = 0; i < num_bookmarks; ++i) {
    ImportedBookmarkEntry& entry = bookmarks[i];
    EXPECT_EQ(kImportedBookmarksData[i].in_toolbar, entry.in_toolbar);
    EXPECT_EQ(kImportedBookmarksData[i].url, entry.url);

    std::vector<std::u16string> path =
        base::SplitString(kImportedBookmarksData[i].path, u"/",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(path.size(), entry.path.size());
    for (size_t j = 0; j < path.size(); ++j) {
      EXPECT_EQ(path[j], entry.path[j]);
    }

    EXPECT_EQ(kImportedBookmarksData[i].title, entry.title);
  }
}

TEST_F(SafariImporterTest, BookmarkImportWithEmptyBookmarksMenu) {
  // Expected results.
  const struct {
    bool in_toolbar;
    GURL url;
    // We store the path with levels of nesting delimited by forward slashes.
    std::u16string path;
    std::u16string title;
  } kImportedBookmarksData[] = {
      {true, GURL("http://www.apple.com/"), u"Toolbar/", u"Apple"},
      {true, GURL("http://www.yahoo.com/"), u"Toolbar/", u"Yahoo!"},
      {true, GURL("http://www.cnn.com/"), u"Toolbar/News", u"CNN"},
      {true, GURL("http://www.nytimes.com/"), u"Toolbar/News",
       u"The New York Times"},
      {false, GURL("http://www.webkit.org/blog/"), std::u16string(),
       u"Surfin' Safari - The WebKit Blog"},
  };

  scoped_refptr<SafariImporter> importer(
      GetSafariImporterWithPathSuffix("empty_bookmarks_menu"));
  std::vector<ImportedBookmarkEntry> bookmarks;
  importer->ParseBookmarks(u"Toolbar", &bookmarks);
  size_t num_bookmarks = bookmarks.size();
  ASSERT_EQ(std::size(kImportedBookmarksData), num_bookmarks);

  for (size_t i = 0; i < num_bookmarks; ++i) {
    ImportedBookmarkEntry& entry = bookmarks[i];
    EXPECT_EQ(kImportedBookmarksData[i].in_toolbar, entry.in_toolbar);
    EXPECT_EQ(kImportedBookmarksData[i].url, entry.url);

    std::vector<std::u16string> path =
        base::SplitString(kImportedBookmarksData[i].path, u"/",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(path.size(), entry.path.size());
    for (size_t j = 0; j < path.size(); ++j) {
      EXPECT_EQ(path[j], entry.path[j]);
    }

    EXPECT_EQ(kImportedBookmarksData[i].title, entry.title);
  }
}

TEST_F(SafariImporterTest, CanImport) {
  uint16_t items = importer::NONE;
  EXPECT_TRUE(SafariImporterCanImport(
      GetTestSafariLibraryPath("default"), &items));
  EXPECT_EQ(items, importer::FAVORITES);

  // Check that we don't import anything from a bogus library directory.
  base::ScopedTempDir fake_library_dir;
  ASSERT_TRUE(fake_library_dir.CreateUniqueTempDir());
  EXPECT_FALSE(SafariImporterCanImport(fake_library_dir.GetPath(), &items));
}
