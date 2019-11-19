// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/thumbnail_database.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/url_database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace history {

// Description of database tables:
//
// icon_mapping
//   id               Unique ID.
//   page_url         Page URL which has one or more associated favicons.
//   icon_id          The ID of favicon that this mapping maps to.
//
// favicons           This table associates a row to each favicon for a
//                    |page_url| in the |icon_mapping| table. This is the
//                    default favicon |page_url|/favicon.ico plus any favicons
//                    associated via <link rel="icon_type" href="url">.
//                    The |id| matches the |icon_id| field in the appropriate
//                    row in the icon_mapping table.
//
//   id               Unique ID.
//   url              The URL at which the favicon file is located.
//   icon_type        The type of the favicon specified in the rel attribute of
//                    the link tag. The kFavicon type is used for the default
//                    favicon.ico favicon.
//
// favicon_bitmaps    This table contains the PNG encoded bitmap data of the
//                    favicons. There is a separate row for every size in a
//                    multi resolution bitmap. The bitmap data is associated
//                    to the favicon via the |icon_id| field which matches
//                    the |id| field in the appropriate row in the |favicons|
//                    table.
//
//   id               Unique ID.
//   icon_id          The ID of the favicon that the bitmap is associated to.
//   last_updated     The time at which this favicon was inserted into the
//                    table. This is used to determine if it needs to be
//                    redownloaded from the web. Value 0 denotes that the bitmap
//                    has been explicitly expired.
//   image_data       PNG encoded data of the favicon.
//   width            Pixel width of |image_data|.
//   height           Pixel height of |image_data|.
//   last_requested   The time at which this bitmap was last requested. This
//                    entry is non-zero iff the bitmap is of type ON_DEMAND.
//                    This info is used for clearing old ON_DEMAND bitmaps.
//                    (On-demand bitmaps cannot get cleared along with expired
//                    visits in history DB because there is no corresponding
//                    visit.)

namespace {

// For this database, schema migrations are deprecated after two
// years.  This means that the oldest non-deprecated version should be
// two years old or greater (thus the migrations to get there are
// older).  Databases containing deprecated versions will be cleared
// at startup.  Since this database is a cache, losing old data is not
// fatal (in fact, very old data may be expired immediately at startup
// anyhow).

// Version 8: 982ef2c1/r323176 by rogerm@chromium.org on 2015-03-31
// Version 7: 911a634d/r209424 by qsr@chromium.org on 2013-07-01
// Version 6: 610f923b/r152367 by pkotwicz@chromium.org on 2012-08-20 (depr.)
// Version 5: e2ee8ae9/r105004 by groby@chromium.org on 2011-10-12 (deprecated)
// Version 4: 5f104d76/r77288 by sky@chromium.org on 2011-03-08 (deprecated)
// Version 3: 09911bf3/r15 by initial.commit on 2008-07-26 (deprecated)

// Version number of the database.
// NOTE(shess): When changing the version, add a new golden file for
// the new version and a test to verify that Init() works with it.
const int kCurrentVersionNumber = 8;
const int kCompatibleVersionNumber = 8;
const int kDeprecatedVersionNumber = 6;  // and earlier.

void FillIconMapping(const sql::Statement& statement,
                     const GURL& page_url,
                     IconMapping* icon_mapping) {
  icon_mapping->mapping_id = statement.ColumnInt64(0);
  icon_mapping->icon_id = statement.ColumnInt64(1);
  icon_mapping->icon_type =
      ThumbnailDatabase::FromPersistedIconType(statement.ColumnInt(2));
  icon_mapping->icon_url = GURL(statement.ColumnString(3));
  icon_mapping->page_url = page_url;
}

enum InvalidStructureType {
  // NOTE(shess): Intentionally skip bucket 0 to account for
  // conversion from a boolean histogram.
  STRUCTURE_EVENT_FAVICON = 1,
  STRUCTURE_EVENT_VERSION4,
  STRUCTURE_EVENT_VERSION5,

