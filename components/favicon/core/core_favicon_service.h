// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_CORE_FAVICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_CORE_FAVICON_SERVICE_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class SkBitmap;

namespace gfx {
class Image;
}

namespace favicon {

// CoreFaviconService defines the API needed to persist and restore favicons.
class CoreFaviconService : public KeyedService {
 public:
  CoreFaviconService() = default;

  // Gets the favicons mapped to `page_url` for `icon_types` whose edge sizes
  // most closely match `desired_size_in_dip`. A value of 0 gets the
  // largest bitmap for `icon_types`. The returned FaviconBitmapResults will
  // have at most one result for each entry in `desired_sizes`. If a bitmap is
  // determined to be the best candidate for multiple `desired_sizes` there
  // will be fewer results.
  virtual base::CancelableTaskTracker::TaskId GetFaviconForPageURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Marks all types of favicon for the page as being out of date.
  virtual void SetFaviconOutOfDateForPage(const GURL& page_url) = 0;

  // Set the favicon for all URLs in `page_urls` for `icon_type` in the
  // thumbnail database. `icon_url` is the single favicon to map to. Mappings
  // from page URLs to favicons at different icon URLs will be deleted.
  // A favicon bitmap is added for each image rep in `image`. Any preexisting
  // bitmap data for `icon_url` is deleted. It is important that `image`
  // contains image reps for all of ui::GetSupportedResourceScaleFactors(). Use
  // MergeFavicon() if it does not.
  // TODO(pkotwicz): Save unresized favicon bitmaps to the database.
  // TODO(pkotwicz): Support adding favicons for multiple icon URLs to the
  // thumbnail database.
  virtual void SetFavicons(const base::flat_set<GURL>& page_urls,
                           const GURL& icon_url,
                           favicon_base::IconType icon_type,
                           const gfx::Image& image) = 0;

  // Causes each page in `page_urls_to_write` to be associated to the same
  // icon as the page `page_url_to_read` for icon types matching `icon_types`.
  // No-op if `page_url_to_read` has no mappings for `icon_types`.
  virtual void CloneFaviconMappingsForPages(
      const GURL& page_url_to_read,
      const favicon_base::IconTypeSet& icon_types,
      const base::flat_set<GURL>& page_urls_to_write) = 0;

  // The first argument for `callback` is the set of bitmaps for the passed in
  // URL and icon types whose pixel sizes best match the passed in
  // `desired_size_in_dip` at the resource scale factors supported by the
  // current platform (eg MacOS) in addition to 1x. The vector has at most one
  // result for each of the resource scale factors. There are less entries if a
  // single/ result is the best bitmap to use for several resource scale
  // factors.
  virtual base::CancelableTaskTracker::TaskId GetFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Maps `page_urls` to the favicon at `icon_url` if there is an entry in the
  // database for `icon_url` and `icon_type`. This occurs when there is a
  // mapping from a different page URL to `icon_url`. The favicon bitmaps whose
  // edge sizes most closely match `desired_size_in_dip` from the favicons which
  // were just mapped to `page_urls` are returned. If `desired_size_in_dip` has
  // a '0' entry, the largest favicon bitmap is returned.
  virtual base::CancelableTaskTracker::TaskId UpdateFaviconMappingsAndFetch(
      const base::flat_set<GURL>& page_urls,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Deletes favicon mappings for each URL in `page_urls` and their redirects.
  virtual void DeleteFaviconMappings(const base::flat_set<GURL>& page_urls,
                                     favicon_base::IconType icon_type) = 0;

  // Avoid repeated requests to download missing favicon.
  virtual void UnableToDownloadFavicon(const GURL& icon_url) = 0;
  virtual void ClearUnableToDownloadFavicons() = 0;
  virtual bool WasUnableToDownloadFavicon(const GURL& icon_url) const = 0;

 protected:
  // Returns a vector of pixel edge sizes from `size_in_dip` and
  // GetFaviconScales().
  static std::vector<int> GetPixelSizesForFaviconScales(int size_in_dip);

  // Returns a vector of the bitmaps to store for the specified image.
  static std::vector<SkBitmap> ExtractSkBitmapsToStore(const gfx::Image& image);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_CORE_FAVICON_SERVICE_H_
