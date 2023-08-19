// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_FAVICON_PROVIDER_H_
#define COMPONENTS_FAVICON_CORE_LARGE_FAVICON_PROVIDER_H_

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"

class GURL;

namespace favicon {

// An interface used to look up large favicons. It's used by LargeIconBridge and
// LargeIconService, and embedders provide an implementation via
// SetLargeFaviconProviderGetter(), allowing code in //components to access the
// provider for a given BrowserContext.
class LargeFaviconProvider {
 public:
  // Requests the best large icon for the page at |page_url|.
  // Case 1. An icon exists whose size is >= MAX(|min_source_size_in_pixel|,
  // |desired_size_in_pixel|):
  // - If |desired_size_in_pixel| == 0: returns icon as is.
  // - Else: returns the icon resized to |desired_size_in_pixel|.
  // Case 2. An icon exists whose size is >= |min_source_size_in_pixel| and <
  // |desired_size_in_pixel|:
  // - Same as 1 with the biggest icon.
  // Case 3. An icon exists whose size is < |min_source_size_in_pixel|:
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

  // Requests the best large icon raw bitmap for the page at |page_url|.
  // If there are several icons cached in the favicon database for |page_url|
  // which are > |minimum_size_in_pixels|, selects an icon to return based on an
  // LargeFaviconProvider-hard-coded ordering of preference for certain
  // IconTypes. If no icon is larger than |minimum_size_in_pixels|, the largest
  // one will be returned.
  virtual base::CancelableTaskTracker::TaskId GetLargeIconRawBitmapForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_FAVICON_PROVIDER_H_