  // Always keep this at the end.
  STRUCTURE_EVENT_MAX,
};

void RecordInvalidStructure(InvalidStructureType invalid_type) {
  UMA_HISTOGRAM_ENUMERATION("History.InvalidFaviconsDBStructure",
                            invalid_type, STRUCTURE_EVENT_MAX);
}

// NOTE(shess): Schema modifications must consider initial creation in
// |InitImpl()| and history pruning in |RetainDataForPageUrls()|.
bool InitTables(sql::Database* db) {
  static const char kIconMappingSql[] =
      "CREATE TABLE IF NOT EXISTS icon_mapping"
      "("
      "id INTEGER PRIMARY KEY,"
      "page_url LONGVARCHAR NOT NULL,"
      "icon_id INTEGER"
      ")";
  if (!db->Execute(kIconMappingSql))
    return false;

  static const char kFaviconsSql[] =
      "CREATE TABLE IF NOT EXISTS favicons"
      "("
      "id INTEGER PRIMARY KEY,"
      "url LONGVARCHAR NOT NULL,"
      // default icon_type kFavicon to be consistent with past migration.
      "icon_type INTEGER DEFAULT 1"
      ")";
  if (!db->Execute(kFaviconsSql))
    return false;

  static const char kFaviconBitmapsSql[] =
      "CREATE TABLE IF NOT EXISTS favicon_bitmaps"
      "("
      "id INTEGER PRIMARY KEY,"
      "icon_id INTEGER NOT NULL,"
      "last_updated INTEGER DEFAULT 0,"
      "image_data BLOB,"
      "width INTEGER DEFAULT 0,"
      "height INTEGER DEFAULT 0,"
      // This field is at the end so that fresh tables and migrated tables have
      // the same layout.
      "last_requested INTEGER DEFAULT 0"
      ")";
  if (!db->Execute(kFaviconBitmapsSql))
    return false;

  return true;
}

// NOTE(shess): Schema modifications must consider initial creation in
// |InitImpl()| and history pruning in |RetainDataForPageUrls()|.
bool InitIndices(sql::Database* db) {
  static const char kIconMappingUrlIndexSql[] =
      "CREATE INDEX IF NOT EXISTS icon_mapping_page_url_idx"
      " ON icon_mapping(page_url)";
  static const char kIconMappingIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS icon_mapping_icon_id_idx"
      " ON icon_mapping(icon_id)";
  if (!db->Execute(kIconMappingUrlIndexSql) ||
      !db->Execute(kIconMappingIdIndexSql)) {
    return false;
  }

  static const char kFaviconsIndexSql[] =
      "CREATE INDEX IF NOT EXISTS favicons_url ON favicons(url)";
  if (!db->Execute(kFaviconsIndexSql))
    return false;

  static const char kFaviconBitmapsIndexSql[] =
      "CREATE INDEX IF NOT EXISTS favicon_bitmaps_icon_id ON "
      "favicon_bitmaps(icon_id)";
  if (!db->Execute(kFaviconBitmapsIndexSql))
    return false;

  return true;
}

void DatabaseErrorCallback(sql::Database* db,
                           const base::FilePath& db_path,
                           HistoryBackendClient* backend_client,
                           int extended_error,
                           sql::Statement* stmt) {
  // TODO(shess): Assert that this is running on a safe thread.
  // AFAICT, should be the history thread, but at this level I can't
  // see how to reach that.

  // Attempt to recover corrupt databases.
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // NOTE(shess): This approach is valid as of version 8.  When bumping the
    // version, it will PROBABLY remain valid, but consider whether any schema
    // changes might break automated recovery.
    DCHECK_EQ(8, kCurrentVersionNumber);

    // Prevent reentrant calls.
    db->reset_error_callback();

    // TODO(shess): Is it possible/likely to have broken foreign-key
    // issues with the tables?
    // - icon_mapping.icon_id maps to no favicons.id
    // - favicon_bitmaps.icon_id maps to no favicons.id
    // - favicons.id is referenced by no icon_mapping.icon_id
    // - favicons.id is referenced by no favicon_bitmaps.icon_id
    // This step is possibly not worth the effort necessary to develop
    // and sequence the statements, as it is basically a form of garbage
    // collection.

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabaseWithMetaVersion(db, db_path);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

}  // namespace

ThumbnailDatabase::IconMappingEnumerator::IconMappingEnumerator() {
}

ThumbnailDatabase::IconMappingEnumerator::~IconMappingEnumerator() {
}

bool ThumbnailDatabase::IconMappingEnumerator::GetNextIconMapping(
    IconMapping* icon_mapping) {
  if (!statement_.Step())
    return false;
  FillIconMapping(statement_, GURL(statement_.ColumnString(4)), icon_mapping);
  return true;
}

ThumbnailDatabase::ThumbnailDatabase(HistoryBackendClient* backend_client)
    : backend_client_(backend_client) {
}

ThumbnailDatabase::~ThumbnailDatabase() {
  // The DBCloseScoper will delete the DB and the cache.
}

sql::InitStatus ThumbnailDatabase::Init(const base::FilePath& db_name) {
  // TODO(shess): Consider separating database open from schema setup.
  // With that change, this code could Raze() from outside the
  // transaction, rather than needing RazeAndClose() in InitImpl().

  // Retry failed setup in case the recovery system fixed things.
  const size_t kAttempts = 2;

  sql::InitStatus status = sql::INIT_FAILURE;
  for (size_t i = 0; i < kAttempts; ++i) {
    status = InitImpl(db_name);
    if (status == sql::INIT_OK)
      return status;

    meta_table_.Reset();
    db_.Close();
  }
  return status;
}

void ThumbnailDatabase::ComputeDatabaseMetrics() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Calculate the size of the favicon database.
  {
    sql::Statement page_count(
        db_.GetCachedStatement(SQL_FROM_HERE, "PRAGMA page_count"));
    int64_t page_count_bytes =
        page_count.Step() ? page_count.ColumnInt64(0) : 0;
    sql::Statement page_size(
        db_.GetCachedStatement(SQL_FROM_HERE, "PRAGMA page_size"));
    int64_t page_size_bytes = page_size.Step() ? page_size.ColumnInt64(0) : 0;
    int size_mb = static_cast<int>(
        (page_count_bytes * page_size_bytes) / (1024 * 1024));
    UMA_HISTOGRAM_MEMORY_MB("History.FaviconDatabaseSizeMB", size_mb);
  }

