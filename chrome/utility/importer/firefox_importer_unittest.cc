// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/firefox_importer.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_url_row.h"
#include "chrome/common/importer/mock_importer_bridge.h"
#include "chrome/utility/importer/nss_decryptor.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Imports bookmarks from Firefox profile files into |bookmarks| and |favicons|
// containers. |firefox_version| must match the name of subdirectory where test
// files are stored.
void ImportBookmarksFromVersion(std::string_view firefox_version,
                                std::vector<ImportedBookmarkEntry>* bookmarks,
                                favicon_base::FaviconUsageDataList* favicons) {
  using ::testing::_;
  base::FilePath places_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &places_path));
  places_path =
      places_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
          firefox_version);
  ASSERT_TRUE(base::DirectoryExists(places_path));
  scoped_refptr<FirefoxImporter> importer = new FirefoxImporter;
  importer::SourceProfile profile;
  profile.source_path = places_path;
  scoped_refptr<MockImporterBridge> bridge = new MockImporterBridge;
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyItemStarted(importer::FAVORITES));
  EXPECT_CALL(*bridge, AddBookmarks(_, _))
      .WillOnce(::testing::SaveArg<0>(bookmarks));
  EXPECT_CALL(*bridge, SetFavicons(_))
      .WillOnce(::testing::SaveArg<0>(favicons));
  EXPECT_CALL(*bridge, NotifyItemEnded(importer::FAVORITES));
  EXPECT_CALL(*bridge, NotifyEnded());
  importer->StartImport(profile, importer::FAVORITES, bridge.get());
}

}  // namespace

TEST(FirefoxImporterTest, ImportBookmarks_Firefox48) {
  std::vector<ImportedBookmarkEntry> bookmarks;
  favicon_base::FaviconUsageDataList favicons;
  ImportBookmarksFromVersion("48.0.2", &bookmarks, &favicons);

  ASSERT_EQ(6u, bookmarks.size());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/central/",
            bookmarks[0].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/help/",
            bookmarks[1].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/customize/",
            bookmarks[2].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/contribute/",
            bookmarks[3].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/about/", bookmarks[4].url.spec());
  EXPECT_EQ("https://www.google.com/", bookmarks[5].url.spec());

  ASSERT_EQ(5u, favicons.size());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/0-1473403921346",
            favicons[0].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/1-1473403921347",
            favicons[1].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/2-1473403921348",
            favicons[2].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/3-1473403921349",
            favicons[3].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/4-1473403921349",
            favicons[4].favicon_url.spec());
}

TEST(FirefoxImporterTest, ImportBookmarks_Firefox57) {
  std::vector<ImportedBookmarkEntry> bookmarks;
  favicon_base::FaviconUsageDataList favicons;
  ImportBookmarksFromVersion("57.0.1", &bookmarks, &favicons);

  ASSERT_EQ(6u, bookmarks.size());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/central/",
            bookmarks[0].url.spec());
  EXPECT_EQ("https://support.mozilla.org/en-US/products/firefox",
            bookmarks[1].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/customize/",
            bookmarks[2].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/contribute/",
            bookmarks[3].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/about/", bookmarks[4].url.spec());
  EXPECT_EQ("https://www.google.com/", bookmarks[5].url.spec());

  ASSERT_EQ(4u, favicons.size());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/0-1513248843421",
            favicons[0].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/1-1513248843424",
            favicons[1].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/3-1513248843427",
            favicons[2].favicon_url.spec());
  EXPECT_EQ("http://www.mozilla.org/2005/made-up-favicon/4-1513248843429",
            favicons[3].favicon_url.spec());
}

TEST(FirefoxImporterTest, ImportHistorySchema) {
  using ::testing::_;
  base::FilePath places_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &places_path));
  places_path =
      places_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
          "48.0.2");
  scoped_refptr<FirefoxImporter> ff_importer = new FirefoxImporter;
  importer::SourceProfile profile;
  profile.source_path = places_path;
  scoped_refptr<MockImporterBridge> bridge = new MockImporterBridge;
  std::vector<ImporterURLRow> history;
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyItemStarted(importer::HISTORY));
  EXPECT_CALL(*bridge, SetHistoryItems(_, _))
      .WillOnce(::testing::SaveArg<0>(&history));
  EXPECT_CALL(*bridge, NotifyItemEnded(importer::HISTORY));
  EXPECT_CALL(*bridge, NotifyEnded());
  ff_importer->StartImport(profile, importer::HISTORY, bridge.get());
  ASSERT_EQ(3u, history.size());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/48.0.2/firstrun/learnmore/",
            history[0].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/48.0.2/firstrun/",
            history[1].url.spec());
  EXPECT_EQ("http://google.com/", history[2].url.spec());
}
