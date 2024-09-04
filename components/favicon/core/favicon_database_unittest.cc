// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_database.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/test/database_test_utils.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Pair;
using testing::Return;

namespace favicon {

namespace {

// Blobs for the bitmap tests.  These aren't real bitmaps.  Golden
// database files store the same blobs (see VersionN tests).
const unsigned char kBlob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";
const unsigned char kBlob2[] =
    "goiwuegrqrcomizqyzkjalitbahxfjytrqvpqeroicxmnlkhlzunacxaneviawrtxcywhgef";

const gfx::Size kSmallSize = gfx::Size(16, 16);
const gfx::Size kLargeSize = gfx::Size(32, 32);

// Verify that the up-to-date database has the expected tables and
// columns.  Functional tests only check whether the things which
// should be there are, but do not check if extraneous items are
// present.  Any extraneous items have the potential to interact
// negatively with future schema changes.
void VerifyTablesAndColumns(sql::Database* db) {
  // [meta], [favicons], [favicon_bitmaps], and [icon_mapping].
  EXPECT_EQ(4u, sql::test::CountSQLTables(db));

  // Implicit index on [meta], index on [favicons], index on
  // [favicon_bitmaps], two indices on [icon_mapping].
  EXPECT_EQ(5u, sql::test::CountSQLIndices(db));

  // [key] and [value].
  EXPECT_EQ(2u, sql::test::CountTableColumns(db, "meta"));

  // [id], [url], and [icon_type].
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "favicons"));

  // [id], [icon_id], [last_updated], [image_data], [width], [height] and
  // [last_requested].
  EXPECT_EQ(7u, sql::test::CountTableColumns(db, "favicon_bitmaps"));

  // [id], [page_url], and [icon_id].
  EXPECT_EQ(3u, sql::test::CountTableColumns(db, "icon_mapping"));
}

// Adds a favicon at `icon_url` with `icon_type` with default bitmap data and
// maps `page_url` to `icon_url`.
void AddAndMapFaviconSimple(FaviconDatabase* db,
                            const GURL& page_url,
                            const GURL& icon_url,
                            favicon_base::IconType icon_type) {
  scoped_refptr<base::RefCountedStaticMemory> data(
      new base::RefCountedStaticMemory(kBlob1));
  favicon_base::FaviconID favicon_id =
      db->AddFavicon(icon_url, icon_type, data, FaviconBitmapType::ON_VISIT,
                     base::Time::Now(), gfx::Size());
  db->AddIconMapping(page_url, favicon_id);
}

void VerifyDatabaseEmpty(sql::Database* db) {
  size_t rows = 0;
  EXPECT_TRUE(sql::test::CountTableRows(db, "favicons", &rows));
  EXPECT_EQ(0u, rows);
  EXPECT_TRUE(sql::test::CountTableRows(db, "favicon_bitmaps", &rows));
  EXPECT_EQ(0u, rows);
  EXPECT_TRUE(sql::test::CountTableRows(db, "icon_mapping", &rows));
  EXPECT_EQ(0u, rows);
}

// Helper to check that an expected mapping exists.
[[nodiscard]] bool CheckPageHasIcon(
    FaviconDatabase* db,
    const GURL& page_url,
    favicon_base::IconType expected_icon_type,
    const GURL& expected_icon_url,
    const gfx::Size& expected_icon_size,
    size_t expected_icon_contents_size,
    const unsigned char* expected_icon_contents) {
  std::vector<IconMapping> icon_mappings;
  if (!db->GetIconMappingsForPageURL(page_url, &icon_mappings)) {
    ADD_FAILURE() << "failed GetIconMappingsForPageURL()";
    return false;
  }

  // Scan for the expected type.
  std::vector<IconMapping>::const_iterator iter = icon_mappings.begin();
  for (; iter != icon_mappings.end(); ++iter) {
    if (iter->icon_type == expected_icon_type)
      break;
  }
  if (iter == icon_mappings.end()) {
    ADD_FAILURE() << "failed to find `expected_icon_type`";
    return false;
  }

  if (expected_icon_url != iter->icon_url) {
    EXPECT_EQ(expected_icon_url, iter->icon_url);
    return false;
  }

  std::vector<FaviconBitmap> favicon_bitmaps;
  if (!db->GetFaviconBitmaps(iter->icon_id, &favicon_bitmaps)) {
    ADD_FAILURE() << "failed GetFaviconBitmaps()";
    return false;
  }

  if (1 != favicon_bitmaps.size()) {
    EXPECT_EQ(1u, favicon_bitmaps.size());
    return false;
  }

  if (expected_icon_size != favicon_bitmaps[0].pixel_size) {
    EXPECT_EQ(expected_icon_size, favicon_bitmaps[0].pixel_size);
    return false;
  }

  if (expected_icon_contents_size != favicon_bitmaps[0].bitmap_data->size()) {
    EXPECT_EQ(expected_icon_contents_size,
              favicon_bitmaps[0].bitmap_data->size());
    return false;
  }

  if (memcmp(favicon_bitmaps[0].bitmap_data->front(), expected_icon_contents,
             expected_icon_contents_size)) {
    ADD_FAILURE() << "failed to match `expected_icon_contents`";
    return false;
  }
  return true;
}

bool CompareIconMappingIconUrl(const IconMapping& a, const IconMapping& b) {
  return a.icon_url < b.icon_url;
}

void SortMappingsByIconUrl(std::vector<IconMapping>* mappings) {
  std::sort(mappings->begin(), mappings->end(), &CompareIconMappingIconUrl);
}

base::Time GetLastUpdated(FaviconDatabase* db, favicon_base::FaviconID icon) {
  base::Time last_updated;
  EXPECT_TRUE(db->GetFaviconLastUpdatedTime(icon, &last_updated));
  return last_updated;
}

}  // namespace

class FaviconDatabaseTest : public testing::Test {
 public:
  FaviconDatabaseTest() = default;
  ~FaviconDatabaseTest() override = default;

