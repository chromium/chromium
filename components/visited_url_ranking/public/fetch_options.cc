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
    std::map<Fetcher, FetchSources> fetcher_sources_arg,
    base::Time begin_time_arg,
    std::vector<URLVisitAggregatesTransformType> transforms_arg)
    : result_sources(std::move(result_sources_arg)),
      fetcher_sources(std::move(fetcher_sources_arg)),
      begin_time(begin_time_arg),
      transforms(std::move(transforms_arg)) {
  DCHECK(!result_sources.empty());
  DCHECK(!fetcher_sources.empty());
  DCHECK(!begin_time.is_null());
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
  std::vector<URLVisitAggregatesTransformType> transforms{
      URLVisitAggregatesTransformType::kRecencyFilter,
      URLVisitAggregatesTransformType::kBookmarkData,
#if BUILDFLAG(IS_ANDROID)
      URLVisitAggregatesTransformType::kDefaultAppUrlFilter,
      URLVisitAggregatesTransformType::kHistoryBrowserTypeFilter,
#endif
  };
  if (base::FeatureList::IsEnabled(
          features::kVisitedURLRankingHistoryVisibilityScoreFilter)) {
    transforms.push_back(
        URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          features::kVisitedURLRankingSegmentationMetricsData)) {
    transforms.push_back(
        URLVisitAggregatesTransformType::kSegmentationMetricsData);
  }
#endif

  std::map<Fetcher, FetchSources> fetcher_sources;
  // Always useful for signals.
  fetcher_sources.emplace(Fetcher::kHistory, kOriginSources);
  if (result_sources.Has(FetchOptions::URLType::kActiveRemoteTab)) {
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
      result_sources.Has(FetchOptions::URLType::kActiveLocalTab) == 0;
  if (!disable_local_fetcher) {
    fetcher_sources.emplace(Fetcher::kTabModel, FetchSources({Source::kLocal}));
  }

  int query_duration = base::GetFieldTrialParamByFeatureAsInt(
      features::kVisitedURLRankingService,
      features::kVisitedURLRankingFetchDurationInHoursParam, 168);
  std::map<URLType, ResultOption> result_map;
  for (FetchOptions::URLType type : result_sources) {
    result_map[type] = ResultOption{.age_limit = GetDefaultAgeLimit(type)};
  }
  return FetchOptions(std::move(result_map), std::move(fetcher_sources),
                      base::Time::Now() - base::Hours(query_duration),
                      std::move(transforms));
}

}  // namespace visited_url_ranking