  // Count all icon URLs referenced by the DB.
  {
    sql::Statement favicon_count(
        db_.GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM favicons"));
    UMA_HISTOGRAM_COUNTS_10000(
        "History.NumFaviconsInDB",
        favicon_count.Step() ? favicon_count.ColumnInt(0) : 0);
  }

  // Count all bitmap resources cached in the DB.
  {
    sql::Statement bitmap_count(
        db_.GetCachedStatement(
            SQL_FROM_HERE, "SELECT COUNT(*) FROM favicon_bitmaps"));
    UMA_HISTOGRAM_COUNTS_10000(
        "History.NumFaviconBitmapsInDB",
        bitmap_count.Step() ? bitmap_count.ColumnInt(0) : 0);
  }

  // Count "touch" icon URLs referenced by the DB.
  {
    sql::Statement touch_icon_count(
        db_.GetCachedStatement(
            SQL_FROM_HERE,
            "SELECT COUNT(*) FROM favicons WHERE icon_type IN (?, ?)"));
    touch_icon_count.BindInt64(
        0, ToPersistedIconType(favicon_base::IconType::kTouchIcon));
    touch_icon_count.BindInt64(
        1, ToPersistedIconType(favicon_base::IconType::kTouchPrecomposedIcon));
    UMA_HISTOGRAM_COUNTS_10000(
        "History.NumTouchIconsInDB",
        touch_icon_count.Step() ? touch_icon_count.ColumnInt(0) : 0);
  }

  // Count "large" bitmap resources cached in the DB.
  {
    sql::Statement large_bitmap_count(
        db_.GetCachedStatement(
            SQL_FROM_HERE,
            "SELECT COUNT(*) FROM favicon_bitmaps WHERE width >= 64"));
    UMA_HISTOGRAM_COUNTS_10000(
        "History.NumLargeFaviconBitmapsInDB",
        large_bitmap_count.Step() ? large_bitmap_count.ColumnInt(0) : 0);
  }

  // Count all icon mappings maintained by the DB.
  {
    sql::Statement mapping_count(
        db_.GetCachedStatement(
            SQL_FROM_HERE, "SELECT COUNT(*) FROM icon_mapping"));
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "History.NumFaviconMappingsInDB",
        (mapping_count.Step() ? mapping_count.ColumnInt(0) : 0),
        1, 100000, 100);
  }

  UMA_HISTOGRAM_TIMES("History.FaviconDatabaseAdvancedMetricsTime",
                      base::TimeTicks::Now() - start_time);
}

void ThumbnailDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void ThumbnailDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void ThumbnailDatabase::RollbackTransaction() {
  db_.RollbackTransaction();
}

void ThumbnailDatabase::Vacuum() {
  DCHECK(db_.transaction_nesting() == 0) <<
      "Can not have a transaction when vacuuming.";
  ignore_result(db_.Execute("VACUUM"));
}

void ThumbnailDatabase::TrimMemory() {
  db_.TrimMemory();
}

std::map<favicon_base::FaviconID, IconMappingsForExpiry>
ThumbnailDatabase::GetOldOnDemandFavicons(base::Time threshold) {
  // Restrict to on-demand bitmaps (i.e. with last_requested != 0).
  // This is called rarely during history expiration cleanup and hence not worth
  // caching.
  sql::Statement old_icons(db_.GetUniqueStatement(
      "SELECT favicons.id, favicons.url, icon_mapping.page_url "
      "FROM favicons "
      "JOIN favicon_bitmaps ON (favicon_bitmaps.icon_id = favicons.id) "
      "JOIN icon_mapping ON (icon_mapping.icon_id = favicon_bitmaps.icon_id) "
      "WHERE (favicon_bitmaps.last_requested > 0 AND "
      "       favicon_bitmaps.last_requested < ?)"));
  old_icons.BindInt64(0, threshold.ToDeltaSinceWindowsEpoch().InMicroseconds());

  std::map<favicon_base::FaviconID, IconMappingsForExpiry> icon_mappings;

  while (old_icons.Step()) {
    favicon_base::FaviconID id = old_icons.ColumnInt64(0);
    icon_mappings[id].icon_url = GURL(old_icons.ColumnString(1));
    icon_mappings[id].page_urls.push_back(GURL(old_icons.ColumnString(2)));
  }

  return icon_mappings;
}

bool ThumbnailDatabase::GetFaviconBitmapIDSizes(
    favicon_base::FaviconID icon_id,
    std::vector<FaviconBitmapIDSize>* bitmap_id_sizes) {
  DCHECK(icon_id);
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, width, height FROM favicon_bitmaps WHERE icon_id=?"));
  statement.BindInt64(0, icon_id);

  bool result = false;
  while (statement.Step()) {
    result = true;
    if (!bitmap_id_sizes)
      return result;

    FaviconBitmapIDSize bitmap_id_size;
    bitmap_id_size.bitmap_id = statement.ColumnInt64(0);
    bitmap_id_size.pixel_size = gfx::Size(statement.ColumnInt(1),
                                          statement.ColumnInt(2));
    bitmap_id_sizes->push_back(bitmap_id_size);
  }
  return result;
}