  // Initialize a favicon database instance from the SQL file at
  // `golden_path` in the "History/" subdirectory of test data.
  std::unique_ptr<FaviconDatabase> LoadFromGolden(const char* golden_path) {
    if (!history::CreateDatabaseFromSQL(file_name_, golden_path)) {
      ADD_FAILURE() << "Failed loading " << golden_path;
      return nullptr;
    }

    std::unique_ptr<FaviconDatabase> db = std::make_unique<FaviconDatabase>();
    EXPECT_EQ(sql::INIT_OK, db->Init(file_name_));
    db->BeginTransaction();

    return db;
  }

 protected:
  void SetUp() override {
    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.GetPath().AppendASCII("TestFavicons.db");
  }

  // Page and icon urls shared by tests.  Present in golden database files (see
  // VersionN tests).
  const GURL kPageUrl1{"http://google.com/"};
  const GURL kPageUrl2{"http://yahoo.com/"};
  const GURL kPageUrl3{"http://www.google.com/"};
  const GURL kPageUrl4{"http://www.google.com/blank.html"};
  const GURL kPageUrl5{"http://www.bing.com/"};
  const GURL kIconUrl1{"http://www.google.com/favicon.ico"};
  const GURL kIconUrl2{"http://www.yahoo.com/favicon.ico"};
  const GURL kIconUrl3{"http://www.google.com/touch.ico"};
  const GURL kIconUrl5{"http://www.bing.com/favicon.ico"};

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
};

TEST_F(FaviconDatabaseTest, AddIconMapping) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  base::Time time = base::Time::Now();
  favicon_base::FaviconID id =
      db.AddFavicon(url, favicon_base::IconType::kTouchIcon, favicon,
                    FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(0, id);

  EXPECT_NE(0, db.AddIconMapping(url, id));
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(url, icon_mappings.front().page_url);
  EXPECT_EQ(id, icon_mappings.front().icon_id);
}

TEST_F(FaviconDatabaseTest, AddOnDemandFaviconBitmapCreatesCorrectTimestamps) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time add_time;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &add_time));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, icon);
  FaviconBitmapID bitmap = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_DEMAND, add_time, gfx::Size());
  ASSERT_NE(0, bitmap);

  base::Time last_updated;
  base::Time last_requested;
  ASSERT_TRUE(db.GetFaviconBitmap(bitmap, &last_updated, &last_requested,
                                  nullptr, nullptr));
  EXPECT_EQ(base::Time(), last_updated);
  EXPECT_EQ(add_time, last_requested);
}

TEST_F(FaviconDatabaseTest, AddFaviconBitmapCreatesCorrectTimestamps) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time add_time;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &add_time));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, icon);
  FaviconBitmapID bitmap = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_VISIT, add_time, gfx::Size());
  ASSERT_NE(0, bitmap);

  base::Time last_updated;
  base::Time last_requested;
  ASSERT_TRUE(db.GetFaviconBitmap(bitmap, &last_updated, &last_requested,
                                  nullptr, nullptr));
  EXPECT_EQ(add_time, last_updated);
  EXPECT_EQ(base::Time(), last_requested);
}

TEST_F(FaviconDatabaseTest, GetFaviconLastUpdatedTimeReturnsFalseForNoBitmaps) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, icon);

  base::Time last_updated;
  ASSERT_FALSE(db.GetFaviconLastUpdatedTime(icon, &last_updated));
}

TEST_F(FaviconDatabaseTest, GetFaviconLastUpdatedTimeReturnsMaxTime) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time add_time1;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &add_time1));
  base::Time add_time2 = add_time1 - base::Seconds(1);
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, icon);
  FaviconBitmapID bitmap1 = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_VISIT, add_time1, gfx::Size());
  ASSERT_NE(0, bitmap1);
  FaviconBitmapID bitmap2 = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_VISIT, add_time2, gfx::Size());
  ASSERT_NE(0, bitmap2);

  base::Time last_updated;
  ASSERT_TRUE(db.GetFaviconLastUpdatedTime(icon, &last_updated));
  EXPECT_EQ(add_time1, last_updated);
}

TEST_F(FaviconDatabaseTest, TouchUpdatesOnDemandFavicons) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Create an on-demand favicon.
  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, icon);
  FaviconBitmapID bitmap = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_DEMAND, start, gfx::Size());
  ASSERT_NE(0, bitmap);

  base::Time end = start + base::Days(kFaviconUpdateLastRequestedAfterDays);
  EXPECT_TRUE(db.TouchOnDemandFavicon(url, end));

  base::Time last_updated;
  base::Time last_requested;
  EXPECT_TRUE(db.GetFaviconBitmap(bitmap, &last_updated, &last_requested,
                                  nullptr, nullptr));
  // Does not mess with the last_updated field.
  EXPECT_EQ(base::Time(), last_updated);
  EXPECT_EQ(end, last_requested);  // Updates the last_requested field.
}

TEST_F(FaviconDatabaseTest, TouchUpdatesOnlyInfrequently) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Create an on-demand favicon.
  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, icon);
  FaviconBitmapID bitmap = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_DEMAND, start, gfx::Size());
  ASSERT_NE(0, bitmap);

  base::Time end = start + base::Minutes(1);
  EXPECT_TRUE(db.TouchOnDemandFavicon(url, end));

  base::Time last_requested;
  EXPECT_TRUE(
      db.GetFaviconBitmap(bitmap, nullptr, &last_requested, nullptr, nullptr));
  EXPECT_EQ(start, last_requested);  // No update.
}

TEST_F(FaviconDatabaseTest, TouchDoesNotUpdateStandardFavicons) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Create a standard favicon.
  GURL url("http://google.com");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  EXPECT_NE(0, icon);
  FaviconBitmapID bitmap = db.AddFaviconBitmap(
      icon, favicon, FaviconBitmapType::ON_VISIT, start, gfx::Size());
  EXPECT_NE(0, bitmap);

  base::Time end = start + base::Days(kFaviconUpdateLastRequestedAfterDays);
  db.TouchOnDemandFavicon(url, end);

  base::Time last_updated;
  base::Time last_requested;
  EXPECT_TRUE(db.GetFaviconBitmap(bitmap, &last_updated, &last_requested,
                                  nullptr, nullptr));
  EXPECT_EQ(start, last_updated);           // Does not mess with last_updated.
  EXPECT_EQ(base::Time(), last_requested);  // No update.
}

