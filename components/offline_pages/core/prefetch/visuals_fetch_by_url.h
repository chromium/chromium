// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_VISUALS_FETCH_BY_URL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_VISUALS_FETCH_BY_URL_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace offline_pages {

// Attempts to fetch a thumbnail, and returns the result to callback.
// |image_data| will be empty if the thumbnail fetch fails. Otherwise,
// |image_data| contains the raw image data (typically JPEG).
void FetchThumbnailByURL(
    base::OnceCallback<void(const std::string& image_data)> callback,
    image_fetcher::ImageFetcher* fetcher,
    const GURL& thumbnail_url);

// Attempts to fetch a favicon. (This works like |FetchThumbnailByURL| but uses
// different ImageFetcherParams.)
void FetchFaviconByURL(
    base::OnceCallback<void(const std::string& image_data)> callback,
    image_fetcher::ImageFetcher* fetcher,
    const GURL& favicon_url);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_VISUALS_FETCH_BY_URL_H_
