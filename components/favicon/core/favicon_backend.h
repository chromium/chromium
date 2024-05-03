// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_BACKEND_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_BACKEND_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/favicon/core/favicon_types.h"
#include "components/favicon_base/favicon_types.h"

class GURL;
class SkBitmap;

namespace base {
class FilePath;
}

namespace favicon {

// The maximum number of bitmaps for a single icon URL which can be stored in
// the favicon database.
static const size_t kMaxFaviconBitmapsPerIconURL = 8;

class FaviconBackendDelegate;
class FaviconDatabase;

// FaviconBackend owns and interacts with FaviconDatabase to maintain favicons.
// FaviconDatabase provides a thin veneer on top of sqlite for reading/writing
// favicons. FaviconBackend provides the logic for querying and updating the
// database.
class FaviconBackend {
 public:
  // Creates a new FaviconBackend. Returns the backend on success, null if
  // there is an error in creating it.
  static std::unique_ptr<FaviconBackend> Create(
      const base::FilePath& path,
      FaviconBackendDelegate* delegate);

  FaviconBackend(const FaviconBackend&) = delete;
  FaviconBackend& operator=(const FaviconBackend&) = delete;
  ~FaviconBackend();

  FaviconDatabase* db() { return db_.get(); }

  void Commit();
  void TrimMemory();

  // Removes all favicons, except those referenced by `kept_page_urls`.
  // Returns true on success.
  bool ClearAllExcept(const std::vector<GURL>& kept_page_urls);

  // See function of same name in HistoryService for details.
  favicon_base::FaviconRawBitmapResult GetLargestFaviconForUrl(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types_list,
      int minimum_size_in_pixels);

  // See function of same name in HistoryService for details.
  std::vector<favicon_base::FaviconRawBitmapResult> GetFaviconsForUrl(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      const std::vector<int>& desired_sizes,
      bool fallback_to_host);

  // See function of same name in HistoryService for details.
  std::vector<favicon_base::FaviconRawBitmapResult> GetFaviconForId(
      favicon_base::FaviconID favicon_id,
      int desired_size);

  // Used by both UpdateFaviconMappingsAndFetch() and GetFavicon().
  // If there is a favicon stored in the database for `icon_url`, a mapping is
  // added to the database from each element in `page_urls` (and all redirects)
  // to `icon_url`.
  UpdateFaviconMappingsResult UpdateFaviconMappingsAndFetch(
      const base::flat_set<GURL>& page_urls,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const std::vector<int>& desired_sizes);

  // Deletes the mappings for the specified page urls. Returns the set of
  // page urls that changed.
  base::flat_set<GURL> DeleteFaviconMappings(
      const base::flat_set<GURL>& page_urls,
      favicon_base::IconType icon_type);

  // See function of same name in HistoryService for details.
  MergeFaviconResult MergeFavicon(
      const GURL& page_url,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      scoped_refptr<base::RefCountedMemory> bitmap_data,
      const gfx::Size& pixel_size);

  // If `bitmap_type` is ON_DEMAND, the icon for `icon_url` will be modified
  // only if it's not present in the database. In that case, it will be
  // initially set as expired. `page_urls` must not be empty.
  SetFaviconsResult SetFavicons(const base::flat_set<GURL>& page_urls,
                                favicon_base::IconType icon_type,
                                const GURL& icon_url,
                                const std::vector<SkBitmap>& bitmaps,
                                FaviconBitmapType bitmap_type);

  // Causes each page in `page_urls_to_write` to be associated to the same
  // icon as the page `page_url_to_read` for icon types matching `icon_types`.
  // No-op if `page_url_to_read` has no mappings for `icon_types`. Returns the
  // set of page urls that were updated, which may be empty.
  std::set<GURL> CloneFaviconMappingsForPages(
      const GURL& page_url_to_read,
      const favicon_base::IconTypeSet& icon_types,
      const base::flat_set<GURL>& page_urls_to_write);

  // Returns all icon URLs associated with the given `page_url`. In case there
  // are multiple, they're ordered in descending order of IconType.
  std::vector<GURL> GetFaviconUrlsForUrl(const GURL& page_url);

  // See function of same name in HistoryService for details.
  SetFaviconsResult SetOnDemandFavicons(const GURL& page_url,
                                        favicon_base::IconType icon_type,
                                        const GURL& icon_url,
                                        const std::vector<SkBitmap>& bitmaps);

  // See function of same name in HistoryService for details.
  bool CanSetOnDemandFavicons(const GURL& page_url,
                              favicon_base::IconType icon_type);