bool ThumbnailDatabase::GetFaviconBitmaps(
    favicon_base::FaviconID icon_id,
    std::vector<FaviconBitmap>* favicon_bitmaps) {
  DCHECK(icon_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, last_updated, image_data, width, height, last_requested "
      "FROM favicon_bitmaps WHERE icon_id=?"));
  statement.BindInt64(0, icon_id);

  bool result = false;
  while (statement.Step()) {
    result = true;
    if (!favicon_bitmaps)
      return result;

    FaviconBitmap favicon_bitmap;
    favicon_bitmap.bitmap_id = statement.ColumnInt64(0);
    favicon_bitmap.icon_id = icon_id;
    favicon_bitmap.last_updated = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(statement.ColumnInt64(1)));
    if (statement.ColumnByteLength(2) > 0) {
      scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
      statement.ColumnBlobAsVector(2, &data->data());
      favicon_bitmap.bitmap_data = data;
    }
    favicon_bitmap.pixel_size = gfx::Size(statement.ColumnInt(3),
                                          statement.ColumnInt(4));
    favicon_bitmap.last_requested = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(statement.ColumnInt64(5)));
    favicon_bitmaps->push_back(favicon_bitmap);
  }
  return result;
}

bool ThumbnailDatabase::GetFaviconBitmap(
    FaviconBitmapID bitmap_id,
    base::Time* last_updated,
    base::Time* last_requested,
    scoped_refptr<base::RefCountedMemory>* png_icon_data,
    gfx::Size* pixel_size) {
  DCHECK(bitmap_id);
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT last_updated, image_data, width, height, last_requested "
      "FROM favicon_bitmaps WHERE id=?"));
  statement.BindInt64(0, bitmap_id);

  if (!statement.Step())
    return false;

  if (last_updated) {
    *last_updated = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(statement.ColumnInt64(0)));
  }

  if (png_icon_data && statement.ColumnByteLength(1) > 0) {
    scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
    statement.ColumnBlobAsVector(1, &data->data());
    *png_icon_data = data;
  }

  if (pixel_size) {
    *pixel_size = gfx::Size(statement.ColumnInt(2),
                            statement.ColumnInt(3));
  }

  if (last_requested) {
    *last_requested = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(statement.ColumnInt64(4)));
  }

  return true;
}

FaviconBitmapID ThumbnailDatabase::AddFaviconBitmap(
    favicon_base::FaviconID icon_id,
    const scoped_refptr<base::RefCountedMemory>& icon_data,
    FaviconBitmapType type,
    base::Time time,
    const gfx::Size& pixel_size) {
  DCHECK(icon_id);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO favicon_bitmaps (icon_id, image_data, last_updated, "
      "last_requested, width, height) VALUES (?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, icon_id);
  if (icon_data.get() && icon_data->size()) {
    statement.BindBlob(1, icon_data->front(),
                       static_cast<int>(icon_data->size()));
  } else {
    statement.BindNull(1);
  }

  // On-visit bitmaps:
  //  - keep track of last_updated: last write time is used for expiration;
  //  - always have last_requested==0: no need to keep track of last read time.
  statement.BindInt64(2, type == ON_VISIT
                             ? time.ToDeltaSinceWindowsEpoch().InMicroseconds()
                             : 0);
  // On-demand bitmaps:
  //  - always have last_updated==0: last write time is not stored as they are
  //    always expired and thus ready to be replaced by ON_VISIT icons;
  //  - keep track of last_requested: last read time is used for cache eviction.
  statement.BindInt64(3, type == ON_DEMAND
                             ? time.ToDeltaSinceWindowsEpoch().InMicroseconds()
                             : 0);

  statement.BindInt(4, pixel_size.width());
  statement.BindInt(5, pixel_size.height());

  if (!statement.Run())
    return 0;
  return db_.GetLastInsertRowId();
}

bool ThumbnailDatabase::SetFaviconBitmap(
    FaviconBitmapID bitmap_id,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    base::Time time) {
  DCHECK(bitmap_id);
  // By updating last_updated timestamp, we assume the icon is of type ON_VISIT.
  // If it is ON_DEMAND, reset last_requested to 0 and thus silently change the
  // type to ON_VISIT.
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "UPDATE favicon_bitmaps SET image_data=?, "
                             "last_updated=?, last_requested=? WHERE id=?"));
  if (bitmap_data.get() && bitmap_data->size()) {
    statement.BindBlob(0, bitmap_data->front(),
                       static_cast<int>(bitmap_data->size()));
  } else {
    statement.BindNull(0);
  }
  statement.BindInt64(1, time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  statement.BindInt64(2, 0);
  statement.BindInt64(3, bitmap_id);

  return statement.Run();
}

bool ThumbnailDatabase::SetFaviconBitmapLastUpdateTime(
    FaviconBitmapID bitmap_id,
    base::Time time) {
  DCHECK(bitmap_id);
  // By updating last_updated timestamp, we assume the icon is of type ON_VISIT.
  // If it is ON_DEMAND, reset last_requested to 0 and thus silently change the
  // type to ON_VISIT.
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "UPDATE favicon_bitmaps SET last_updated=?, "
                             "last_requested=? WHERE id=?"));
  statement.BindInt64(0, time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  statement.BindInt64(1, 0);
  statement.BindInt64(2, bitmap_id);
  return statement.Run();
}