// Test that FaviconDatabase::GetOldOnDemandFavicons() returns on-demand icons
// which were requested prior to the passed in timestamp.
TEST_F(FaviconDatabaseTest, GetOldOnDemandFaviconsReturnsOld) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_DEMAND, start, gfx::Size());
  ASSERT_NE(0, icon);
  // Associate two different URLs with the icon.
  GURL page_url1("http://google.com/1");
  ASSERT_NE(0, db.AddIconMapping(page_url1, icon));
  GURL page_url2("http://google.com/2");
  ASSERT_NE(0, db.AddIconMapping(page_url2, icon));

  base::Time get_older_than = start + base::Seconds(1);
  auto map = db.GetOldOnDemandFavicons(get_older_than);

  // The icon is returned.
  EXPECT_THAT(map, ElementsAre(Pair(
                       icon, AllOf(Field(&IconMappingsForExpiry::icon_url, url),
                                   Field(&IconMappingsForExpiry::page_urls,
                                         ElementsAre(page_url1, page_url2))))));
}

// Test that FaviconDatabase::GetOldOnDemandFavicons() returns on-visit icons
// if the on-visit icons have expired. We need this behavior in order to delete
// icons stored via HistoryService::SetOnDemandFavicons() prior to on-demand
// icons setting the "last_requested" time.
TEST_F(FaviconDatabaseTest, GetOldOnDemandFaviconsDoesNotReturnExpired) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_VISIT, start, gfx::Size());
  ASSERT_NE(0, icon);
  GURL page_url("http://google.com/");
  ASSERT_NE(0, db.AddIconMapping(page_url, icon));
  ASSERT_TRUE(db.SetFaviconOutOfDate(icon));

  base::Time get_older_than = start + base::Seconds(1);
  auto map = db.GetOldOnDemandFavicons(get_older_than);

  // No icon is returned.
  EXPECT_TRUE(map.empty());
}

// Test that FaviconDatabase::GetOldOnDemandFavicons() does not return
// on-demand icons which were requested after the passed in timestamp.
TEST_F(FaviconDatabaseTest, GetOldOnDemandFaviconsDoesNotReturnFresh) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon =
      db.AddFavicon(url, favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_DEMAND, start, gfx::Size());
  ASSERT_NE(0, icon);
  ASSERT_NE(0, db.AddIconMapping(GURL("http://google.com/"), icon));

  // Touch the icon 3 weeks later.
  base::Time now = start + base::Days(21);
  EXPECT_TRUE(db.TouchOnDemandFavicon(url, now));

  base::Time get_older_than = start + base::Seconds(1);
  auto map = db.GetOldOnDemandFavicons(get_older_than);

  // No icon is returned.
  EXPECT_TRUE(map.empty());
}

// Test that FaviconDatabase::GetOldOnDemandFavicons() does not return
// non-expired on-visit icons.
TEST_F(FaviconDatabaseTest, GetOldOnDemandFaviconsDoesNotDeleteStandard) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time start;
  ASSERT_TRUE(base::Time::FromUTCExploded({2017, 5, 0, 1, 0, 0, 0, 0}, &start));
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  favicon_base::FaviconID icon = db.AddFavicon(
      GURL("http://google.com/favicon.ico"), favicon_base::IconType::kFavicon,
      favicon, FaviconBitmapType::ON_VISIT, start, gfx::Size());
  ASSERT_NE(0, icon);
  ASSERT_NE(0, db.AddIconMapping(GURL("http://google.com/"), icon));

  base::Time get_older_than = start + base::Seconds(1);
  auto map = db.GetOldOnDemandFavicons(get_older_than);

  // No icon is returned.
  EXPECT_TRUE(map.empty());
}

TEST_F(FaviconDatabaseTest, DeleteIconMappings) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");
  favicon_base::FaviconID id =
      db.AddFavicon(url, favicon_base::IconType::kTouchIcon);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id, favicon, FaviconBitmapType::ON_VISIT, time,
                      gfx::Size());
  EXPECT_LT(0, db.AddIconMapping(url, id));

  favicon_base::FaviconID id2 =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  EXPECT_LT(0, db.AddIconMapping(url, id2));
  ASSERT_NE(id, id2);

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mapping));
  ASSERT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_mapping.front().icon_type, favicon_base::IconType::kTouchIcon);
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
      url, {favicon_base::IconType::kFavicon}, nullptr));

  db.DeleteIconMappings(url);

  EXPECT_FALSE(db.GetIconMappingsForPageURL(url, nullptr));
  EXPECT_FALSE(db.GetIconMappingsForPageURL(
      url, {favicon_base::IconType::kFavicon}, nullptr));
}

TEST_F(FaviconDatabaseTest, GetIconMappingsForPageURL) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL url("http://google.com");

  favicon_base::FaviconID id1 =
      db.AddFavicon(url, favicon_base::IconType::kTouchIcon);
  base::Time time = base::Time::Now();
  db.AddFaviconBitmap(id1, favicon, FaviconBitmapType::ON_VISIT, time,
                      kSmallSize);
  db.AddFaviconBitmap(id1, favicon, FaviconBitmapType::ON_VISIT, time,
                      kLargeSize);
  EXPECT_LT(0, db.AddIconMapping(url, id1));

  favicon_base::FaviconID id2 =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  EXPECT_NE(id1, id2);
  db.AddFaviconBitmap(id2, favicon, FaviconBitmapType::ON_VISIT, time,
                      kSmallSize);
  EXPECT_LT(0, db.AddIconMapping(url, id2));

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(url, &icon_mappings));
  ASSERT_EQ(2u, icon_mappings.size());
  EXPECT_EQ(id1, icon_mappings[0].icon_id);
  EXPECT_EQ(id2, icon_mappings[1].icon_id);
}

