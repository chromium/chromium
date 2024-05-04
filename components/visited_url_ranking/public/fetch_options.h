// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_

#include <map>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

// The options that may be specified when fetching URL visit data.
struct FetchOptions {
  using Source = URLVisit::Source;
  using FetchSources =
      base::EnumSet<Source, Source::kNotApplicable, Source::kForeign>;
  FetchOptions(
      std::map<Fetcher, FetchSources> fetcher_sources_arg,
      base::Time begin_time_arg,
      std::vector<URLVisitAggregatesTransformType> transforms_arg = {});
  FetchOptions(const FetchOptions&) = delete;
  FetchOptions(FetchOptions&& other);
  FetchOptions& operator=(FetchOptions&& other);
  ~FetchOptions();

  // The set of sources that correspond to an origin.
  static constexpr FetchSources kOriginSources = {Source::kLocal,
                                                  Source::kForeign};

  // Returns the default fetch options for tab resumption use cases.
  static FetchOptions CreateDefaultFetchOptionsForTabResumption();

  // The set of data fetchers that should participate in the data fetching and
  // computation of URLVisit data, including their data source characteristics.
  std::map<Fetcher, FetchSources> fetcher_sources;

  // The earliest visit associated time to consider when fetching data. Each
  // fetcher may leverage this time differently depending on the timestamps that
  // are supported by their associated sources.
  base::Time begin_time;

  // A series of transformations to apply on the `URVisitAggregate` object
  // collection. These may include operations that mutate the collection or
  // specific field of the collection objects.
  std::vector<URLVisitAggregatesTransformType> transforms;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_
