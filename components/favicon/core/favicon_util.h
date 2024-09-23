// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_UTIL_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_UTIL_H_

#include "base/functional/callback_forward.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"

class GURL;

namespace favicon {

class FaviconService;

// Request a favicon from `favicon_service` for the page at `page_url`.
// `callback` is run when the the favicon has been fetched. If `type` is:
// - favicon_base::IconType::kFavicon, the returned gfx::Image is a
//   multi-resolution image of gfx::kFaviconSize DIP width and height (the data
//   from the cache is resized if need be),
// - otherwise, the returned gfx::Image is a single-resolution image with the
//   largest bitmap in the cache for `page_url` and `type`.
base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
    FaviconService* favicon_service,
    const GURL& page_url,
    favicon_base::IconType type,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_UTIL_H_
