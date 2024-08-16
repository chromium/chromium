// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/fetcher_config.h"

#include "base/logging.h"
#include "base/time/clock.h"
#include "components/url_deduplication/url_deduplication_helper.h"

namespace visited_url_ranking {

FetcherConfig::FetcherConfig(base::Clock* clock_arg) : clock(clock_arg) {
  DCHECK(clock);
}

FetcherConfig::FetcherConfig(
    url_deduplication::URLDeduplicationHelper* deduplication_helper,
    base::Clock* clock_arg)
    : deduplication_helper(deduplication_helper), clock(clock_arg) {
  DCHECK(clock);
}

}  // namespace visited_url_ranking
