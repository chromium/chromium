// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/fetch_options.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/enum_set.h"

namespace visited_url_ranking {

FetchOptions::FetchOptions(
    std::map<Fetcher, FetchSources> fetcher_sources_arg,
    base::Time begin_time_arg,
    std::vector<URLVisitAggregatesTransformType> transforms_arg)
    : fetcher_sources(std::move(fetcher_sources_arg)),
      begin_time(begin_time_arg),
      transforms(std::move(transforms_arg)) {
  DCHECK(!fetcher_sources.empty());
  DCHECK(!begin_time.is_null());
}

FetchOptions::~FetchOptions() = default;

FetchOptions::FetchOptions(FetchOptions&& other) = default;

FetchOptions& FetchOptions::operator=(FetchOptions&& other) = default;

// static
FetchOptions FetchOptions::CreateDefaultFetchOptionsForTabResumption() {
  return FetchOptions(
      {
          {Fetcher::kHistory, FetchOptions::kOriginSources},
          {Fetcher::kSession, FetchOptions::kOriginSources},
      },
      base::Time::Now() - base::Days(1),
      {
          URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter,
          URLVisitAggregatesTransformType::kBookmarkData,
          URLVisitAggregatesTransformType::kShoppingData,
      });
}

}  // namespace visited_url_ranking