bool ThumbnailDatabase::TouchOnDemandFavicon(const GURL& icon_url,
                                             base::Time time) {
  // Look up the icon ids for the url.
  sql::Statement id_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM favicons WHERE url=?"));
  id_statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));

  base::Time max_time =
      time - base::TimeDelta::FromDays(kFaviconUpdateLastRequestedAfterDays);

  while (id_statement.Step()) {
    favicon_base::FaviconID icon_id = id_statement.ColumnInt64(0);

    // Update the time only for ON_DEMAND bitmaps (i.e. with last_requested >
    // 0). For performance reasons, update the time only if the currently stored
    // time is old enough (UPDATEs where the WHERE condition does not match any
    // entries are way faster than UPDATEs that really change some data).
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE favicon_bitmaps SET last_requested=? WHERE icon_id=? AND "
        "last_requested>0 AND last_requested<=?"));
    statement.BindInt64(0, time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    statement.BindInt64(1, icon_id);
    statement.BindInt64(2,
                        max_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    if (!statement.Run())
      return false;
  }
  return true;
}

bool ThumbnailDatabase::DeleteFaviconBitmap(FaviconBitmapID bitmap_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicon_bitmaps WHERE id=?"));
  statement.BindInt64(0, bitmap_id);
  return statement.Run();
}

bool ThumbnailDatabase::SetFaviconOutOfDate(favicon_base::FaviconID icon_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE favicon_bitmaps SET last_updated=? WHERE icon_id=?"));
  statement.BindInt64(0, 0);
  statement.BindInt64(1, icon_id);

  return statement.Run();
}

bool ThumbnailDatabase::GetFaviconLastUpdatedTime(
    favicon_base::FaviconID icon_id,
    base::Time* last_updated) {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT MAX(last_updated) FROM favicon_bitmaps WHERE icon_id=?"));
  statement.BindInt64(0, icon_id);

  if (!statement.Step())
    return false;

  // Return false also if there there is no bitmap with |icon_id|.
  if (statement.GetColumnType(0) == sql::ColumnType::kNull)
    return false;

  if (last_updated) {
    *last_updated = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(statement.ColumnInt64(0)));
  }
  return true;
}

favicon_base::FaviconID ThumbnailDatabase::GetFaviconIDForFaviconURL(
    const GURL& icon_url,
    favicon_base::IconType icon_type) {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM favicons WHERE url=? AND icon_type=?"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  statement.BindInt(1, ToPersistedIconType(icon_type));

  if (!statement.Step())
    return 0;  // not cached

  return statement.ColumnInt64(0);
}

bool ThumbnailDatabase::GetFaviconHeader(favicon_base::FaviconID icon_id,
                                         GURL* icon_url,
                                         favicon_base::IconType* icon_type) {
  DCHECK(icon_id);

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT url, icon_type FROM favicons WHERE id=?"));
  statement.BindInt64(0, icon_id);

  if (!statement.Step())
    return false;  // No entry for the id.

  if (icon_url)
    *icon_url = GURL(statement.ColumnString(0));
  if (icon_type)
    *icon_type = FromPersistedIconType(statement.ColumnInt(1));

  return true;
}

favicon_base::FaviconID ThumbnailDatabase::AddFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type) {

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO favicons (url, icon_type) VALUES (?, ?)"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(icon_url));
  statement.BindInt(1, ToPersistedIconType(icon_type));

  if (!statement.Run())
    return 0;
  return db_.GetLastInsertRowId();
}

favicon_base::FaviconID ThumbnailDatabase::AddFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const scoped_refptr<base::RefCountedMemory>& icon_data,
    FaviconBitmapType type,
    base::Time time,
    const gfx::Size& pixel_size) {
  favicon_base::FaviconID icon_id = AddFavicon(icon_url, icon_type);
  if (!icon_id || !AddFaviconBitmap(icon_id, icon_data, type, time, pixel_size))
    return 0;

  return icon_id;
}

bool ThumbnailDatabase::DeleteFavicon(favicon_base::FaviconID id) {
  sql::Statement statement;
  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicons WHERE id = ?"));
  statement.BindInt64(0, id);
  if (!statement.Run())
    return false;

  statement.Assign(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM favicon_bitmaps WHERE icon_id = ?"));
  statement.BindInt64(0, id);
  return statement.Run();
}

bool ThumbnailDatabase::GetIconMappingsForPageURL(
    const GURL& page_url,
    const favicon_base::IconTypeSet& required_icon_types,
    std::vector<IconMapping>* filtered_mapping_data) {
  std::vector<IconMapping> mapping_data;
  if (!GetIconMappingsForPageURL(page_url, &mapping_data))
    return false;

  bool result = false;
  for (auto m = mapping_data.begin(); m != mapping_data.end(); ++m) {
    if (required_icon_types.count(m->icon_type) != 0) {
      result = true;
      if (!filtered_mapping_data)
        return result;

      filtered_mapping_data->push_back(*m);
    }
  }
  return result;
}

bool ThumbnailDatabase::GetIconMappingsForPageURL(
    const GURL& page_url,
    std::vector<IconMapping>* mapping_data) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT icon_mapping.id, icon_mapping.icon_id, favicons.icon_type, "
      "favicons.url "
      "FROM icon_mapping "
      "INNER JOIN favicons "
      "ON icon_mapping.icon_id = favicons.id "
      "WHERE icon_mapping.page_url=? "
      "ORDER BY favicons.icon_type DESC"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(page_url));

  bool result = false;
  while (statement.Step()) {
    result = true;
    if (!mapping_data)
      return result;

    IconMapping icon_mapping;
    FillIconMapping(statement, page_url, &icon_mapping);
    mapping_data->push_back(icon_mapping);
  }
  return result;
}

