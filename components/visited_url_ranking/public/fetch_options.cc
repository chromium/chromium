// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/fetch_options.h"

#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

namespace {

// Get the default age limit for the `url_type`.
base::TimeDelta GetDefaultAgeLimit(FetchOptions::URLType url_type) {
  switch (url_type) {
    case FetchOptions::URLType::kActiveLocalTab:
    case FetchOptions::URLType::kActiveRemoteTab:
      return base::Hours(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService, features::kTabAgeThresholdHours,
          features::kTabAgeThresholdHoursDefaultValue));
    case FetchOptions::URLType::kLocalVisit:
    case FetchOptions::URLType::kRemoteVisit:
    case FetchOptions::URLType::kCCTVisit:
      return base::Hours(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          features::kHistoryAgeThresholdHours,
          features::kHistoryAgeThresholdHoursDefaultValue));
    case FetchOptions::URLType::kUnknown:
      return base::TimeDelta();
  }
}

}  // namespace

FetchOptions::FetchOptions(
    std::map<URLType, ResultOption> result_sources_arg,
    base::Time begin_time_arg,
    std::vector<URLVisitAggregatesTransformType> transforms_arg)
    : result_sources(std::move(result_sources_arg)),
      begin_time(begin_time_arg),
      transforms(std::move(transforms_arg)) {
  DCHECK(!result_sources.empty());
  DCHECK(!begin_time.is_null());

  if (result_sources.count(FetchOptions::URLType::kActiveRemoteTab)) {
    // TODO(ssid): the recency filter and signal aggregation should detect the
    // local tabs from sync correctly. Fix that and enable fetching local tabs
    // from sync.
    fetcher_sources.emplace(Fetcher::kSession,
                            FetchSources({Source::kForeign}));
  }
  // Required to make sure the module can resume an active tab with the URL.
  bool disable_local_fetcher =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService, "disable_local_tab_model",
          false) &&
      result_sources.count(FetchOptions::URLType::kActiveLocalTab) == 0;
  if (!disable_local_fetcher) {
    fetcher_sources.emplace(Fetcher::kTabModel, kOriginSources);
  }
  // Always useful for signals.
  fetcher_sources.emplace(Fetcher::kHistory, kOriginSources);
}

FetchOptions::FetchOptions(
    std::map<Fetcher, FetchSources> fetcher_sources_arg,
    base::Time begin_time_arg,
    std::vector<URLVisitAggregatesTransformType> transforms_arg)
    : fetcher_sources(std::move(fetcher_sources_arg)),
      begin_time(begin_time_arg),
      transforms(std::move(transforms_arg)) {
  CHECK_IS_TEST();
  ResultOption result_option{.age_limit = base::Time::Now() - begin_time_arg};
  for (const URLType type : kAllResultTypes) {
    result_sources.emplace(type, result_option);
  }
}

FetchOptions::~FetchOptions() = default;

FetchOptions::FetchOptions(const FetchOptions&) = default;
FetchOptions::FetchOptions(FetchOptions&& other) = default;

FetchOptions& FetchOptions::operator=(FetchOptions&& other) = default;

// static
FetchOptions FetchOptions::CreateDefaultFetchOptionsForTabResumption() {
  return CreateFetchOptionsForTabResumption({
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
      FetchOptions::URLType::kActiveLocalTab,
#endif
      FetchOptions::URLType::kActiveRemoteTab,
      FetchOptions::URLType::kLocalVisit,
      FetchOptions::URLType::kRemoteVisit,
#if BUILDFLAG(IS_ANDROID)
      // Available in Android only.
      FetchOptions::URLType::kCCTVisit,
#endif
  });
}

// static
FetchOptions FetchOptions::CreateFetchOptionsForTabResumption(
    const URLTypeSet& result_sources) {
  int query_duration = base::GetFieldTrialParamByFeatureAsInt(
      features::kVisitedURLRankingService,
      features::kVisitedURLRankingFetchDurationInHoursParam, 24);
  std::map<URLType, ResultOption> result_map;
  for (FetchOptions::URLType type : result_sources) {
    result_map[type] = ResultOption{.age_limit = GetDefaultAgeLimit(type)};
  }
  std::vector<URLVisitAggregatesTransformType> transforms{
      URLVisitAggregatesTransformType::kRecencyFilter,
      URLVisitAggregatesTransformType::kBookmarkData,
#if BUILDFLAG(IS_ANDROID)
      URLVisitAggregatesTransformType::kDefaultAppUrlFilter,
#endif
  };
  if (base::FeatureList::IsEnabled(
          features::kVisitedURLRankingHistoryVisibilityScoreFilter)) {
    transforms.push_back(
        URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter);
  }

  if (base::FeatureList::IsEnabled(
          features::kVisitedURLRankingSegmentationMetricsData)) {
    transforms.push_back(
        URLVisitAggregatesTransformType::kSegmentationMetricsData);
  }

  FetchOptions options(result_map,
                       base::Time::Now() - base::Hours(query_duration),
                       std::move(transforms));
  return options;
}

}  // namespace visited_url_ranking
