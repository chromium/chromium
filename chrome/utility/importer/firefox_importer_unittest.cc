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
#include "chrome/common/importer/mock_importer_bridge.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/common/importer_url_row.h"
#include "content/public/test/browser_task_environment.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class FirefoxImporterTest : public testing::Test {
 public:
  // Imports bookmarks from Firefox profile files into |bookmarks| and
  // |favicons| containers. |firefox_version| must match the name of
  // subdirectory where test files are stored.
  void ImportBookmarksFromVersion(
      std::string_view firefox_version,
      std::vector<user_data_importer::ImportedBookmarkEntry>* bookmarks,
      favicon_base::FaviconUsageDataList* favicons) {
    base::FilePath places_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &places_path));
    places_path =
        places_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
            firefox_version);
    ASSERT_TRUE(base::DirectoryExists(places_path));
    user_data_importer::SourceProfile profile;
    profile.source_path = places_path;

    EXPECT_CALL(*bridge_, NotifyStarted());
    EXPECT_CALL(*bridge_, NotifyItemStarted(user_data_importer::FAVORITES));
    EXPECT_CALL(*bridge_, AddBookmarks(_, _))
        .WillOnce(::testing::SaveArg<0>(bookmarks));
    EXPECT_CALL(*bridge_, SetFavicons(_))
        .WillOnce(::testing::SaveArg<0>(favicons));
    EXPECT_CALL(*bridge_, NotifyItemEnded(user_data_importer::FAVORITES));
    EXPECT_CALL(*bridge_, NotifyEnded());
    importer_->StartImport(profile, user_data_importer::FAVORITES,
                           bridge_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<MockImporterBridge> bridge_ =
      base::MakeRefCounted<MockImporterBridge>();
  scoped_refptr<FirefoxImporter> importer_ =
      base::MakeRefCounted<FirefoxImporter>();
};

TEST_F(FirefoxImporterTest, ImportBookmarks_Firefox48) {
  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
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

TEST_F(FirefoxImporterTest, ImportBookmarks_Firefox57) {
  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
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

TEST_F(FirefoxImporterTest, ImportBookmarksWithCorruptedDb) {
  base::ScopedTempDir source_temp_dir;
  ASSERT_TRUE(source_temp_dir.CreateUniqueTempDir());
  base::FilePath places_file =
      source_temp_dir.GetPath().AppendASCII("places.sqlite");

  // Part 1: Test LoadNodeIDByGUID, LoadLivemarkIDs, and GetTopBookmarkFolder
  // validation
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(places_file));
    // Create tables with missing columns (no 'id' column in moz_bookmarks) to
    // test that SQL statement validation in LoadNodeIDByGUID and
    // GetTopBookmarkFolder prevents crashes
    ASSERT_TRUE(
        db.Execute("CREATE TABLE moz_bookmarks (type INTEGER, fk INTEGER, "
                   "parent INTEGER, position INTEGER)"));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE moz_anno_attributes (attr_id INTEGER PRIMARY KEY, "
        "name TEXT)"));
    ASSERT_TRUE(
        db.Execute("CREATE TABLE moz_items_annos (item_id INTEGER, "
                   "anno_attribute_id INTEGER)"));
    ASSERT_TRUE(db.Execute("INSERT INTO moz_bookmarks VALUES (2, NULL, 0, 0)"));
    ASSERT_TRUE(db.Execute("INSERT INTO moz_bookmarks VALUES (2, NULL, 1, 0)"));
  }
  scoped_refptr<FirefoxImporter> first_importer =
      base::MakeRefCounted<FirefoxImporter>();
  user_data_importer::SourceProfile profile;
  profile.source_path = source_temp_dir.GetPath();
  scoped_refptr<MockImporterBridge> bridge =
      base::MakeRefCounted<MockImporterBridge>();
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyItemStarted(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*bridge, SetFavicons(_)).Times(0);
  EXPECT_CALL(*bridge, NotifyItemEnded(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge, NotifyEnded());
  first_importer->StartImport(profile, user_data_importer::FAVORITES,
                              bridge.get());

  // Part 2: Test GetWholeBookmarkFolder validation
  base::ScopedTempDir second_source_dir;
  ASSERT_TRUE(second_source_dir.CreateUniqueTempDir());
  base::FilePath second_places_file =
      second_source_dir.GetPath().AppendASCII("places.sqlite");

  {
    // Create the database with a valid moz_bookmarks table structure
    // but missing the tables needed for the joins in GetWholeBookmarkFolder
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(second_places_file));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE moz_bookmarks (id INTEGER PRIMARY KEY, type INTEGER, "
        "parent INTEGER, position INTEGER, title TEXT, "
        "guid TEXT)"));
    ASSERT_TRUE(
        db.Execute("INSERT INTO moz_bookmarks VALUES (1, 2, 0, 0, 'root', "
                   "'root________')"));
    ASSERT_TRUE(
        db.Execute("INSERT INTO moz_bookmarks VALUES (2, 2, 1, 0, 'toolbar', "
                   "'toolbar_____')"));
    ASSERT_TRUE(
        db.Execute("INSERT INTO moz_bookmarks VALUES (3, 2, 1, 1, 'menu', "
                   "'menu________')"));
    ASSERT_TRUE(
        db.Execute("CREATE TABLE moz_anno_attributes (id INTEGER PRIMARY KEY, "
                   "name TEXT)"));
    ASSERT_TRUE(
        db.Execute("CREATE TABLE moz_items_annos (item_id INTEGER, "
                   "anno_attribute_id INTEGER)"));
  }
  scoped_refptr<FirefoxImporter> second_importer =
      base::MakeRefCounted<FirefoxImporter>();
  user_data_importer::SourceProfile second_profile;
  second_profile.source_path = second_source_dir.GetPath();
  scoped_refptr<MockImporterBridge> second_bridge =
      base::MakeRefCounted<MockImporterBridge>();
  EXPECT_CALL(*second_bridge, NotifyStarted());
  EXPECT_CALL(*second_bridge, NotifyItemStarted(user_data_importer::FAVORITES));
  EXPECT_CALL(*second_bridge, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*second_bridge, SetFavicons(_)).Times(0);
  EXPECT_CALL(*second_bridge, NotifyItemEnded(user_data_importer::FAVORITES));
  EXPECT_CALL(*second_bridge, NotifyEnded());
  second_importer->StartImport(second_profile, user_data_importer::FAVORITES,
                               second_bridge.get());
}

