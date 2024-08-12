// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCHER_CONFIG_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCHER_CONFIG_H_

#include "base/memory/raw_ptr.h"
#include "components/url_deduplication/deduplication_strategy.h"

namespace url_deduplication {
class URLDeduplicationHelper;
}  // namespace url_deduplication

namespace visited_url_ranking {

struct FetcherConfig {
  FetcherConfig() = default;
  explicit FetcherConfig(
      url_deduplication::URLDeduplicationHelper* deduplication_helper);
  ~FetcherConfig() = default;

  raw_ptr<url_deduplication::URLDeduplicationHelper> deduplication_helper;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCHER_CONFIG_H_