TEST_F(FaviconDatabaseTest, RetainDataForPageUrls) {
  FaviconDatabase db;

  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

  db.BeginTransaction();

  // Build a database mapping
  // kPageUrl1 -> kIconUrl1
  // kPageUrl2 -> kIconUrl2
  // kPageUrl3 -> kIconUrl1
  // kPageUrl4 -> kIconUrl1
  // kPageUrl5 -> kIconUrl5
  // Then retain kPageUrl1, kPageUrl3, and kPageUrl5. kPageUrl2
  // and kPageUrl4 should go away, but the others should be retained
  // correctly.

  // TODO(shess): This would probably make sense as a golden file.

  scoped_refptr<base::RefCountedStaticMemory> favicon1(
      new base::RefCountedStaticMemory(kBlob1));
  scoped_refptr<base::RefCountedStaticMemory> favicon2(
      new base::RefCountedStaticMemory(kBlob2));

  favicon_base::FaviconID kept_id1 =
      db.AddFavicon(kIconUrl1, favicon_base::IconType::kFavicon);
  db.AddFaviconBitmap(kept_id1, favicon1, FaviconBitmapType::ON_VISIT,
                      base::Time::Now(), kLargeSize);
  db.AddIconMapping(kPageUrl1, kept_id1);
  db.AddIconMapping(kPageUrl3, kept_id1);
  db.AddIconMapping(kPageUrl4, kept_id1);

  favicon_base::FaviconID unkept_id =
      db.AddFavicon(kIconUrl2, favicon_base::IconType::kFavicon);
  db.AddFaviconBitmap(unkept_id, favicon1, FaviconBitmapType::ON_VISIT,
                      base::Time::Now(), kLargeSize);
  db.AddIconMapping(kPageUrl2, unkept_id);

  favicon_base::FaviconID kept_id2 =
      db.AddFavicon(kIconUrl5, favicon_base::IconType::kFavicon);
  db.AddFaviconBitmap(kept_id2, favicon2, FaviconBitmapType::ON_VISIT,
                      base::Time::Now(), kLargeSize);
  db.AddIconMapping(kPageUrl5, kept_id2);

  // RetainDataForPageUrls() uses schema manipulations for efficiency.
  // Grab a copy of the schema to make sure the final schema matches.
  const std::string original_schema = db.db_.GetSchema();

  std::vector<GURL> pages_to_keep;
  pages_to_keep.push_back(kPageUrl1);
  pages_to_keep.push_back(kPageUrl3);
  pages_to_keep.push_back(kPageUrl5);
  EXPECT_TRUE(db.RetainDataForPageUrls(pages_to_keep));

  // Mappings from the retained urls should be left.
  EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl1, favicon_base::IconType::kFavicon,
                               kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl3, favicon_base::IconType::kFavicon,
                               kIconUrl1, kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl5, favicon_base::IconType::kFavicon,
                               kIconUrl5, kLargeSize, sizeof(kBlob2), kBlob2));

  // The ones not retained should be missing.
  EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, nullptr));
  EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl4, nullptr));

  // Schema should be the same.
  EXPECT_EQ(original_schema, db.db_.GetSchema());
}

// Test that RetainDataForPageUrls() expires retained favicons.
TEST_F(FaviconDatabaseTest, RetainDataForPageUrlsExpiresRetainedFavicons) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  scoped_refptr<base::RefCountedStaticMemory> favicon1(
      new base::RefCountedStaticMemory(kBlob1));
  favicon_base::FaviconID kept_id = db.AddFavicon(
      kIconUrl1, favicon_base::IconType::kFavicon, favicon1,
      FaviconBitmapType::ON_VISIT, base::Time::Now(), gfx::Size());
  db.AddIconMapping(kPageUrl1, kept_id);

  EXPECT_TRUE(db.RetainDataForPageUrls(std::vector<GURL>(1u, kPageUrl1)));

  favicon_base::FaviconID new_favicon_id =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon);
  ASSERT_NE(0, new_favicon_id);
  std::vector<FaviconBitmap> new_favicon_bitmaps;
  db.GetFaviconBitmaps(new_favicon_id, &new_favicon_bitmaps);

  ASSERT_EQ(1u, new_favicon_bitmaps.size());
  EXPECT_EQ(0, new_favicon_bitmaps[0].last_updated.ToInternalValue());
}

// Tests that deleting a favicon deletes the favicon row and favicon bitmap
// rows from the database.
TEST_F(FaviconDatabaseTest, DeleteFavicon) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data1(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon1(
      new base::RefCountedBytes(data1));
  std::vector<unsigned char> data2(kBlob2, kBlob2 + sizeof(kBlob2));
  scoped_refptr<base::RefCountedBytes> favicon2(
      new base::RefCountedBytes(data2));

  GURL url("http://google.com");
  favicon_base::FaviconID id =
      db.AddFavicon(url, favicon_base::IconType::kFavicon);
  base::Time last_updated = base::Time::Now();
  db.AddFaviconBitmap(id, favicon1, FaviconBitmapType::ON_VISIT, last_updated,
                      kSmallSize);
  db.AddFaviconBitmap(id, favicon2, FaviconBitmapType::ON_VISIT, last_updated,
                      kLargeSize);

  EXPECT_TRUE(db.GetFaviconBitmaps(id, nullptr));

  EXPECT_TRUE(db.DeleteFavicon(id));
  EXPECT_FALSE(db.GetFaviconBitmaps(id, nullptr));
}

