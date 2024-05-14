// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/favicon_url_util.h"

#include <iterator>

#include "base/ranges/algorithm.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon_base/favicon_types.h"

namespace favicon {
namespace {

favicon_base::IconType IconTypeFromContentIconType(
    blink::mojom::FaviconIconType icon_type) {
  switch (icon_type) {
    case blink::mojom::FaviconIconType::kFavicon:
      return favicon_base::IconType::kFavicon;
    case blink::mojom::FaviconIconType::kTouchIcon:
      return favicon_base::IconType::kTouchIcon;
    case blink::mojom::FaviconIconType::kTouchPrecomposedIcon:
      return favicon_base::IconType::kTouchPrecomposedIcon;
    case blink::mojom::FaviconIconType::kInvalid:
      return favicon_base::IconType::kInvalid;
  }
  NOTREACHED_IN_MIGRATION();
  return favicon_base::IconType::kInvalid;
}

}  // namespace

FaviconURL FaviconURLFromContentFaviconURL(
    const blink::mojom::FaviconURLPtr& favicon_url) {
  return FaviconURL(favicon_url->icon_url,
                    IconTypeFromContentIconType(favicon_url->icon_type),
                    favicon_url->icon_sizes);
}

std::vector<FaviconURL> FaviconURLsFromContentFaviconURLs(
    const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) {
  std::vector<FaviconURL> result;
  result.reserve(favicon_urls.size());
  base::ranges::transform(favicon_urls, std::back_inserter(result),
                          FaviconURLFromContentFaviconURL);
  return result;
}

}  // namespace favicon
