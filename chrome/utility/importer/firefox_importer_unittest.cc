// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/firefox_importer.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/mock_importer_bridge.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/common/importer_url_row.h"
#include "components/user_data_importer/content/fake_bookmark_html_parser.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

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

    base::RunLoop run_loop;
    EXPECT_CALL(*bridge_, NotifyStarted());
    EXPECT_CALL(*bridge_, NotifyItemStarted(user_data_importer::FAVORITES));
    EXPECT_CALL(*bridge_, AddBookmarks(_, _))
        .WillOnce(::testing::SaveArg<0>(bookmarks));
    EXPECT_CALL(*bridge_, SetFavicons(_))
        .WillOnce(::testing::SaveArg<0>(favicons));
    EXPECT_CALL(*bridge_, NotifyItemEnded(user_data_importer::FAVORITES));
    EXPECT_CALL(*bridge_, NotifyEnded())
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    importer_->SetBookmarkHtmlParser(receiver_.BindNewPipeAndPassRemote());
    importer_->StartImport(profile, user_data_importer::FAVORITES,
                           bridge_.get());
    run_loop.Run();

    EXPECT_EQ(1u, fake_parser_.reencode_favicons_call_count());
    for (const auto& favicon : *favicons) {
      EXPECT_FALSE(favicon.png_data.empty());
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<MockImporterBridge> bridge_ =
      base::MakeRefCounted<MockImporterBridge>();
  scoped_refptr<FirefoxImporter> importer_ =
      base::MakeRefCounted<FirefoxImporter>();
  user_data_importer::FakeBookmarkHtmlParser fake_parser_;
  mojo::Receiver<user_data_importer::mojom::BookmarkHtmlParser> receiver_{
      &fake_parser_};
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
  base::RunLoop run_loop1;
  EXPECT_CALL(*bridge, NotifyStarted());
  EXPECT_CALL(*bridge, NotifyItemStarted(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*bridge, SetFavicons(_)).Times(0);
  EXPECT_CALL(*bridge, NotifyItemEnded(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge, NotifyEnded())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop1, &base::RunLoop::Quit));

  user_data_importer::FakeBookmarkHtmlParser fake_parser1;
  mojo::Receiver<user_data_importer::mojom::BookmarkHtmlParser> receiver1{
      &fake_parser1};
  first_importer->SetBookmarkHtmlParser(receiver1.BindNewPipeAndPassRemote());

  first_importer->StartImport(profile, user_data_importer::FAVORITES,
                              bridge.get());
  run_loop1.Run();

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
  base::RunLoop run_loop2;
  EXPECT_CALL(*second_bridge, NotifyStarted());
  EXPECT_CALL(*second_bridge, NotifyItemStarted(user_data_importer::FAVORITES));
  EXPECT_CALL(*second_bridge, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*second_bridge, SetFavicons(_)).Times(0);
  EXPECT_CALL(*second_bridge, NotifyItemEnded(user_data_importer::FAVORITES));
  EXPECT_CALL(*second_bridge, NotifyEnded())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop2, &base::RunLoop::Quit));

  user_data_importer::FakeBookmarkHtmlParser fake_parser2;
  mojo::Receiver<user_data_importer::mojom::BookmarkHtmlParser> receiver2{
      &fake_parser2};
  second_importer->SetBookmarkHtmlParser(receiver2.BindNewPipeAndPassRemote());

  second_importer->StartImport(second_profile, user_data_importer::FAVORITES,
                               second_bridge.get());
  run_loop2.Run();
}

TEST_F(FirefoxImporterTest, ImportBookmarksWithParserDisconnect) {
  base::FilePath places_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &places_path));
  places_path =
      places_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
          "48.0.2");
  ASSERT_TRUE(base::DirectoryExists(places_path));
  user_data_importer::SourceProfile profile;
  profile.source_path = places_path;

  base::RunLoop run_loop;
  EXPECT_CALL(*bridge_, NotifyStarted());
  EXPECT_CALL(*bridge_, NotifyItemStarted(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge_, AddBookmarks(_, _)).Times(0);
  EXPECT_CALL(*bridge_, SetFavicons(_)).Times(0);
  EXPECT_CALL(*bridge_, NotifyItemEnded(user_data_importer::FAVORITES));
  // The items that follow the bookmarks must still be imported.
  EXPECT_CALL(*bridge_,
              NotifyItemStarted(user_data_importer::AUTOFILL_FORM_DATA));
  EXPECT_CALL(*bridge_,
              NotifyItemEnded(user_data_importer::AUTOFILL_FORM_DATA));
  EXPECT_CALL(*bridge_, NotifyEnded())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Bind a remote whose receiver is dropped immediately, so that the parser is
  // disconnected and never replies.
  mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser;
  std::ignore = parser.InitWithNewPipeAndPassReceiver();
  importer_->SetBookmarkHtmlParser(std::move(parser));

  importer_->StartImport(
      profile,
      user_data_importer::FAVORITES | user_data_importer::AUTOFILL_FORM_DATA,
      bridge_.get());

  // The import must still complete instead of stalling forever.
  run_loop.Run();
}