TEST_F(FaviconDatabaseTest, GetIconMappingsForPageURLForReturnOrder) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  // Add a favicon
  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  GURL page_url("http://google.com");
  GURL icon_url("http://google.com/favicon.ico");
  base::Time time = base::Time::Now();

  favicon_base::FaviconID id =
      db.AddFavicon(icon_url, favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id));
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id, icon_mappings.front().icon_id);
  EXPECT_EQ(favicon_base::IconType::kFavicon, icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);

  // Add a touch icon
  std::vector<unsigned char> data2(kBlob2, kBlob2 + sizeof(kBlob2));
  scoped_refptr<base::RefCountedBytes> favicon2 =
      new base::RefCountedBytes(data);

  favicon_base::FaviconID id2 =
      db.AddFavicon(icon_url, favicon_base::IconType::kTouchIcon, favicon2,
                    FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id2));

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id2, icon_mappings.front().icon_id);
  EXPECT_EQ(favicon_base::IconType::kTouchIcon,
            icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);

  // Add a touch precomposed icon
  scoped_refptr<base::RefCountedBytes> favicon3 =
      new base::RefCountedBytes(data2);

  favicon_base::FaviconID id3 =
      db.AddFavicon(icon_url, favicon_base::IconType::kTouchPrecomposedIcon,
                    favicon3, FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(0, db.AddIconMapping(page_url, id3));

  icon_mappings.clear();
  EXPECT_TRUE(db.GetIconMappingsForPageURL(page_url, &icon_mappings));

  EXPECT_EQ(page_url, icon_mappings.front().page_url);
  EXPECT_EQ(id3, icon_mappings.front().icon_id);
  EXPECT_EQ(favicon_base::IconType::kTouchPrecomposedIcon,
            icon_mappings.front().icon_type);
  EXPECT_EQ(icon_url, icon_mappings.front().icon_url);
}

// Test that when multiple icon types are passed to GetIconMappingsForPageURL()
// that the results are filtered according to the passed in types.
TEST_F(FaviconDatabaseTest, GetIconMappingsForPageURLWithIconTypes) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  const GURL kPageUrl("http://www.google.com");
  AddAndMapFaviconSimple(&db, kPageUrl, kIconUrl1,
                         favicon_base::IconType::kFavicon);
  AddAndMapFaviconSimple(&db, kPageUrl, kIconUrl2,
                         favicon_base::IconType::kTouchIcon);
  AddAndMapFaviconSimple(&db, kPageUrl, kIconUrl3,
                         favicon_base::IconType::kTouchIcon);
  AddAndMapFaviconSimple(&db, kPageUrl, kIconUrl5,
                         favicon_base::IconType::kTouchPrecomposedIcon);

  // Only the mappings for kFavicon and kTouchIcon should be returned.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
      kPageUrl,
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon},
      &icon_mappings));
  SortMappingsByIconUrl(&icon_mappings);

  ASSERT_EQ(3u, icon_mappings.size());
  EXPECT_EQ(kIconUrl1, icon_mappings[0].icon_url);
  EXPECT_EQ(kIconUrl3, icon_mappings[1].icon_url);
  EXPECT_EQ(kIconUrl2, icon_mappings[2].icon_url);
}

TEST_F(FaviconDatabaseTest, FindFirstPageURLForHost) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  const GURL kPageUrlHttp("http://www.google.com");
  const GURL kPageUrlHttps("https://www.google.com");
  const GURL kPageUrlHttpsSamePrefix("https://www.google.com.au");
  const GURL kPageUrlHttpsSameSuffix("https://m.www.google.com");
  const GURL kPageUrlInPath("https://www.example.com/www.google.com/");

  EXPECT_FALSE(db.FindFirstPageURLForHost(
      kPageUrlHttps,
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon}));

  AddAndMapFaviconSimple(&db, kPageUrlHttpsSamePrefix, kIconUrl1,
                         favicon_base::IconType::kFavicon);
  AddAndMapFaviconSimple(&db, kPageUrlHttpsSameSuffix, kIconUrl2,
                         favicon_base::IconType::kFavicon);
  AddAndMapFaviconSimple(&db, kPageUrlInPath, kIconUrl3,
                         favicon_base::IconType::kTouchIcon);

  // There should be no matching host for www.google.com when no matching host
  // exists with the required icon types.
  EXPECT_FALSE(db.FindFirstPageURLForHost(kPageUrlHttps,
                                          {favicon_base::IconType::kFavicon}));

  // Register the HTTP url in the database as a touch icon.
  AddAndMapFaviconSimple(&db, kPageUrlHttp, kIconUrl5,
                         favicon_base::IconType::kTouchIcon);

  EXPECT_FALSE(db.FindFirstPageURLForHost(kPageUrlHttps,
                                          {favicon_base::IconType::kFavicon}));

  // Expect a match when we search for a TouchIcon.
  std::optional<GURL> result = db.FindFirstPageURLForHost(
      kPageUrlHttps,
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon});

  EXPECT_EQ(kPageUrlHttp, result.value());

  // Expect that when we query for icon mappings with the result, we retrieve
  // the correct icon URL.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(db.GetIconMappingsForPageURL(
      result.value(),
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon},
      &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(kIconUrl5, icon_mappings[0].icon_url);
}

