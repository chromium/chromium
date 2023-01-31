// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/feature_list.h"

namespace bookmarks {

// If enabled, uses an approximate pre-check to determine if an input matches a
// particular bookmark index node. This pre-check is faster than the more
// accurate check, but it returns false positives; therefore, it's only a
// precursor to and not a replacement for the real check. Does nothing if
// `omnibox::kBookmarkPaths` is disabled.
BASE_FEATURE(kApproximateNodeMatch,
             "BookmarkApproximateNodeMatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, uses an alternative approach to loading typed counts for URLs
// when fetching bookmark matches for the bookmark provider.
// - When disabled, for each matching bookmark, it runs 1 SQL query to look up
//   its typed count by URL, which is indexed and therefore runs O(n * log(m)),
//   where n is the # of bookmark matches, and m is the # of URLs.
// - When enabled, reads all URLs from the DB in 1 scan and stores them to a
//   `std::map`. Then for each matching bookmark, it looks up the URL in the
//   map. This is O(n*log(m) + m) runtime and requires O(m) additional space.
//   This map isn't cached since the DB changes as the user visits and deletes
//   visits; and propagating those changes to the cached map would add
//   complexity.
BASE_FEATURE(kTypedUrlsMap,
             "BookmarkTypedUrlsMap",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, further limits the maximum number of nodes to fetch when looking
// for bookmark nodes that match any input term. When disabled, the limit is
// 3000, which was picked to be very lax; it should rarely be reached and avoids
// only extreme latency but still allows noticeable latency. Does nothing when
// `omnibox::kBookmarkPaths` is disabled.
BASE_FEATURE(kLimitNumNodesForBookmarkSearch,
             "BookmarkLimitNumNodesForBookmarkSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// See `kLimitNumNodesForBookmarkSearch`.
const base::FeatureParam<int> kLimitNumNodesForBookmarkSearchCount(
    &kLimitNumNodesForBookmarkSearch,
    "BookmarkLimitNumNodesForBookmarkSearchCount",
    3000);

// If enabled, creates and uses a lightweight (compared to the existing
// `TitledUrlIndex`). The index maps the terms in paths and the number of paths
// containing those terms. It's updated on folder rename, creation, and
// deletion. It's not updated when bookmarks or folders are moved. It's used to
// short circuit unioning per-term matches when matching paths, as intersecting
// results in much fewer nodes and processing. Should be disabled if
// `omnibox::kBookmarkPaths` is disabled; otherwise, it'll create the index
// unnecessarily.
BASE_FEATURE(kIndexPaths,
             "BookmarkIndexPaths",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace bookmarks