base::Optional<GURL> ThumbnailDatabase::FindFirstPageURLForHost(
    const GURL& url,
    const favicon_base::IconTypeSet& required_icon_types) {
  if (url.host().empty())
    return base::nullopt;

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "SELECT icon_mapping.page_url, favicons.icon_type "
                             "FROM icon_mapping "
                             "INNER JOIN favicons "
                             "ON icon_mapping.icon_id = favicons.id "
                             "WHERE icon_mapping.page_url LIKE ? "
                             "ORDER BY favicons.icon_type DESC"));

  // Bind the host with a prefix of "://" and suffix of "/" to ensure the entire
  // host name is matched.
  statement.BindString(0, base::StringPrintf("%%://%s/%%", url.host().c_str()));

  while (statement.Step()) {
    favicon_base::IconType icon_type =
        ThumbnailDatabase::FromPersistedIconType(statement.ColumnInt(1));

    if (required_icon_types.count(icon_type) != 0)
      return base::make_optional(GURL(statement.ColumnString(0)));
  }
  return base::nullopt;
}

IconMappingID ThumbnailDatabase::AddIconMapping(
    const GURL& page_url,
    favicon_base::FaviconID icon_id) {
  static const char kSql[] =
      "INSERT INTO icon_mapping (page_url, icon_id) VALUES (?, ?)";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(page_url));
  statement.BindInt64(1, icon_id);

  if (!statement.Run())
    return 0;

  return db_.GetLastInsertRowId();
}

bool ThumbnailDatabase::DeleteIconMappings(const GURL& page_url) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM icon_mapping WHERE page_url = ?"));
  statement.BindString(0, URLDatabase::GURLToDatabaseURL(page_url));

  return statement.Run();
}

bool ThumbnailDatabase::DeleteIconMappingsForFaviconId(
    favicon_base::FaviconID id) {
  // This is called rarely during history expiration cleanup and hence not
  // worth caching.
  sql::Statement statement(
      db_.GetUniqueStatement("DELETE FROM icon_mapping WHERE icon_id=?"));
  statement.BindInt64(0, id);
  return statement.Run();
}

bool ThumbnailDatabase::DeleteIconMapping(IconMappingID mapping_id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM icon_mapping WHERE id=?"));
  statement.BindInt64(0, mapping_id);

  return statement.Run();
}

bool ThumbnailDatabase::HasMappingFor(favicon_base::FaviconID id) {
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM icon_mapping "
      "WHERE icon_id=?"));
  statement.BindInt64(0, id);

  return statement.Step();
}

bool ThumbnailDatabase::InitIconMappingEnumerator(
    favicon_base::IconType type,
    IconMappingEnumerator* enumerator) {
  DCHECK(!enumerator->statement_.is_valid());
  enumerator->statement_.Assign(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT icon_mapping.id, icon_mapping.icon_id, favicons.icon_type, "
             "favicons.url, icon_mapping.page_url "
         "FROM icon_mapping JOIN favicons ON ("
              "icon_mapping.icon_id = favicons.id) "
         "WHERE favicons.icon_type = ?"));
  enumerator->statement_.BindInt(0, ToPersistedIconType(type));
  return enumerator->statement_.is_valid();
}