TEST_F(FaviconDatabaseTest, HasMappingFor) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Add a favicon which will have icon_mappings
  base::Time time = base::Time::Now();
  favicon_base::FaviconID id1 =
      db.AddFavicon(GURL("http://google.com"), favicon_base::IconType::kFavicon,
                    favicon, FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(id1, 0);

  // Add another type of favicon
  time = base::Time::Now();
  favicon_base::FaviconID id2 = db.AddFavicon(
      GURL("http://www.google.com/icon"), favicon_base::IconType::kTouchIcon,
      favicon, FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(id2, 0);

  // Add 3rd favicon
  time = base::Time::Now();
  favicon_base::FaviconID id3 = db.AddFavicon(
      GURL("http://www.google.com/icon"), favicon_base::IconType::kTouchIcon,
      favicon, FaviconBitmapType::ON_VISIT, time, gfx::Size());
  EXPECT_NE(id3, 0);

  // Add 2 icon mapping
  GURL page_url("http://www.google.com");
  EXPECT_TRUE(db.AddIconMapping(page_url, id1));
  EXPECT_TRUE(db.AddIconMapping(page_url, id2));

  EXPECT_TRUE(db.HasMappingFor(id1));
  EXPECT_TRUE(db.HasMappingFor(id2));
  EXPECT_FALSE(db.HasMappingFor(id3));

  // Remove all mappings
  db.DeleteIconMappings(page_url);
  EXPECT_FALSE(db.HasMappingFor(id1));
  EXPECT_FALSE(db.HasMappingFor(id2));
  EXPECT_FALSE(db.HasMappingFor(id3));
}

// Test loading version 3 database.
TEST_F(FaviconDatabaseTest, Version3) {
  std::unique_ptr<FaviconDatabase> db = LoadFromGolden("Favicons.v3.sql");
  ASSERT_TRUE(db);
  VerifyTablesAndColumns(&db->db_);

  // Version 3 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 4 database.
TEST_F(FaviconDatabaseTest, Version4) {
  std::unique_ptr<FaviconDatabase> db = LoadFromGolden("Favicons.v4.sql");
  ASSERT_TRUE(db);
  VerifyTablesAndColumns(&db->db_);

  // Version 4 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 5 database.
TEST_F(FaviconDatabaseTest, Version5) {
  std::unique_ptr<FaviconDatabase> db = LoadFromGolden("Favicons.v5.sql");
  ASSERT_TRUE(db);
  VerifyTablesAndColumns(&db->db_);

  // Version 5 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 6 database.
TEST_F(FaviconDatabaseTest, Version6) {
  std::unique_ptr<FaviconDatabase> db = LoadFromGolden("Favicons.v6.sql");
  ASSERT_TRUE(db);
  VerifyTablesAndColumns(&db->db_);

  // Version 6 is deprecated, the data should all be gone.
  VerifyDatabaseEmpty(&db->db_);
}

// Test loading version 7 database.
TEST_F(FaviconDatabaseTest, Version7) {
  std::unique_ptr<FaviconDatabase> db = LoadFromGolden("Favicons.v7.sql");
  ASSERT_TRUE(db);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl1,
                               favicon_base::IconType::kFavicon, kIconUrl1,
                               kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl2,
                               favicon_base::IconType::kFavicon, kIconUrl2,
                               kLargeSize, sizeof(kBlob2), kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3,
                               favicon_base::IconType::kFavicon, kIconUrl1,
                               kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3,
                               favicon_base::IconType::kTouchIcon, kIconUrl3,
                               kLargeSize, sizeof(kBlob2), kBlob2));
}

// Test loading version 8 database.
TEST_F(FaviconDatabaseTest, Version8) {
  std::unique_ptr<FaviconDatabase> db = LoadFromGolden("Favicons.v8.sql");
  ASSERT_TRUE(db);
  VerifyTablesAndColumns(&db->db_);

  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl1,
                               favicon_base::IconType::kFavicon, kIconUrl1,
                               kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl2,
                               favicon_base::IconType::kFavicon, kIconUrl2,
                               kLargeSize, sizeof(kBlob2), kBlob2));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3,
                               favicon_base::IconType::kFavicon, kIconUrl1,
                               kLargeSize, sizeof(kBlob1), kBlob1));
  EXPECT_TRUE(CheckPageHasIcon(db.get(), kPageUrl3,
                               favicon_base::IconType::kTouchIcon, kIconUrl3,
                               kLargeSize, sizeof(kBlob2), kBlob2));
}

TEST_F(FaviconDatabaseTest, Recovery) {
  // Create an example database.
  {
    EXPECT_TRUE(history::CreateDatabaseFromSQL(file_name_, "Favicons.v8.sql"));

    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    VerifyTablesAndColumns(&raw_db);
  }

  // Test that the contents make sense after clean open.
  {
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl1,
                                 favicon_base::IconType::kFavicon, kIconUrl1,
                                 kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl2,
                                 favicon_base::IconType::kFavicon, kIconUrl2,
                                 kLargeSize, sizeof(kBlob2), kBlob2));
  }

  // Corrupt the `icon_mapping.page_url` index by zeroing its root page.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }
  static const char kIndexName[] = "icon_mapping_page_url_idx";
  ASSERT_TRUE(sql::test::CorruptIndexRootPage(file_name_, kIndexName));

  // Database should be corrupt at the SQLite level.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_NE("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Open the database and access the corrupt index.
  {
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      // Accessing the index will throw SQLITE_CORRUPT. The corruption handler
      // will recover the database and poison the handle, so the outer call
      // fails.
      EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, nullptr));
      ASSERT_TRUE(expecter.SawExpectedErrors());
    }
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);
  }

  // Database should also be recovered at higher levels. Recovery should have
  // regenerated the index with no data loss.
  {
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl1,
                                 favicon_base::IconType::kFavicon, kIconUrl1,
                                 kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl2,
                                 favicon_base::IconType::kFavicon, kIconUrl2,
                                 kLargeSize, sizeof(kBlob2), kBlob2));
  }

  // Corrupt the database again by adjusting the header.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      EXPECT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Database should be recovered during open.
  {
    FaviconDatabase db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
      ASSERT_TRUE(expecter.SawExpectedErrors());
    }

    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl1,
                                 favicon_base::IconType::kFavicon, kIconUrl1,
                                 kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl2,
                                 favicon_base::IconType::kFavicon, kIconUrl2,
                                 kLargeSize, sizeof(kBlob2), kBlob2));
  }
}

TEST_F(FaviconDatabaseTest, Recovery7) {
  // Create an example database without loading into FaviconDatabase
  // (which would upgrade it).
  EXPECT_TRUE(history::CreateDatabaseFromSQL(file_name_, "Favicons.v7.sql"));

  // Corrupt the `icon_mapping.page_url` index by zeroing its root page.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }
  static const char kIndexName[] = "icon_mapping_page_url_idx";
  ASSERT_TRUE(sql::test::CorruptIndexRootPage(file_name_, kIndexName));

  // Database should be corrupt at the SQLite level.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    EXPECT_NE("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Open the database and access the corrupt index. Note that this upgrades
  // the database.
  {
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      // Accessing the index will throw SQLITE_CORRUPT. The corruption handler
      // will recover the database and poison the handle, so the outer call
      // fails.
      EXPECT_FALSE(db.GetIconMappingsForPageURL(kPageUrl2, nullptr));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
  }

  // Check that the database is recovered at the SQLite level.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);
  }

  // Database should also be recovered at higher levels. Recovery should have
  // regenerated the index with no data loss.
  {
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));

    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl1,
                                 favicon_base::IconType::kFavicon, kIconUrl1,
                                 kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl2,
                                 favicon_base::IconType::kFavicon, kIconUrl2,
                                 kLargeSize, sizeof(kBlob2), kBlob2));
  }

  // Corrupt the database again by adjusting the header.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Database should be recovered during open.
  {
    FaviconDatabase db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }

    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl1,
                                 favicon_base::IconType::kFavicon, kIconUrl1,
                                 kLargeSize, sizeof(kBlob1), kBlob1));
    EXPECT_TRUE(CheckPageHasIcon(&db, kPageUrl2,
                                 favicon_base::IconType::kFavicon, kIconUrl2,
                                 kLargeSize, sizeof(kBlob2), kBlob2));
  }
}