TEST_F(FirefoxImporterTest,
       ImportBookmarksWithDisconnectDuringFaviconReencode) {
  base::FilePath places_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &places_path));
  places_path =
      places_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
          "48.0.2");
  ASSERT_TRUE(base::DirectoryExists(places_path));
  user_data_importer::SourceProfile profile;
  profile.source_path = places_path;

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  base::RunLoop run_loop;
  EXPECT_CALL(*bridge_, NotifyStarted());
  EXPECT_CALL(*bridge_, NotifyItemStarted(user_data_importer::FAVORITES));
  // Default bookmarks HTML parsing succeeds, so bookmarks are imported before
  // favicon re-encoding is requested.
  EXPECT_CALL(*bridge_, AddBookmarks(_, _))
      .WillOnce(::testing::SaveArg<0>(&bookmarks));
  // Disconnecting during ReencodeFavicons must drop favicons without falling
  // back to local decoding.
  EXPECT_CALL(*bridge_, SetFavicons(_)).Times(0);
  EXPECT_CALL(*bridge_, NotifyItemEnded(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge_,
              NotifyItemStarted(user_data_importer::AUTOFILL_FORM_DATA));
  EXPECT_CALL(*bridge_,
              NotifyItemEnded(user_data_importer::AUTOFILL_FORM_DATA));
  EXPECT_CALL(*bridge_, NotifyEnded())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  fake_parser_.set_reencode_favicons_override_for_testing(
      base::BindLambdaForTesting([this]() { receiver_.reset(); }));
  importer_->SetBookmarkHtmlParser(receiver_.BindNewPipeAndPassRemote());

  importer_->StartImport(
      profile,
      user_data_importer::FAVORITES | user_data_importer::AUTOFILL_FORM_DATA,
      bridge_.get());
  run_loop.Run();

  EXPECT_EQ(6u, bookmarks.size());
  EXPECT_EQ(1u, fake_parser_.reencode_favicons_call_count());
}