bool ThumbnailDatabase::RetainDataForPageUrls(
    const std::vector<GURL>& urls_to_keep) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  // Populate temp.retained_urls with |urls_to_keep|.
  {
    static const char kCreateRetainedUrls[] =
        "CREATE TEMP TABLE retained_urls (url LONGVARCHAR PRIMARY KEY)";
    if (!db_.Execute(kCreateRetainedUrls))
      return false;

    static const char kRetainedUrlSql[] =
        "INSERT OR IGNORE INTO temp.retained_urls (url) VALUES (?)";
    sql::Statement statement(db_.GetUniqueStatement(kRetainedUrlSql));
    for (const GURL& url : urls_to_keep) {
      statement.BindString(0, URLDatabase::GURLToDatabaseURL(url));
      if (!statement.Run())
        return false;
      statement.Reset(true);
    }
  }

  // temp.icon_id_mapping generates new icon ids as consecutive
  // integers starting from 1, and maps them to the old icon ids.
  {
    static const char kIconMappingCreate[] =
        "CREATE TEMP TABLE icon_id_mapping "
        "("
        "new_icon_id INTEGER PRIMARY KEY,"
        "old_icon_id INTEGER NOT NULL UNIQUE"
        ")";
    if (!db_.Execute(kIconMappingCreate))
      return false;

    // Insert the icon ids for retained urls, skipping duplicates.
    static const char kIconMappingSql[] =
        "INSERT OR IGNORE INTO temp.icon_id_mapping (old_icon_id) "
        "SELECT icon_id FROM icon_mapping "
        "JOIN temp.retained_urls "
        "ON (temp.retained_urls.url = icon_mapping.page_url)";
    if (!db_.Execute(kIconMappingSql))
      return false;
  }

  static const char kRenameIconMappingTable[] =
      "ALTER TABLE icon_mapping RENAME TO old_icon_mapping";
  static const char kCopyIconMapping[] =
      "INSERT INTO icon_mapping (page_url, icon_id) "
      "SELECT temp.retained_urls.url, mapping.new_icon_id "
      "FROM temp.retained_urls "
      "JOIN old_icon_mapping AS old "
      "ON (temp.retained_urls.url = old.page_url) "
      "JOIN temp.icon_id_mapping AS mapping "
      "ON (old.icon_id = mapping.old_icon_id)";
  static const char kDropOldIconMappingTable[] = "DROP TABLE old_icon_mapping";

  static const char kRenameFaviconsTable[] =
      "ALTER TABLE favicons RENAME TO old_favicons";
  static const char kCopyFavicons[] =
      "INSERT INTO favicons (id, url, icon_type) "
      "SELECT mapping.new_icon_id, old.url, old.icon_type "
      "FROM old_favicons AS old "
      "JOIN temp.icon_id_mapping AS mapping "
      "ON (old.id = mapping.old_icon_id)";
  static const char kDropOldFaviconsTable[] = "DROP TABLE old_favicons";

  // Set the retained favicon bitmaps to be expired (last_updated == 0).
  // The user may be deleting their favicon bitmaps because the favicon bitmaps
  // are incorrect. Expiring a favicon bitmap causes it to be redownloaded when
  // the user visits a page associated with the favicon bitmap. See
  // crbug.com/474421 for an example of a bug which caused favicon bitmaps to
  // become incorrect.
  static const char kRenameFaviconBitmapsTable[] =
      "ALTER TABLE favicon_bitmaps RENAME TO old_favicon_bitmaps";
  static const char kCopyFaviconBitmaps[] =
      "INSERT INTO favicon_bitmaps "
      "  (icon_id, last_updated, image_data, width, height, last_requested) "
      "SELECT mapping.new_icon_id, 0, old.image_data, old.width, old.height,"
      "    old.last_requested "
      "FROM old_favicon_bitmaps AS old "
      "JOIN temp.icon_id_mapping AS mapping "
      "ON (old.icon_id = mapping.old_icon_id)";
  static const char kDropOldFaviconBitmapsTable[] =
      "DROP TABLE old_favicon_bitmaps";

  // Rename existing tables to new location.
  if (!db_.Execute(kRenameIconMappingTable) ||
      !db_.Execute(kRenameFaviconsTable) ||
      !db_.Execute(kRenameFaviconBitmapsTable)) {
    return false;
  }

  // Initialize the replacement tables.  At this point the old indices
  // still exist (pointing to the old_* tables), so do not initialize
  // the indices.
  if (!InitTables(&db_))
    return false;

  // Copy all of the data over.
  if (!db_.Execute(kCopyIconMapping) ||
      !db_.Execute(kCopyFavicons) ||
      !db_.Execute(kCopyFaviconBitmaps)) {
    return false;
  }

  // Drop the old_* tables, which also drops the indices.
  if (!db_.Execute(kDropOldIconMappingTable) ||
      !db_.Execute(kDropOldFaviconsTable) ||
      !db_.Execute(kDropOldFaviconBitmapsTable)) {
    return false;
  }

  // Recreate the indices.
  // TODO(shess): UNIQUE indices could fail due to duplication.  This
  // could happen in case of corruption.
  if (!InitIndices(&db_))
    return false;

  static const char kIconMappingDrop[] = "DROP TABLE temp.icon_id_mapping";
  static const char kRetainedUrlsDrop[] = "DROP TABLE temp.retained_urls";
  if (!db_.Execute(kIconMappingDrop) || !db_.Execute(kRetainedUrlsDrop))
    return false;

  return transaction.Commit();
}

// static
int ThumbnailDatabase::ToPersistedIconType(favicon_base::IconType icon_type) {
  if (icon_type == favicon_base::IconType::kInvalid)
    return 0;

  return 1 << (static_cast<int>(icon_type) - 1);
}

// static
favicon_base::IconType ThumbnailDatabase::FromPersistedIconType(int icon_type) {
  if (icon_type == 0)
    return favicon_base::IconType::kInvalid;

  int val = 1 + base::bits::Log2Floor(icon_type);
  if (val > static_cast<int>(favicon_base::IconType::kMax))
    return favicon_base::IconType::kInvalid;

  return static_cast<favicon_base::IconType>(val);
}

sql::InitStatus ThumbnailDatabase::OpenDatabase(sql::Database* db,
                                                const base::FilePath& db_name) {
  db->set_histogram_tag("Thumbnail");
  db->set_error_callback(base::Bind(&DatabaseErrorCallback,
                                    db, db_name, backend_client_));

  // Thumbnails db now only stores favicons, so we don't need that big a page
  // size or cache.
  db->set_page_size(2048);
  db->set_cache_size(32);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db->set_exclusive_locking();

  if (!db->Open(db_name))
    return sql::INIT_FAILURE;
  db->Preload();

  return sql::INIT_OK;
}

