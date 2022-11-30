// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CONTENT_FAVICON_URL_UTIL_H_
#define COMPONENTS_FAVICON_CONTENT_FAVICON_URL_UTIL_H_

#include <vector>
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

namespace favicon {

struct FaviconURL;

// Creates a favicon::FaviconURL from a blink::mojom::FaviconURL.
FaviconURL FaviconURLFromContentFaviconURL(
    const blink::mojom::FaviconURLPtr& favicon_url);

// Creates favicon::FaviconURLs from blink::mojom::FaviconURLPtrs.
std::vector<FaviconURL> FaviconURLsFromContentFaviconURLs(
    const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CONTENT_FAVICON_URL_UTIL_H_
