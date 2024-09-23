// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FETCH_OPTIONS_H_

#include <map>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

// A series of supported data transforms that modify a collection of
// `URLVisitAggregate` objects.
enum class URLVisitAggregatesTransformType {
  // Do not use! Internal purposes only.
  kUnspecified = 0,
  // Set bookmark related fields.
  kBookmarkData = 1,
  // Set shopping related fields.
  kShoppingData = 2,
  // Filter based on visibility score field.
  kHistoryVisibilityScoreFilter = 3,
  // Filter based on categories field.
  kHistoryCategoriesFilter = 4,
  // Filter based on whether the URL can be opened by default apps.
  kDefaultAppUrlFilter = 5,
  // Filter based on last active timestamp.
  kRecencyFilter = 6,
  // Set segmenation metrics related fields.
  kSegmentationMetricsData = 7,
  // Filter based on browser type, on Android only (for now). This may
  // expand to other platforms when they need filtering based on browser
  // type as well.
  kHistoryBrowserTypeFilter = 8,
};

// The options that may be specified when fetching URL visit data.
struct FetchOptions {
  // Type of result URLVisitAggregate, note that each visit can match multiple
  // types. If any of the types match, then the URL will be returned. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class URLType {
    kUnknown = 0,
    // The visit has an active local tab.
    kActiveLocalTab = 1,
    // The visit has an active remote tab, based on the latest sync.
    kActiveRemoteTab = 2,
    // The visit is recorded in history, is not from remote client.
    kLocalVisit = 3,
    // The visit is recorded in history, is from a remote client.
    kRemoteVisit = 4,
    // The visit is local and registered with app ID from an Android CCT
    // (Android only).
    kCCTVisit = 5,
    kMaxValue = kCCTVisit,
  };
  using URLTypeSet =
      base::EnumSet<URLType, URLType::kUnknown, URLType::kMaxValue>;
  static constexpr URLTypeSet kAllResultTypes = {
      URLType::kActiveLocalTab, URLType::kActiveRemoteTab, URLType::kLocalVisit,
      URLType::kRemoteVisit,
#if BUILDFLAG(IS_ANDROID)
      URLType::kCCTVisit
#endif
  };

  // Options to specify the expected results.
  struct ResultOption {
    // Any visit within the `age_limit` will be retained.
    base::TimeDelta age_limit;
  };

  using Source = URLVisit::Source;
  using FetchSources =
      base::EnumSet<Source, Source::kNotApplicable, Source::kForeign>;
  FetchOptions(
      std::map<URLType, ResultOption> result_sources_arg,
      std::map<Fetcher, FetchSources> fetcher_sources_arg,
      base::Time begin_time_arg,
      std::vector<URLVisitAggregatesTransformType> transforms_arg = {});
  FetchOptions(const FetchOptions&);
  FetchOptions(FetchOptions&& other);
  FetchOptions& operator=(FetchOptions&& other);
  ~FetchOptions();

  // The set of sources that correspond to an origin.
  static constexpr FetchSources kOriginSources = {Source::kLocal,
                                                  Source::kForeign};

  // Returns the default fetch options for tab resumption use cases.
  static FetchOptions CreateDefaultFetchOptionsForTabResumption();

  // Returns the default fetch options for fetching the expected
  // `result_sources`.
  static FetchOptions CreateFetchOptionsForTabResumption(
      const URLTypeSet& result_sources);

  // The source of expected results. A visit can have multiple types, if any of
  // the types match the `result_sources`, then the visit can be returned.
  std::map<URLType, ResultOption> result_sources;

  // The set of data fetchers that should participate in the data fetching and
  // computation of URLVisit data, including their data source characteristics.
  // Mainly useful for turning off a fetcher for performance or stability issue.
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
