// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/ios/favicon_url_util.h"

#include <algorithm>
#include <iterator>

#include "components/favicon/core/favicon_url.h"
#include "components/favicon_base/favicon_types.h"
#include "ios/web/public/favicon/favicon_url.h"

namespace favicon {
namespace {

favicon_base::IconType IconTypeFromWebIconType(
    web::FaviconURL::IconType icon_type) {
  switch (icon_type) {
    case web::FaviconURL::IconType::kFavicon:
      return favicon_base::IconType::kFavicon;
    case web::FaviconURL::IconType::kTouchIcon:
      return favicon_base::IconType::kTouchIcon;
    case web::FaviconURL::IconType::kTouchPrecomposedIcon:
      return favicon_base::IconType::kTouchPrecomposedIcon;
    case web::FaviconURL::IconType::kWebManifestIcon:
      return favicon_base::IconType::kWebManifestIcon;
    case web::FaviconURL::IconType::kInvalid:
      return favicon_base::IconType::kInvalid;
  }
  NOTREACHED();
  return favicon_base::IconType::kInvalid;
}

}  // namespace

FaviconURL FaviconURLFromWebFaviconURL(
    const web::FaviconURL& favicon_url) {
  return FaviconURL(favicon_url.icon_url,
                    IconTypeFromWebIconType(favicon_url.icon_type),
                    favicon_url.icon_sizes);
}

std::vector<FaviconURL> FaviconURLsFromWebFaviconURLs(
    const std::vector<web::FaviconURL>& favicon_urls) {
  std::vector<FaviconURL> result;
  result.reserve(favicon_urls.size());
  std::transform(favicon_urls.begin(), favicon_urls.end(),
                 std::back_inserter(result), FaviconURLFromWebFaviconURL);
  return result;
}

}  // namespace favicon
