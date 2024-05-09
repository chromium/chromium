// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_VISIT_UTIL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_VISIT_UTIL_H_

#include "components/visited_url_ranking/public/fetch_result.h"
#include "url/gurl.h"

namespace visited_url_ranking {

// Generates an identifier for the given URL leveraged for merging and
// deduplication of similar URLs.
URLMergeKey ComputeURLMergeKey(const GURL& url);

}  // namespace visited_url_ranking

#endif  // COMPONENTS_URL_VISIT_URL_VISIT_UTIL_H_
