// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_

#include <optional>

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

class GURL;

namespace favicon {

// The large icon service provides methods to access large icons. The actual
// implementation of this uses Google's favicon service.
class LargeIconService : public KeyedService {
 public:
  // Controls the icon size in pixels returned by the
  // `GetLargeIconFromCacheFallbackToGoogleServer()`. This enum is used to
  // ensure icons are requested at standard sizes, because the underlying
  // favicon database stores at most 1 icon per domain and favicons are
  // requested from the Google server at a single hard-coded size.
  enum class StandardIconSize {
    k16x16 = 0,
    k32x32 = 1,
  };
  // Controls the behavior when there is no icon bigger than the minimum size to
  // return.
  enum class NoBigEnoughIconBehavior {
    // Return the biggest favicon bitmap available.
    kReturnBitmap = 0,
    // Extract the dominant color of the smaller image.
    kReturnFallbackColor = 1,
    // Empty return.
    kReturnEmpty = 2,
  };

  LargeIconService(const LargeIconService&) = delete;
  LargeIconService& operator=(const LargeIconService&) = delete;

  // Requests the best large icon for the page at `page_url`.
  // Case 1. An icon exists whose size is >= MAX(`min_source_size_in_pixel`,
  // `desired_size_in_pixel`):
  // - If `desired_size_in_pixel` == 0: returns icon as is.
  // - Else: returns the icon resized to `desired_size_in_pixel`.
  // Case 2. An icon exists whose size is >= `min_source_size_in_pixel` and <
  // `desired_size_in_pixel`:
  // - Same as 1 with the biggest icon.
  // Case 3. An icon exists whose size is < `min_source_size_in_pixel`:
  // - Extracts dominant color of smaller image, returns a fallback icon style
  //   that has a matching background.
  // Case 4. No icon exists.
  // - Returns the default fallback icon style.
  // For cases 3 and 4, this function returns the style of the fallback icon
  // instead of rendering an icon so clients can render the icon themselves.
  virtual base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Behaves the same as GetLargeIconRawBitmapOrFallbackStyleForPageUrl(), only
  // returns the large icon (if available) decoded.
  virtual base::CancelableTaskTracker::TaskId
  GetLargeIconImageOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconImageCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Queries the favicon database for an icon larger than
  // `min_source_size_in_pixel`. If `size_in_pixel_to_resize_to` is specified,
  // the returned icon will be resized to the passed-in size. The resizing also
  // occurs if there is no big enough icon and `no_big_enough_icon_behavior` ==
  // NoBigEnoughIconBehavior::kReturnBitmap. `no_big_enough_icon_behavior`
  // controls the returned value if there is no icon larger than
  // `min_source_size_in_pixel` in the database.
  virtual base::CancelableTaskTracker::TaskId GetLargeIconRawBitmapForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      std::optional<int> size_in_pixel_to_resize_to,
      NoBigEnoughIconBehavior no_big_enough_icon_behavior,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Behaves the same as GetLargeIconRawBitmapOrFallbackStyleForPageUrl, except
  // uses icon URL instead of page URL.
  virtual base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Requests the best icon for the page at `page_url`. Fallbacks to the host's
  // favicon, and resizes the most similar bitmat to `desired_size_in_pizel` if
  // no exact match is found.
  virtual base::CancelableTaskTracker::TaskId
  GetIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Fetches the best large icon for the page at `page_url` from a Google
  // favicon server and stores the result in the FaviconService database
  // (implemented in HistoryService). The write will be a no-op if the local
  // favicon database contains an icon for `page_url`, so clients are
  // encouraged to use GetLargeIconOrFallbackStyle() first.
  //
  // A parameter in the server request representing the desired favicon size is
  // set according solely to the device and scale factor. However, it serves
  // only as a hint to the service, no guarantees on the fetched size are
  // provided.
  //
  // If `should_trim_page_url_path` is set to true, the path will be removed
  // from the URL used to query the server but the result will be stored under
  // the full URL provided to the API.
  //
  // The callback is triggered when the operation finishes, where `success`
  // tells whether the fetch actually managed to database a new icon in the
  // FaviconService.
  //
  // WARNING: This function will share the `page_url` with a Google server. This
  // can be used only for urls that are not privacy sensitive or for users that
  // sync their history with Google servers.
  // TODO(crbug.com/41425581): It is not clear from the name of this function,
  // that it actually adds the icon to the local cache. Maybe
  // "StoreLargeIcon..."?
  virtual void GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
      const GURL& page_url,
      bool should_trim_page_url_path,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::GoogleFaviconServerCallback callback) = 0;

  // Requests the best large icon for the `page_url` via
  // `GetLargeIconRawBitmapForPageUrl()`. `min_source_size` and
  // `size_to_resize_to` are converted to integer values before being processed
  // further. They and `no_big_enough_icon_behavior` control the behavior of the
  // `GetLargeIconRawBitmapForPageUrl()`. If no icon (of any size) is found in
  // the local cache for `page_url`, the icon is queried from the Google server
  // via `GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache()`.
  //
  // Note: it's possible to obtain an image bigger than the largest standard
  // size (32x32) if the user has visited the `page_url` previously a mobile
  // device and the passed-in `size_to_resize_to` is `std::nullopt`.
  //
  // WARNING: This function may share the `page_url` with a Google server if the
  // icon is not found locally. This can be used only for urls that are not
  // privacy sensitive or for users that sync their history with Google servers.
  virtual void GetLargeIconFromCacheFallbackToGoogleServer(
      const GURL& page_url,
      StandardIconSize min_source_size,
      std::optional<StandardIconSize> size_to_resize_to,
      NoBigEnoughIconBehavior no_big_enough_icon_behavior,
      bool should_trim_page_url_path,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Update the time that the icon at `icon_url` was requested. This should be
  // called after obtaining the icon by GetLargeIcon*OrFallbackStyle() for any
  // icon that _may_ originate from the Google favicon server (i.e. if the
  // caller uses
  // GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache()). This
  // postpones the automatic eviction of the favicon from the database.
  virtual void TouchIconFromGoogleServer(const GURL& icon_url) = 0;
 protected:
  LargeIconService() = default;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_
