// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_H_

#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/core_favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_usage_data.h"

class GURL;

namespace favicon {

class FaviconService : public CoreFaviconService {
 public:
  //////////////////////////////////////////////////////////////////////////////
  // Methods to request favicon bitmaps from the history backend for `icon_url`.
  // `icon_url` is the URL of the icon itself.
  // (e.g. <http://www.google.com/favicon.ico>)

  // Requests the favicon at `icon_url` of type favicon_base::IconType::kFavicon
  // and of size gfx::kFaviconSize. The returned gfx::Image is populated with
  // representations for all of the scale factors supported by the platform
  // (e.g. MacOS). If data is unavailable for some or all of the scale factors,
  // the bitmaps with the best matching sizes are resized.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImage(
      const GURL& icon_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Requests the favicon at `icon_url` of `icon_type` of size
  // `desired_size_in_pixel`. If there is no favicon of size
  // `desired_size_in_pixel`, the favicon bitmap which best matches
  // `desired_size_in_pixel` is resized. If `desired_size_in_pixel` is 0,
  // the largest favicon bitmap is returned.
  virtual base::CancelableTaskTracker::TaskId GetRawFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Methods to request favicon bitmaps from the history backend for `page_url`.
  // `page_url` is the web page the favicon is associated with.
  // (e.g. <http://www.google.com>)

  // Requests the favicon for the page at `page_url` of type
  // favicon_base::IconType::kFavicon and of size gfx::kFaviconSize. The
  // returned gfx::Image is populated with representations for all of the scale
  // factors supported by the platform (e.g. MacOS). If data is unavailable for
  // some or all of the scale factors, the bitmaps with the best matching sizes
  // are resized.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Requests the favicon for the page at `page_url` with one of `icon_types`
  // and with `desired_size_in_pixel`. `icon_types` can be any combination of
  // IconTypes. If there is no favicon bitmap of size `desired_size_in_pixel`,
  // the favicon bitmap which best matches `desired_size_in_pixel` is resized.
  // If `desired_size_in_pixel` is 0, the largest favicon bitmap is returned.
  // If `fallback_to_host` is true, the host of `page_url` will be used to
  // search the favicon database if an exact match cannot be found. Generally
  // code showing an icon for a full/previously visited URL should set
  // `fallback_to_host`=false. Otherwise, if only a host is available, and any
  // icon matching the host is permissible, use `fallback_to_host`=true.
  virtual base::CancelableTaskTracker::TaskId GetRawFaviconForPageURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      int desired_size_in_pixel,
      bool fallback_to_host,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // This searches for icons by IconType. Each element of `icon_types` is a
  // bitmask of IconTypes indicating the types to search for. If the largest
  // icon of `icon_types[0]` is not larger than `minimum_size_in_pixel`, the
  // next icon types of `icon_types` will be searched and so on. If no icon is
  // larger than `minimum_size_in_pixel`, the largest one of all icon types in
  // `icon_types` is returned. This feature is especially useful when some types
  // of icon is preferred as long as its size is larger than a specific value.
  virtual base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types,
      int minimum_size_in_pixels,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Used to request a bitmap for the favicon with `favicon_id` which is not
  // resized from the size it is stored at in the database. If there are
  // multiple favicon bitmaps for `favicon_id`, the largest favicon bitmap is
  // returned.
  virtual base::CancelableTaskTracker::TaskId GetLargestRawFaviconForID(
      favicon_base::FaviconID favicon_id,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Mark that the on-demand favicon at `icon_url` was requested now. This
  // postpones the automatic eviction of the favicon from the database. Not all
  // calls end up in a write into the DB:
  // - It is no-op if the bitmaps are not stored using SetOnDemandFavicons().
  // - The updates of the "last requested time" have limited frequency for each
  //   particular favicon (e.g. once per week). This limits the overhead of
  //   cache management for on-demand favicons.
  virtual void TouchOnDemandFavicon(const GURL& icon_url) = 0;

  // Allows the importer to set many favicons for many pages at once. The pages
  // must exist, any favicon sets for unknown pages will be discarded. Existing
  // favicons will not be overwritten.
  virtual void SetImportedFavicons(
      const favicon_base::FaviconUsageDataList& favicon_usage) = 0;

  // See HistoryService::AddPageNoVisitForBookmark(). Adds an entry for the
  // specified url in the history service without creating a visit.
  virtual void AddPageNoVisitForBookmark(const GURL& url,
                                         const std::u16string& title) = 0;

  // Set the favicon for `page_url` for `icon_type` in the thumbnail database.
  // Unlike SetFavicons(), this method will not delete preexisting bitmap data
  // which is associated to `page_url` if at all possible. Use this method if
  // the favicon bitmaps for any of ui::GetSupportedResourceScaleFactors() are
  // not known.
  virtual void MergeFavicon(const GURL& page_url,
                            const GURL& icon_url,
                            favicon_base::IconType icon_type,
                            scoped_refptr<base::RefCountedMemory> bitmap_data,
                            const gfx::Size& pixel_size) = 0;

  // Figures out whether an on-demand favicon can be written for provided
  // `page_url` and returns the result via `callback`. The result is false if
  // there is an existing cached favicon for `icon_type` or if there is a
  // non-expired icon of *any* type for `page_url`.
  virtual void CanSetOnDemandFavicons(
      const GURL& page_url,
      favicon_base::IconType icon_type,
      base::OnceCallback<void(bool)> callback) const = 0;

  // Same as SetFavicons with three differences:
  // 1) It will be a no-op if CanSetOnDemandFavicons() returns false.
  // 2) If `icon_url` is known to the database, `bitmaps` will be ignored (i.e.
  //    the icon won't be overwritten) but the mappings from `page_url` to
  //    `icon_url` will be stored (conditioned to point 1 above).
  // 3) If `icon_url` is stored, it will be marked as "on-demand".
  //
  // On-demand favicons are those that are fetched without visiting their page.
  // For this reason, their life-time cannot be bound to the life-time of the
  // corresponding visit in history.
  // - These bitmaps are evicted from the database based on the last time they
  //   get requested. The last requested time is initially set to Now() and is
  //   further updated by calling TouchOnDemandFavicon().
  // - Furthermore, on-demand bitmaps are immediately marked as expired. Hence,
  //   they are always replaced by standard favicons whenever their page gets
  //   visited.
  // The callback will receive whether the write actually happened.
  virtual void SetOnDemandFavicons(const GURL& page_url,
                                   const GURL& icon_url,
                                   favicon_base::IconType icon_type,
                                   const gfx::Image& image,
                                   base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_H_