sql::InitStatus ThumbnailDatabase::InitImpl(const base::FilePath& db_name) {
  sql::InitStatus status = OpenDatabase(&db_, db_name);
  if (status != sql::INIT_OK)
    return status;

  // Clear databases which are too old to process.
  DCHECK_LT(kDeprecatedVersionNumber, kCurrentVersionNumber);
  sql::MetaTable::RazeIfDeprecated(&db_, kDeprecatedVersionNumber);

  // TODO(shess): Sqlite.Version.Thumbnail shows versions 22, 23, and
  // 25.  Future versions are not destroyed because that could lead to
  // data loss if the profile is opened by a later channel, but
  // perhaps a heuristic like >kCurrentVersionNumber+3 could be used.

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // TODO(shess): Failing Begin() implies that something serious is
  // wrong with the database.  Raze() may be in order.

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Exclude the thumbnails file from backups.
  base::mac::SetFileBackupExclusion(db_name);
#endif

  // thumbnails table has been obsolete for a long time, remove any detritus.
  ignore_result(db_.Execute("DROP TABLE IF EXISTS thumbnails"));

  // At some point, operations involving temporary tables weren't done
  // atomically and users have been stranded.  Drop those tables and
  // move on.
  // TODO(shess): Prove it?  Audit all cases and see if it's possible
  // that this implies non-atomic update, and should thus be handled
  // via the corruption handler.
  ignore_result(db_.Execute("DROP TABLE IF EXISTS temp_favicons"));
  ignore_result(db_.Execute("DROP TABLE IF EXISTS temp_favicon_bitmaps"));
  ignore_result(db_.Execute("DROP TABLE IF EXISTS temp_icon_mapping"));

  // Create the tables.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber) ||
      !InitTables(&db_) ||
      !InitIndices(&db_)) {
    return sql::INIT_FAILURE;
  }

  // Version check. We should not encounter a database too old for us to handle
  // in the wild, so we try to continue in that case.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Thumbnail database is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();

  if (!db_.DoesColumnExist("favicons", "icon_type")) {
    LOG(ERROR) << "Raze because of missing favicon.icon_type";
    RecordInvalidStructure(STRUCTURE_EVENT_VERSION4);

    db_.RazeAndClose();
    return sql::INIT_FAILURE;
  }

  if (cur_version < 7 && !db_.DoesColumnExist("favicons", "sizes")) {
    LOG(ERROR) << "Raze because of missing favicon.sizes";
    RecordInvalidStructure(STRUCTURE_EVENT_VERSION5);

    db_.RazeAndClose();
    return sql::INIT_FAILURE;
  }

  if (cur_version == 6) {
    ++cur_version;
    if (!UpgradeToVersion7())
      return CantUpgradeToVersion(cur_version);
  }

  if (cur_version == 7) {
    ++cur_version;
    if (!UpgradeToVersion8())
      return CantUpgradeToVersion(cur_version);
  }

  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Thumbnail database version " << cur_version << " is too old to handle.";

  // Initialization is complete.
  if (!transaction.Commit())
    return sql::INIT_FAILURE;

  // Raze the database if the structure of the favicons database is not what
  // it should be. This error cannot be detected via the SQL error code because
  // the error code for running SQL statements against a database with missing
  // columns is SQLITE_ERROR which is not unique enough to act upon.
  // TODO(pkotwicz): Revisit this in M27 and see if the razing can be removed.
  // (crbug.com/166453)
  if (IsFaviconDBStructureIncorrect()) {
    LOG(ERROR) << "Raze because of invalid favicon db structure.";
    RecordInvalidStructure(STRUCTURE_EVENT_FAVICON);

    db_.RazeAndClose();
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

sql::InitStatus ThumbnailDatabase::CantUpgradeToVersion(int cur_version) {
  LOG(WARNING) << "Unable to update to thumbnail database to version " <<
               cur_version << ".";
  db_.Close();
  return sql::INIT_FAILURE;
}

bool ThumbnailDatabase::UpgradeToVersion7() {
  // Sizes column was never used, remove it.
  bool success =
      db_.Execute(
          "CREATE TABLE temp_favicons ("
          "id INTEGER PRIMARY KEY,"
          "url LONGVARCHAR NOT NULL,"
          // default icon_type kFavicon to be consistent with
          // past migration.
          "icon_type INTEGER DEFAULT 1)") &&
      db_.Execute(
          "INSERT INTO temp_favicons (id, url, icon_type) "
          "SELECT id, url, icon_type FROM favicons") &&
      db_.Execute("DROP TABLE favicons") &&
      db_.Execute("ALTER TABLE temp_favicons RENAME TO favicons") &&
      db_.Execute("CREATE INDEX IF NOT EXISTS favicons_url ON favicons(url)");

  if (!success)
    return false;

  meta_table_.SetVersionNumber(7);
  meta_table_.SetCompatibleVersionNumber(std::min(7, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::UpgradeToVersion8() {
  // Add the last_requested column to the favicon_bitmaps table.
  static const char kFaviconBitmapsAddLastRequestedSql[] =
      "ALTER TABLE favicon_bitmaps ADD COLUMN last_requested INTEGER DEFAULT 0";
  if (!db_.Execute(kFaviconBitmapsAddLastRequestedSql))
    return false;

  meta_table_.SetVersionNumber(8);
  meta_table_.SetCompatibleVersionNumber(std::min(8, kCompatibleVersionNumber));
  return true;
}

bool ThumbnailDatabase::IsFaviconDBStructureIncorrect() {
  return !db_.IsSQLValid("SELECT id, url, icon_type FROM favicons");
}

}  // namespace history
