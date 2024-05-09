// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_visit_util.h"

#include "components/visited_url_ranking/public/fetch_result.h"
#include "url/gurl.h"

namespace visited_url_ranking {

// TODO(crbug.com/335200723): Integrate client configurable merging and
// deduplication logic to produce "merge" keys for provided URLs.
URLMergeKey ComputeURLMergeKey(const GURL& url) {
  return url.spec();
}

}  // namespace visited_url_ranking