TEST_F(FaviconDatabaseTest, Recovery6) {
  // Create an example database without loading into FaviconDatabase
  // (which would upgrade it).
  EXPECT_TRUE(history::CreateDatabaseFromSQL(file_name_, "Favicons.v6.sql"));

  // Corrupt the database by adjusting the header.  This form of corruption will
  // cause immediate failures during Open(), before the migration code runs, so
  // the recovery code will run.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      EXPECT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Database open should succeed.
  {
    FaviconDatabase db;
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // The database should be usable at the SQLite level, with a current schema
  // and no data.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);

    // Version 6 recovery is deprecated, the data should all be gone.
    VerifyDatabaseEmpty(&raw_db);
  }
}

TEST_F(FaviconDatabaseTest, Recovery5) {
  // Create an example database without loading into FaviconDatabase
  // (which would upgrade it).
  EXPECT_TRUE(history::CreateDatabaseFromSQL(file_name_, "Favicons.v5.sql"));

  // Corrupt the database by adjusting the header.  This form of corruption will
  // cause immediate failures during Open(), before the migration code runs, so
  // the recovery code will run.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(file_name_));

  // Database is unusable at the SQLite level.
  {
    sql::Database raw_db;
    {
      sql::test::ScopedErrorExpecter expecter;
      expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
      EXPECT_FALSE(raw_db.Open(file_name_));
      EXPECT_TRUE(expecter.SawExpectedErrors());
    }
    EXPECT_EQ("ok", sql::test::IntegrityCheck(raw_db));
  }

  // Database open should succeed.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kCorrupt);
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // The database should be usable at the SQLite level, with a current schema
  // and no data.
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(file_name_));
    ASSERT_EQ("ok", sql::test::IntegrityCheck(raw_db));

    // Check that the expected tables exist.
    VerifyTablesAndColumns(&raw_db);

    // Version 5 recovery is deprecated, the data should all be gone.
    VerifyDatabaseEmpty(&raw_db);
  }
}

// Test that various broken schema found in the wild can be opened
// successfully, and result in the correct schema.
TEST_F(FaviconDatabaseTest, WildSchema) {
  base::FilePath sql_path;
  EXPECT_TRUE(history::GetTestDataHistoryDir(&sql_path));
  sql_path = sql_path.AppendASCII("thumbnail_wild");

  base::FileEnumerator fe(sql_path, false, base::FileEnumerator::FILES,
                          FILE_PATH_LITERAL("*.sql"));
  for (base::FilePath name = fe.Next(); !name.empty(); name = fe.Next()) {
    SCOPED_TRACE(name.BaseName().AsUTF8Unsafe());
    // Generate a database path based on the golden's basename.
    base::FilePath db_base_name =
        name.BaseName().ReplaceExtension(FILE_PATH_LITERAL("db"));
    base::FilePath db_path = file_name_.DirName().Append(db_base_name);
    ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path, name));

    // All schema flaws should be cleaned up by Init().
    // TODO(shess): Differentiate between databases which need Raze()
    // and those which can be salvaged.
    FaviconDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(db_path));

    // Verify that the resulting schema is correct, whether it
    // involved razing the file or fixing things in place.
    VerifyTablesAndColumns(&db.db_);
  }
}

TEST(FaviconDatabaseIconTypeTest, ShouldBeBackwardCompatible) {
  EXPECT_EQ(0, FaviconDatabase::ToPersistedIconType(
                   favicon_base::IconType::kInvalid));
  EXPECT_EQ(1, FaviconDatabase::ToPersistedIconType(
                   favicon_base::IconType::kFavicon));
  EXPECT_EQ(2, FaviconDatabase::ToPersistedIconType(
                   favicon_base::IconType::kTouchIcon));
  EXPECT_EQ(4, FaviconDatabase::ToPersistedIconType(
                   favicon_base::IconType::kTouchPrecomposedIcon));
  EXPECT_EQ(8, FaviconDatabase::ToPersistedIconType(
                   favicon_base::IconType::kWebManifestIcon));

  EXPECT_EQ(favicon_base::IconType::kInvalid,
            FaviconDatabase::FromPersistedIconType(0));
  EXPECT_EQ(favicon_base::IconType::kFavicon,
            FaviconDatabase::FromPersistedIconType(1));
  EXPECT_EQ(favicon_base::IconType::kTouchIcon,
            FaviconDatabase::FromPersistedIconType(2));
  EXPECT_EQ(favicon_base::IconType::kTouchPrecomposedIcon,
            FaviconDatabase::FromPersistedIconType(4));
  EXPECT_EQ(favicon_base::IconType::kWebManifestIcon,
            FaviconDatabase::FromPersistedIconType(8));
  EXPECT_EQ(favicon_base::IconType::kInvalid,
            FaviconDatabase::FromPersistedIconType(16));
}

