// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_UTILS_H_

class GURL;

namespace offline_pages {

struct OfflinePageItem;

// Describes how a URL matches an OfflinePageItem's final and original URLs.
enum class OfflinePageUrlMatch {
  // The URL does not match either the item's final URL or its original URL.
  kNone,
  // The URL matches `item.url` (ignoring fragment), so the offline archive can
  // be served directly without redirecting.
  kMatchesFinalUrl,
  // The URL does not match `item.url`, but matches
  // `item.original_url_if_different` (ignoring fragment), so serving the
  // offline archive requires redirecting to `item.url` first.
  kMatchesOriginalUrl,
};

// Returns true if two URLs are equal, ignoring the fragment.
// Because offline page items are stored without fragment, this is appropriate
// for checking if an offline item's URL matches another URL.
bool EqualsIgnoringFragment(const GURL& lhs, const GURL& rhs);

// Strips any fragment from |url| and returns the result.
GURL UrlWithoutFragment(const GURL& url);

// Classifies how `url` matches `item`'s final and original URLs, ignoring
// fragments. Returns `kNone` unless both `url` and `item.url` satisfy
// `OfflinePageModel::CanSaveURL()` (and, for `kMatchesOriginalUrl`,
// `item.original_url_if_different` does as well). Matches against `item.url`
// take precedence over `item.original_url_if_different` so items whose final
// and original URLs are equal (ignoring fragment) do not trigger redirects.
OfflinePageUrlMatch ClassifyOfflinePageUrlMatch(const GURL& url,
                                                const OfflinePageItem& item);

}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_UTILS_H_
