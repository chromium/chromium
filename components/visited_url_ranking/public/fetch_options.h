// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_

#include <map>

#include "base/containers/enum_set.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

// The currently supported URL visit data fetchers that may participate in a
// fetch request.
enum class Fetcher {
  kTabModel = 0,
  kSession = 1,
  kHistory = 2,
};

// The options that may be specified when fetching URL visit data.
struct FetchOptions {
  using Source = URLVisit::Source;
  using FetchSources =
      base::EnumSet<Source, Source::kNotApplicable, Source::kForeign>;
  FetchOptions(std::map<Fetcher, FetchSources> fetcher_sources,
               base::Time begin_time);
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
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_