TEST_F(FaviconDatabaseTest, GetFaviconsLastUpdatedBefore) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));

  // Add two favicons, 10 seconds apart. `time1` is after `time2`.
  GURL url("http://google.com");
  const base::Time time1 = base::Time::Now();
  favicon_base::FaviconID id1 =
      db.AddFavicon(url, favicon_base::IconType::kTouchIcon, favicon,
                    FaviconBitmapType::ON_VISIT, time1, gfx::Size());
  EXPECT_NE(0u, id1);

  const base::Time time2 = time1 - base::Seconds(10);
  favicon_base::FaviconID id2 =
      db.AddFavicon(url, favicon_base::IconType::kTouchIcon, favicon,
                    FaviconBitmapType::ON_VISIT, time2, gfx::Size());
  EXPECT_NE(0u, id2);
  EXPECT_NE(id1, id2);

  // There should be no favicons before `time2`.
  EXPECT_TRUE(
      db.GetFaviconsLastUpdatedBefore(time2 - base::Seconds(1), 10).empty());

  // Requesting a time after `time2` should return `id2`.
  auto ids = db.GetFaviconsLastUpdatedBefore(time2 + base::Seconds(1), 10);
  ASSERT_EQ(1u, ids.size());
  EXPECT_EQ(id2, ids[0]);

  // There should two favicons when using a time after `time1`.
  ids = db.GetFaviconsLastUpdatedBefore(time1 + base::Seconds(1), 10);
  ASSERT_EQ(2u, ids.size());
  // `id2` is before `id1`, so it should be returned first.
  EXPECT_EQ(id2, ids[0]);
  EXPECT_EQ(id1, ids[1]);

  // Repeat previous, but cap the max at 1.
  ids = db.GetFaviconsLastUpdatedBefore(time1 + base::Seconds(1), 1);
  ASSERT_EQ(1u, ids.size());
  // `id2` is before `id1`, so it should be returned first.
  EXPECT_EQ(id2, ids[0]);
}

TEST_F(FaviconDatabaseTest, SetFaviconsOutOfDateBetween) {
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();

  base::Time t1 = base::Time::Now() - base::Minutes(3);
  base::Time t2 = base::Time::Now() - base::Minutes(2);
  base::Time t3 = base::Time::Now() - base::Minutes(1);

  std::vector<unsigned char> data(kBlob1, kBlob1 + sizeof(kBlob1));
  scoped_refptr<base::RefCountedBytes> favicon(new base::RefCountedBytes(data));
  favicon_base::FaviconID icon1 =
      db.AddFavicon(GURL("http://a.example.com/favicon.ico"),
                    favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_VISIT, t1, gfx::Size());
  favicon_base::FaviconID icon2 =
      db.AddFavicon(GURL("http://b.example.com/favicon.ico"),
                    favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_VISIT, t2, gfx::Size());
  favicon_base::FaviconID icon3 =
      db.AddFavicon(GURL("http://c.example.com/favicon.ico"),
                    favicon_base::IconType::kFavicon, favicon,
                    FaviconBitmapType::ON_VISIT, t3, gfx::Size());

  EXPECT_EQ(t1, GetLastUpdated(&db, icon1));
  EXPECT_EQ(t2, GetLastUpdated(&db, icon2));
  EXPECT_EQ(t3, GetLastUpdated(&db, icon3));

  db.SetFaviconsOutOfDateBetween(t2, t3);
  EXPECT_EQ(t1, GetLastUpdated(&db, icon1));
  EXPECT_EQ(base::Time(), GetLastUpdated(&db, icon2));
  EXPECT_EQ(t3, GetLastUpdated(&db, icon3));

  db.SetFaviconsOutOfDateBetween(base::Time(), base::Time::Max());
  EXPECT_EQ(base::Time(), GetLastUpdated(&db, icon1));
  EXPECT_EQ(base::Time(), GetLastUpdated(&db, icon2));
  EXPECT_EQ(base::Time(), GetLastUpdated(&db, icon3));
}

// Test that GetFaviconIDForFaviconURL can filter by origin.
TEST_F(FaviconDatabaseTest, GetFaviconIDForFaviconURLOriginFilter) {
  // Setup DB with `kPageUrl1` mapped to `kIconUrl1`.
  FaviconDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_name_));
  db.BeginTransaction();
  scoped_refptr<base::RefCountedStaticMemory> favicon1(
      new base::RefCountedStaticMemory(kBlob1));
  const auto icon_id = db.AddFavicon(
      kIconUrl1, favicon_base::IconType::kFavicon, favicon1,
      FaviconBitmapType::ON_VISIT, base::Time::Now(), gfx::Size());
  db.AddIconMapping(kPageUrl1, icon_id);
  ASSERT_NE(0, icon_id);

  // We should be able to find the `icon_id` via the non-filtered function.
  auto icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon);
  ASSERT_EQ(icon_id, icon_id_found);

  // We should be able to find the `icon_id` via a the origin of `kPageUrl1`.
  icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon,
                                   url::Origin::Create(kPageUrl1));
  ASSERT_EQ(icon_id, icon_id_found);

  // We shouldn't be able to find the `icon_id` via a the origin of `kPageUrl2`.
  icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon,
                                   url::Origin::Create(kPageUrl2));
  ASSERT_EQ(0, icon_id_found);

  // We shouldn't be able to find the `icon_id` via a the origin of `kPageUrl3`.
  icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon,
                                   url::Origin::Create(kPageUrl3));
  ASSERT_EQ(0, icon_id_found);

  // If we map `kPageUrl2` then the situation changes.
  db.AddIconMapping(kPageUrl2, icon_id);

  // We should be able to find the `icon_id` via a the origin of `kPageUrl1`.
  icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon,
                                   url::Origin::Create(kPageUrl1));
  ASSERT_EQ(icon_id, icon_id_found);

  // We should be able to find the `icon_id` via a the origin of `kPageUrl2`.
  icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon,
                                   url::Origin::Create(kPageUrl2));
  ASSERT_EQ(icon_id, icon_id_found);

  // We shouldn't be able to find the `icon_id` via a the origin of `kPageUrl3`.
  icon_id_found =
      db.GetFaviconIDForFaviconURL(kIconUrl1, favicon_base::IconType::kFavicon,
                                   url::Origin::Create(kPageUrl3));
  ASSERT_EQ(0, icon_id_found);
}

}  // namespace favicon
