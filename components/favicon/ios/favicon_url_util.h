// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_IOS_FAVICON_URL_UTIL_H_
#define COMPONENTS_FAVICON_IOS_FAVICON_URL_UTIL_H_

#include <vector>

namespace web {
struct FaviconURL;
}

namespace favicon {

struct FaviconURL;

// Creates a favicon::FaviconURL from a web::FaviconURL.
FaviconURL FaviconURLFromWebFaviconURL(
    const web::FaviconURL& favicon_url);

// Creates favicon::FaviconURLs from web::FaviconURLs.
std::vector<FaviconURL> FaviconURLsFromWebFaviconURLs(
    const std::vector<web::FaviconURL>& favicon_urls);

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_IOS_FAVICON_URL_UTIL_H_