TEST_F(FirefoxImporterTest, ImportBookmarksWithCorruptedAndOversizedFavicons) {
  base::ScopedTempDir source_temp_dir;
  ASSERT_TRUE(source_temp_dir.CreateUniqueTempDir());

  // Create a minimal Firefox 55+ places.sqlite (without moz_favicons) with
  // three bookmarks.
  {
    sql::Database places_db(sql::test::kTestTag);
    ASSERT_TRUE(
        places_db.Open(source_temp_dir.GetPath().AppendASCII("places.sqlite")));
    ASSERT_TRUE(places_db.Execute(
        "CREATE TABLE moz_places (id INTEGER PRIMARY KEY, url TEXT, "
        "title TEXT)"));
    ASSERT_TRUE(places_db.Execute(
        "CREATE TABLE moz_bookmarks (id INTEGER PRIMARY KEY, type INTEGER, "
        "fk INTEGER, parent INTEGER, position INTEGER, title TEXT, "
        "keyword_id INTEGER, dateAdded INTEGER, guid TEXT)"));
    ASSERT_TRUE(places_db.Execute(
        "CREATE TABLE moz_keywords (id INTEGER PRIMARY KEY, keyword TEXT)"));
    ASSERT_TRUE(places_db.Execute(
        "CREATE TABLE moz_anno_attributes (id INTEGER PRIMARY KEY, "
        "name TEXT)"));
    ASSERT_TRUE(
        places_db.Execute("CREATE TABLE moz_items_annos (item_id INTEGER, "
                          "anno_attribute_id INTEGER)"));

    ASSERT_TRUE(places_db.Execute(
        "INSERT INTO moz_bookmarks VALUES "
        "(1, 2, NULL, 0, 0, 'root', NULL, 0, 'root________'),"
        "(2, 2, NULL, 1, 0, 'toolbar', NULL, 0, 'toolbar_____'),"
        "(3, 2, NULL, 1, 1, 'menu', NULL, 0, 'menu________'),"
        "(4, 2, NULL, 1, 2, 'unfiled', NULL, 0, 'unfiled_____')"));
    ASSERT_TRUE(
        places_db.Execute("INSERT INTO moz_places VALUES "
                          "(10, 'https://example.com/one', 'One'),"
                          "(11, 'https://example.com/two', 'Two'),"
                          "(12, 'https://corrupt.example/page', 'Corrupt')"));
    ASSERT_TRUE(places_db.Execute(
        "INSERT INTO moz_bookmarks VALUES "
        "(100, 1, 10, 2, 0, 'One', NULL, 1000000, 'bm1_________'),"
        "(101, 1, 11, 2, 1, 'Two', NULL, 1000000, 'bm2_________'),"
        "(102, 1, 12, 2, 2, 'Corrupt', NULL, 1000000, 'bm3_________')"));
  }

  // Create favicons.sqlite with one valid 32x32 PNG shared by two bookmarks and
  // one corrupt blob.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(32, 32);
    bitmap.eraseColor(SK_ColorBLUE);
    std::optional<std::vector<uint8_t>> valid_png =
        gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                          /*discard_transparency=*/false);
    ASSERT_TRUE(valid_png.has_value());

    sql::Database favicons_db(sql::test::kTestTag);
    ASSERT_TRUE(favicons_db.Open(
        source_temp_dir.GetPath().AppendASCII("favicons.sqlite")));
    ASSERT_TRUE(favicons_db.Execute(
        "CREATE TABLE moz_icons (id INTEGER PRIMARY KEY, icon_url TEXT, "
        "data BLOB)"));
    ASSERT_TRUE(favicons_db.Execute(
        "CREATE TABLE moz_pages_w_icons (id INTEGER PRIMARY KEY, "
        "page_url TEXT)"));
    ASSERT_TRUE(favicons_db.Execute(
        "CREATE TABLE moz_icons_to_pages (page_id INTEGER, icon_id INTEGER)"));

    sql::Statement insert_icon(favicons_db.GetUniqueStatement(
        "INSERT INTO moz_icons (id, icon_url, data) VALUES (?, ?, ?)"));
    insert_icon.BindInt64(0, 1);
    insert_icon.BindString(1, "https://example.com/favicon.png");
    insert_icon.BindBlob(2, *valid_png);
    ASSERT_TRUE(insert_icon.Run());

    insert_icon.Reset(true);
    insert_icon.BindInt64(0, 2);
    insert_icon.BindString(1, "https://corrupt.example/favicon.png");
    const std::vector<uint8_t> corrupt_bytes = {0x00, 0x01, 0x02, 0x03};
    insert_icon.BindBlob(2, corrupt_bytes);
    ASSERT_TRUE(insert_icon.Run());

    ASSERT_TRUE(
        favicons_db.Execute("INSERT INTO moz_pages_w_icons VALUES "
                            "(1, 'https://example.com/one'),"
                            "(2, 'https://example.com/two'),"
                            "(3, 'https://corrupt.example/page')"));
    ASSERT_TRUE(favicons_db.Execute(
        "INSERT INTO moz_icons_to_pages VALUES (1, 1), (2, 1), (3, 2)"));
  }

  user_data_importer::SourceProfile profile;
  profile.source_path = source_temp_dir.GetPath();

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks;
  favicon_base::FaviconUsageDataList favicons;
  base::RunLoop run_loop;
  EXPECT_CALL(*bridge_, NotifyStarted());
  EXPECT_CALL(*bridge_, NotifyItemStarted(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge_, AddBookmarks(_, _))
      .WillOnce(::testing::SaveArg<0>(&bookmarks));
  EXPECT_CALL(*bridge_, SetFavicons(_))
      .WillOnce(::testing::SaveArg<0>(&favicons));
  EXPECT_CALL(*bridge_, NotifyItemEnded(user_data_importer::FAVORITES));
  EXPECT_CALL(*bridge_, NotifyEnded())
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  importer_->SetBookmarkHtmlParser(receiver_.BindNewPipeAndPassRemote());
  importer_->StartImport(profile, user_data_importer::FAVORITES, bridge_.get());
  run_loop.Run();

  EXPECT_EQ(3u, bookmarks.size());
  EXPECT_EQ(1u, fake_parser_.reencode_favicons_call_count());
  ASSERT_EQ(1u, favicons.size());
  EXPECT_EQ(GURL("https://example.com/favicon.png"), favicons[0].favicon_url);
  EXPECT_EQ((std::set<GURL>{GURL("https://example.com/one"),
                            GURL("https://example.com/two")}),
            favicons[0].urls);
  SkBitmap decoded = gfx::PNGCodec::Decode(favicons[0].png_data);
  ASSERT_FALSE(decoded.isNull());
  EXPECT_EQ(gfx::kFaviconSize, decoded.width());
  EXPECT_EQ(gfx::kFaviconSize, decoded.height());
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
