// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

class GURL;

namespace favicon {

// The large icon service provides methods to access large icons.
class LargeIconService : public KeyedService {
 public:
  // Requests the best large icon for the page at |page_url|.
  // Case 1. An icon exists whose size is >= MAX(|min_source_size_in_pixel|,
  // |desired_size_in_pixel|):
  // - If |desired_size_in_pixel| == 0: returns icon as is.
  // - Else: returns the icon resized to |desired_size_in_pixel|.
  // Case 2. An icon exists whose size is >= |min_source_size_in_pixel| and <
  // |desired_size_in_pixel|:
  // - Same as 1 with the biggest icon.
  // Case 4. An icon exists whose size is < |min_source_size_in_pixel|:
  // - Extracts dominant color of smaller image, returns a fallback icon style
  //   that has a matching background.
  // Case 5. No icon exists.
  // - Returns the default fallback icon style.
  // For cases 4 and 5, this function returns the style of the fallback icon
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

  // Behaves the same as GetLargeIconRawBitmapOrFallbackStyleForPageUrl, except
  // uses icon URL instead of page URL.
  virtual base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Fetches the best large icon for the page at |page_url| from a Google
  // favicon server and stores the result in the FaviconService database
  // (implemented in HistoryService). The write will be a no-op if the local
  // favicon database contains an icon for |page_url|, so clients are
  // encouraged to use GetLargeIconOrFallbackStyle() first.
  //
  // A parameter in the server request representing the desired favicon size is
  // set according solely to the device and scale factor. However, it serves
  // only as a hint to the service, no guarantees on the fetched size are
  // provided.
  //
  // Unless you are sure |page_url| is a public URL (known to Google Search),
  // set |may_page_url_be_private| to true. This slighty increases the chance of
  // a failure (e.g. if the URL _is_ private) but it makes sure Google servers
  // do not crawl a private URL as a result of this call.
  //
  // If |should_trim_page_url_path| is set to true, the path will be removed
  // from the URL used to query the server but the result will be stored under
  // the full URL provided to the API.
  //
  // The callback is triggered when the operation finishes, where |success|
  // tells whether the fetch actually managed to database a new icon in the
  // FaviconService.
  //
  // WARNING: This function will share the |page_url| with a Google server. This
  // Can be used only for urls that are not privacy sensitive or for users that
  // sync their history with Google servers.
  // TODO(crbug.com/903826): It is not clear from the name of this function,
  // that it actually adds the icon to the local cache. Maybe
  // "StoreLargeIcon..."?
  // TODO(victorvianna): Consider moving |may_page_url_be_private| and/or
  // |should_trim_page_url_path| inside the parameters struct.
  virtual void GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
      const GURL& page_url,
      bool may_page_url_be_private,
      bool should_trim_page_url_path,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::GoogleFaviconServerCallback callback) = 0;

  // Update the time that the icon at |icon_url| was requested. This should be
  // called after obtaining the icon by GetLargeIcon*OrFallbackStyle() for any
  // icon that _may_ originate from the Google favicon server (i.e. if the
  // caller uses
  // GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache()). This
  // postpones the automatic eviction of the favicon from the database.
  virtual void TouchIconFromGoogleServer(const GURL& icon_url) = 0;

 protected:
  LargeIconService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LargeIconService);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_
