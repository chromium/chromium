// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/fetcher_config.h"

#include "components/url_deduplication/url_deduplication_helper.h"

namespace visited_url_ranking {

FetcherConfig::FetcherConfig(
    url_deduplication::URLDeduplicationHelper* deduplication_helper)
    : deduplication_helper(deduplication_helper) {}

}  // namespace visited_url_ranking
