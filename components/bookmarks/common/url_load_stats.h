// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_URL_LOAD_STATS_H_
#define COMPONENTS_BOOKMARKS_COMMON_URL_LOAD_STATS_H_

#include <cstdint>

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
  // Number of bookmarks which have a non-default value for time_since_opened.
  // This hints that this bookmark has been used before, but isn't conclusive
  // as this number is reset with history clear events.
  size_t used_url_bookmark_count = 0;
  // The most recent time any bookmark was opened, in days.
  size_t most_recently_used_bookmark_days = SIZE_MAX;
  // The most recent time any bookmark was created, in days.
  size_t most_recently_saved_bookmark_days = SIZE_MAX;
  // The most recent time any folder was created, in days.
  size_t most_recently_saved_folder_days = SIZE_MAX;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_URL_LOAD_STATS_H_