TEST_F(FirefoxImporterTest, ImportHistorySchema) {
  base::FilePath places_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &places_path));
  places_path =
      places_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
          "48.0.2");
  scoped_refptr<FirefoxImporter> ff_importer =
      base::MakeRefCounted<FirefoxImporter>();
  user_data_importer::SourceProfile profile;
  profile.source_path = places_path;
  scoped_refptr<MockImporterBridge> bridge =
      base::MakeRefCounted<MockImporterBridge>();
  std::vector<user_data_importer::ImporterURLRow> history;
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyItemStarted(user_data_importer::HISTORY));
  EXPECT_CALL(*bridge, SetHistoryItems(_, _))
      .WillOnce(::testing::SaveArg<0>(&history));
  EXPECT_CALL(*bridge, NotifyItemEnded(user_data_importer::HISTORY));
  EXPECT_CALL(*bridge, NotifyEnded());
  ff_importer->StartImport(profile, user_data_importer::HISTORY, bridge.get());
  ASSERT_EQ(3u, history.size());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/48.0.2/firstrun/learnmore/",
            history[0].url.spec());
  EXPECT_EQ("https://www.mozilla.org/en-US/firefox/48.0.2/firstrun/",
            history[1].url.spec());
  EXPECT_EQ("http://google.com/", history[2].url.spec());
}

TEST_F(FirefoxImporterTest, ImportHistoryWithCorruptedDb) {
  base::ScopedTempDir source_temp_dir;
  ASSERT_TRUE(source_temp_dir.CreateUniqueTempDir());
  base::FilePath places_file =
      source_temp_dir.GetPath().AppendASCII("places.sqlite");
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(places_file));
    // Create moz_places with missing columns to test s.is_valid() check in
    // ImportHistory
    ASSERT_TRUE(db.Execute("CREATE TABLE moz_places (partial_url TEXT)"));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE moz_historyvisits (visit_id INTEGER PRIMARY KEY)"));
    ASSERT_TRUE(db.Execute("INSERT INTO moz_places VALUES ('invalid-url')"));
    ASSERT_TRUE(db.Execute("INSERT INTO moz_historyvisits VALUES (1)"));
  }
  scoped_refptr<FirefoxImporter> ff_importer =
      base::MakeRefCounted<FirefoxImporter>();
  user_data_importer::SourceProfile profile;
  profile.source_path = source_temp_dir.GetPath();
  scoped_refptr<MockImporterBridge> bridge =
      base::MakeRefCounted<MockImporterBridge>();
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyItemStarted(user_data_importer::HISTORY));
  EXPECT_CALL(*bridge, SetHistoryItems(_, _)).Times(0);
  EXPECT_CALL(*bridge, NotifyItemEnded(user_data_importer::HISTORY));
  EXPECT_CALL(*bridge, NotifyEnded());
  ff_importer->StartImport(profile, user_data_importer::HISTORY, bridge.get());
}

TEST_F(FirefoxImporterTest, ImportAutofillFormDataWithCorruptedDb) {
  base::ScopedTempDir source_temp_dir;
  ASSERT_TRUE(source_temp_dir.CreateUniqueTempDir());
  base::FilePath form_history_file =
      source_temp_dir.GetPath().AppendASCII("formhistory.sqlite");
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(form_history_file));
    // Create moz_formhistory with missing columns to test s.is_valid() check
    // in ImportAutofillFormData
    ASSERT_TRUE(
        db.Execute("CREATE TABLE moz_formhistory (partial_field TEXT)"));
    ASSERT_TRUE(
        db.Execute("INSERT INTO moz_formhistory VALUES ('invalid-data')"));
  }
  scoped_refptr<FirefoxImporter> ff_importer =
      base::MakeRefCounted<FirefoxImporter>();
  user_data_importer::SourceProfile profile;
  profile.source_path = source_temp_dir.GetPath();
  scoped_refptr<MockImporterBridge> bridge =
      base::MakeRefCounted<MockImporterBridge>();
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge,
              NotifyItemStarted(user_data_importer::AUTOFILL_FORM_DATA));
  EXPECT_CALL(*bridge, SetAutofillFormData(_)).Times(0);
  EXPECT_CALL(*bridge, NotifyItemEnded(user_data_importer::AUTOFILL_FORM_DATA));
  EXPECT_CALL(*bridge, NotifyEnded());
  ff_importer->StartImport(profile, user_data_importer::AUTOFILL_FORM_DATA,
                           bridge.get());
}
