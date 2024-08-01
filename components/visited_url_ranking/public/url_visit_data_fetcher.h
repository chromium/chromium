// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_DATA_FETCHER_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_DATA_FETCHER_H_

#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/fetcher_config.h"

namespace visited_url_ranking {

// Derived classes implement logic responsible for fetching and computing
// URL visit related data relevant to the creation of `URLVisitAggregate`
// objects.
class URLVisitDataFetcher {
 public:
  virtual ~URLVisitDataFetcher() = default;

  using FetchResultCallback = base::OnceCallback<void(FetchResult)>;
  virtual void FetchURLVisitData(const FetchOptions& options,
                                 const FetcherConfig& config,
                                 FetchResultCallback callback) = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_DATA_FETCHER_H_
