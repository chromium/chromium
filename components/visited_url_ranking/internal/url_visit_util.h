// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_VISIT_UTIL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_VISIT_UTIL_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "url/gurl.h"

namespace visited_url_ranking {

// TODO(crbug.com/330580421): Remove/replace the category blocklist array
// specified in `modules_util.h` with the one below.
inline constexpr auto kBlocklistedCategories =
    base::MakeFixedFlatSet<std::string_view>(
        {"/g/11b76fyj2r", "/m/09lkz", "/m/012mj", "/m/01rbb", "/m/02px0wr",
         "/m/028hh", "/m/034qg", "/m/034dj", "/m/0jxxt", "/m/015fwp",
         "/m/04shl0", "/m/01h6rj", "/m/05qt0", "/m/06gqm", "/m/09l0j_",
         "/m/01pxgq", "/m/0chbx", "/m/02c66t"});

inline constexpr auto kDefaultAppBlocklist =
    base::MakeFixedFlatSet<std::string_view>(
        {"mail.google.com", "m.youtube.com", "www.youtube.com",
         "drive.google.com", "photos.google.com", "calendar.google.com",
         "docs.google.com", "assistant.google.com", "music.youtube.com"});

// Generates an identifier for the given URL leveraged for merging and
// deduplication of similar URLs.
URLMergeKey ComputeURLMergeKey(const GURL& url);

}  // namespace visited_url_ranking

#endif  // COMPONENTS_URL_VISIT_URL_VISIT_UTIL_H_
