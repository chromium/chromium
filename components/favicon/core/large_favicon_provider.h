// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_FAVICON_PROVIDER_H_
#define COMPONENTS_FAVICON_CORE_LARGE_FAVICON_PROVIDER_H_

#include <vector>

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
  // This searches for icons by IconType. Each element of |icon_types| is a
  // bitmask of IconTypes indicating the types to search for. If the largest
  // icon of |icon_types[0]| is not larger than |minimum_size_in_pixel|, the
  // next icon types of |icon_types| will be searched and so on. If no icon is
  // larger than |minimum_size_in_pixel|, the largest one of all icon types in
  // |icon_types| is returned. This feature is especially useful when some types
  // of icon is preferred as long as its size is larger than a specific value.
  virtual base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types,
      int minimum_size_in_pixels,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_FAVICON_PROVIDER_H_
