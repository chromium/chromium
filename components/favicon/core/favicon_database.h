// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DATABASE_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DATABASE_H_

#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "components/favicon/core/favicon_types.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace base {
class FilePath;
class RefCountedMemory;
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace favicon {

// The minimum number of days after which last_requested field gets updated.
// All earlier updates are ignored.
static const int kFaviconUpdateLastRequestedAfterDays = 10;

// This database interface is owned by the history backend and runs on the
// history thread.
class FaviconDatabase {
 public:
  FaviconDatabase();
  ~FaviconDatabase();

  // Must be called after creation but before any other methods are called.
  // When not INIT_OK, no other functions should be called.
  sql::InitStatus Init(const base::FilePath& db_name);

  // Computes and records various metrics for the database. Should only be
  // called once and only upon successful Init.
  void ComputeDatabaseMetrics();

  // Transactions on the database.
  void BeginTransaction();
  void CommitTransaction();
  int transaction_nesting() const { return db_.transaction_nesting(); }
  void RollbackTransaction();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Release all non-essential memory associated with this database connection.
  void TrimMemory();

  // Get all on-demand favicon bitmaps that have been last requested prior to
  // `threshold`.
  std::map<favicon_base::FaviconID, IconMappingsForExpiry>
  GetOldOnDemandFavicons(base::Time threshold);

  // Favicon Bitmaps -----------------------------------------------------------

  // Returns true if there are favicon bitmaps for `icon_id`. If
  // `bitmap_id_sizes` is non NULL, sets it to a list of the favicon bitmap ids
  // and their associated pixel sizes for the favicon with `icon_id`.
  // The list contains results for the bitmaps which are cached in the
  // favicon_bitmaps table. The pixel sizes are a subset of the sizes in the
  // 'sizes' field of the favicons table for `icon_id`.
  bool GetFaviconBitmapIDSizes(
      favicon_base::FaviconID icon_id,
      std::vector<FaviconBitmapIDSize>* bitmap_id_sizes);

  // Returns true if there are any matched bitmaps for the given `icon_id`. All
  // matched results are returned if `favicon_bitmaps` is not NULL.
  bool GetFaviconBitmaps(favicon_base::FaviconID icon_id,
                         std::vector<FaviconBitmap>* favicon_bitmaps);

  // Gets the last updated time, bitmap data, and pixel size of the favicon
  // bitmap at `bitmap_id`. Returns true if successful.
  bool GetFaviconBitmap(FaviconBitmapID bitmap_id,
                        base::Time* last_updated,
                        base::Time* last_requested,
                        scoped_refptr<base::RefCountedMemory>* png_icon_data,
                        gfx::Size* pixel_size);

  // Adds a bitmap component of `type` at `pixel_size` for the favicon with
  // `icon_id`. Only favicons representing a .ico file should have multiple
  // favicon bitmaps per favicon.
  // `icon_data` is the png encoded data.
  // The `type` indicates how the lifetime of this icon should be managed.
  // The `time` is used for lifetime management of the bitmap (should be Now()).
  // `pixel_size` is the pixel dimensions of `icon_data`.
  // Returns the id of the added bitmap or 0 if unsuccessful.
  FaviconBitmapID AddFaviconBitmap(
      favicon_base::FaviconID icon_id,
      const scoped_refptr<base::RefCountedMemory>& icon_data,
      FaviconBitmapType type,
      base::Time time,
      const gfx::Size& pixel_size);

  // Sets the bitmap data and the last updated time for the favicon bitmap at
  // `bitmap_id`. Should not be called for bitmaps of type ON_DEMAND as they
  // should never get updated (the call silently changes the type to ON_VISIT).
  // Returns true if successful.
  bool SetFaviconBitmap(FaviconBitmapID bitmap_id,
                        scoped_refptr<base::RefCountedMemory> bitmap_data,
                        base::Time time);

  // Sets the last_updated time for the favicon bitmap at `bitmap_id`. Should
  // not be called for bitmaps of type ON_DEMAND as last_updated time is only
  // tracked for ON_VISIT bitmaps (the call silently changes the type to
  // ON_VISIT). Returns true if successful.
  bool SetFaviconBitmapLastUpdateTime(FaviconBitmapID bitmap_id,
                                      base::Time time);

  // Deletes the favicon bitmap with `bitmap_id`.
  // Returns true if successful.
  bool DeleteFaviconBitmap(FaviconBitmapID bitmap_id);

  // Favicons ------------------------------------------------------------------

  // Sets the the favicon as out of date. This will set `last_updated` for all
  // of the bitmaps for `icon_id` to be out of date.
  bool SetFaviconOutOfDate(favicon_base::FaviconID icon_id);

  // Mark all favicons as out of date that have been modified at or after
  // `begin` and before `end`. This will set `last_updated` for all matching
  // bitmaps to be out of date.
  bool SetFaviconsOutOfDateBetween(base::Time begin, base::Time end);

  // Retrieves the newest `last_updated` time across all bitmaps for `icon_id`.
  // Returns true if successful and if there is at least one bitmap.
  bool GetFaviconLastUpdatedTime(favicon_base::FaviconID icon_id,
                                 base::Time* last_updated);

  // Mark all bitmaps of type ON_DEMAND at `icon_url` as requested at `time`.
  // This postpones their automatic eviction from the database. Not all calls
  // end up in a write into the DB:
  // - it is no-op if the bitmaps are not of type ON_DEMAND;
  // - the updates of the "last requested time" have limited frequency for each
  //   particular bitmap (e.g. once per week). This limits the overhead of
  //   cache management for on-demand favicons.
  // Returns true if successful.
  bool TouchOnDemandFavicon(const GURL& icon_url, base::Time time);

  // Returns the id of the entry in the favicon database with the specified
  // `icon_url` and `icon_type` that has an existing mapping to `page_origin`
  // (and 0 if no entry exists). See crbug.com/1300214 for more context.
  favicon_base::FaviconID GetFaviconIDForFaviconURL(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const url::Origin& page_origin);

  // Returns the id of the entry in the favicon database with the specified
  // `icon_url` and `icon_type` (and 0 if no entry exists). This function does
  // not respect cross-origin partitioning and returns an entry from the cache
  // without verifying it was stored for the origin requesting it. This can leak
  // navigation history, see crbug.com/1300214 for more context.
  favicon_base::FaviconID GetFaviconIDForFaviconURL(
      const GURL& icon_url,
      favicon_base::IconType icon_type);

  // Gets the icon_url, icon_type and sizes for the specified `icon_id`.
  bool GetFaviconHeader(favicon_base::FaviconID icon_id,
                        GURL* icon_url,
                        favicon_base::IconType* icon_type);

  // Adds favicon with `icon_url`, `icon_type` and `favicon_sizes` to the
  // favicon db, returning its id.
  favicon_base::FaviconID AddFavicon(const GURL& icon_url,
                                     favicon_base::IconType icon_type);

  // Adds a favicon with a single bitmap. This call is equivalent to calling
  // AddFavicon and AddFaviconBitmap of type `type`.
  favicon_base::FaviconID AddFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const scoped_refptr<base::RefCountedMemory>& icon_data,
      FaviconBitmapType type,
      base::Time time,
      const gfx::Size& pixel_size);

  // Delete the favicon with the provided id. Returns false on failure
  bool DeleteFavicon(favicon_base::FaviconID id);

  // Icon Mapping --------------------------------------------------------------
  //
  // Returns true if there is a matched icon mapping for the given page and
  // icon type.
  // The matched icon mapping is returned in the icon_mapping parameter if it is
  // not NULL.

  // Returns true if there are icon mappings for the given page and icon types.
  // The matched icon mappings are returned in the `mapping_data` parameter if
  // it is not NULL.
  bool GetIconMappingsForPageURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& required_icon_types,
      std::vector<IconMapping>* mapping_data);

  // Returns true if there is any matched icon mapping for the given page.
  // All matched icon mappings are returned in descent order of IconType if
  // mapping_data is not NULL.
  bool GetIconMappingsForPageURL(const GURL& page_url,
                                 std::vector<IconMapping>* mapping_data);

  // Given `url`, returns the `page_url` page mapped to an icon with
  // `required_icon_types`, where `page_url` has host = url.host(). This allows
  // for icons to be retrieved when a full URL is not available. For example,
  // `url` = http://www.google.com would match
  // `page_url` = https://www.google.com/search. The returned optional will be
  // empty if no such `page_url` exists.
  std::optional<GURL> FindFirstPageURLForHost(
      const GURL& url,
      const favicon_base::IconTypeSet& required_icon_types);

  // Adds a mapping between the given page_url and icon_id.
  // Returns the new mapping id if the adding succeeds, otherwise 0 is returned.
  IconMappingID AddIconMapping(const GURL& page_url,
                               favicon_base::FaviconID icon_id);

  // Deletes the icon mapping entries for the given page url.
  // Returns true if the deletion succeeded.
  bool DeleteIconMappings(const GURL& page_url);

  // Deletes the icon mapping entries for the given favicon ID.
  // Returns true if the deletion succeeded.
  bool DeleteIconMappingsForFaviconId(favicon_base::FaviconID id);

  // Deletes the icon mapping with `mapping_id`.
  // Returns true if the deletion succeeded.
  bool DeleteIconMapping(IconMappingID mapping_id);

  // Checks whether a favicon is used by any URLs in the database.
  bool HasMappingFor(favicon_base::FaviconID id);

  // Returns the ids of favicons which were last updated before `time`. This
  // returns at most `max_count` ids.
  std::vector<favicon_base::FaviconID> GetFaviconsLastUpdatedBefore(
      base::Time time,
      int max_count);

  // The class to enumerate icon mappings. Use InitIconMappingEnumerator to
  // initialize.
  class IconMappingEnumerator {
   public:
    IconMappingEnumerator();

    IconMappingEnumerator(const IconMappingEnumerator&) = delete;
    IconMappingEnumerator& operator=(const IconMappingEnumerator&) = delete;

    ~IconMappingEnumerator();

    // Get the next icon mapping, return false if no more are available.
    bool GetNextIconMapping(IconMapping* icon_mapping);

   private:
    friend class FaviconDatabase;

    // Used to query database and return the data for filling IconMapping in
    // each call of GetNextIconMapping().
    sql::Statement statement_;
  };

  // Return all icon mappings of the given `icon_type`.
  bool InitIconMappingEnumerator(favicon_base::IconType type,
                                 IconMappingEnumerator* enumerator);

  // Remove all data except that associated with the passed page urls.
  // Returns false in case of failure.  A nested transaction is used,
  // so failure causes any outer transaction to be rolled back.
  bool RetainDataForPageUrls(const std::vector<GURL>& urls_to_keep);

  // For historical reasons, and for backward compatibility, the icon type
  // values stored in the DB are powers of two. Conversion functions
  // exposed publicly for testing.
  static int ToPersistedIconType(favicon_base::IconType icon_type);
  static favicon_base::IconType FromPersistedIconType(int icon_type);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, RetainDataForPageUrls);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest,
                           RetainDataForPageUrlsExpiresRetainedFavicons);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, Version3);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, Version4);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, Version5);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, Version6);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, Version7);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, Version8);
  FRIEND_TEST_ALL_PREFIXES(FaviconDatabaseTest, WildSchema);

  // Open database on a given filename. If the file does not exist,
  // it is created.
  // `db` is the database to open.
  // `db_name` is a path to the database file.
  sql::InitStatus OpenDatabase(sql::Database* db,
                               const base::FilePath& db_name);

  // Helper function to implement internals of Init().  This allows
  // Init() to retry in case of failure, since some failures run
  // recovery code.
  sql::InitStatus InitImpl(const base::FilePath& db_name);

  // Helper function to handle cleanup on upgrade failures.
  sql::InitStatus CantUpgradeToVersion(int cur_version);

  // Adds support for size in favicons table.
  bool UpgradeToVersion6();

  // Removes sizes column.
  bool UpgradeToVersion7();

  // Adds support for bitmap usage tracking.
  bool UpgradeToVersion8();

  // Returns true if the `favicons` database is missing a column.
  bool IsFaviconDBStructureIncorrect();

  sql::Database db_;
  sql::MetaTable meta_table_;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DATABASE_H_
