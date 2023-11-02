// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_URL_LOAD_STATS_H_
#define COMPONENTS_BOOKMARKS_COMMON_URL_LOAD_STATS_H_

namespace bookmarks {

// Returns some stats about number of URL bookmarks stored, for UMA purposes.
struct UrlLoadStats {
  // Number of bookmark in the index excluding folders.
  size_t total_url_bookmark_count = 0;
  // Number of bookmarks (excluding folders) with a URL that is used by at
  // least one other bookmark, excluding one bookmark per unique URL (i.e. all
  // except one are considered duplicates).
  size_t duplicate_url_bookmark_count = 0;
  // Number of bookmarks (excluding folders) with the pair <URL, title> that
  // is used by at least one other bookmark, excluding one bookmark per unique
  // URL (i.e. all except one are considered duplicates).
  size_t duplicate_url_and_title_bookmark_count = 0;
  // Number of bookmarks (excluding folders) with the triple <URL, title,
  // parent> that is used by at least one other bookmark, excluding one
  // bookmark per unique URL (i.e. all except one are considered duplicates).
  size_t duplicate_url_and_title_and_parent_bookmark_count = 0;
  // Average number of days since each bookmark was added.
  size_t avg_num_days_since_added = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_URL_LOAD_STATS_H_