  // See function of same name in HistoryService for details. Returns true
  // if the mapping was updated, false if `page_url` has no icons associated
  // with it.
  bool SetFaviconsOutOfDateForPage(const GURL& page_url);

  // Mark all favicons as out of date that have been modified at or after
  // `begin` and before `end`.
  bool SetFaviconsOutOfDateBetween(base::Time begin, base::Time end);

  // See function of same name in HistoryService for details.
  void TouchOnDemandFavicon(const GURL& icon_url);

 private:
  FaviconBackend(std::unique_ptr<FaviconDatabase> db,
                 FaviconBackendDelegate* delegate);

  // Set the favicon bitmaps of `type` for `icon_id`.
  // For each entry in `bitmaps`, if a favicon bitmap already exists at the
  // entry's pixel size, replace the favicon bitmap's data with the entry's
  // bitmap data. Otherwise add a new favicon bitmap.
  // Any favicon bitmaps already mapped to `icon_id` whose pixel size does not
  // match the pixel size of one of `bitmaps` is deleted.
  // For bitmap type FaviconBitmapType::ON_DEMAND, this is legal to call only
  // for a newly created `icon_id` (that has no bitmaps yet).
  // Returns true if any of the bitmap data at `icon_id` is changed as a result
  // of calling this method.
  bool SetFaviconBitmaps(favicon_base::FaviconID icon_id,
                         const std::vector<SkBitmap>& bitmaps,
                         FaviconBitmapType type);

  bool IsFaviconBitmapDataEqual(
      FaviconBitmapID bitmap_id,
      const scoped_refptr<base::RefCountedMemory>& new_bitmap_data);

  // Returns the favicon bitmaps whose edge sizes most closely match
  // `desired_sizes`. If `desired_sizes` has a '0' entry, the largest favicon
  // bitmap with one of the icon types in `icon_types` is returned. If
  // `icon_types` contains multiple icon types and there are several matched
  // icon types in the database, results will only be returned for a single
  // icon type in the priority of kTouchPrecomposedIcon, kTouchIcon, and
  // kFavicon. If `fallback_to_host` is true, the host of `page_url` will be
  // used to search the favicon database if an exact match cannot be found.
  // See the comment for GetFaviconResultsForBestMatch() for more details on
  // how `favicon_bitmap_results` is constructed.
  std::vector<favicon_base::FaviconRawBitmapResult> GetFaviconsFromDB(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      const std::vector<int>& desired_sizes,
      bool fallback_to_host);

  // Returns the favicon bitmaps whose edge sizes most closely match
  // `desired_sizes` in `favicon_bitmap_results`. If `desired_sizes` has a '0'
  // entry, only the largest favicon bitmap is returned. Goodness is computed
  // via SelectFaviconFrameIndices(). It is computed on a per FaviconID basis,
  // thus all `favicon_bitmap_results` are guaranteed to be for the same
  // FaviconID. `favicon_bitmap_results` will have at most one entry for each
  // desired edge size. There will be fewer entries if the same favicon bitmap
  // is the best result for multiple edge sizes.
  // Returns true if there were no errors.
  std::vector<favicon_base::FaviconRawBitmapResult>
  GetFaviconBitmapResultsForBestMatch(
      const std::vector<favicon_base::FaviconID>& candidate_favicon_ids,
      const std::vector<int>& desired_sizes);

  // Maps the favicon ID `icon_id` to `page_url` (and all redirects) for
  // `icon_type`. `icon_id` == 0 deletes previously existing mappings.
  // Returns true if the mappings for the page or any of its redirects were
  // changed.
  bool SetFaviconMappingsForPageAndRedirects(const GURL& page_url,
                                             favicon_base::IconType icon_type,
                                             favicon_base::FaviconID icon_id);

  // Maps the favicon ID `icon_id` to URLs in `page_urls` for `icon_type`.
  // `icon_id` == 0 deletes previously existing mappings.
  // Returns page URLs among `page_urls` whose mappings were changed (might be
  // empty).
  std::vector<GURL> SetFaviconMappingsForPages(
      const base::flat_set<GURL>& page_urls,
      favicon_base::IconType icon_type,
      favicon_base::FaviconID icon_id);

  // Maps the favicon ID `icon_ids` to `page_url` for `icon_type`.
  // `icon_id` == 0 deletes previously existing mappings.
  // Returns true if the function changed at least one of `page_url`'s mappings.
  bool SetFaviconMappingsForPage(const GURL& page_url,
                                 favicon_base::IconType icon_type,
                                 favicon_base::FaviconID icon_id);

  std::unique_ptr<FaviconDatabase> db_;
  raw_ptr<FaviconBackendDelegate> delegate_;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_BACKEND_H_
