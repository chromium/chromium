// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_metrics_observer.h"

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {
namespace {

// Represents possible error states of the metrics observer itself. These values
// persist to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class Error {
  kMissingNotifier = 0,
  kResultNotFound = 1,
  kUntrackedLocation = 2,
  kMaxValue = kUntrackedLocation
};

// Represents the 'overall' states of a launcher usage recorded by
// Apps.AppList.UserEvent.Overall.*.
enum class Action {
  kImpression = 0,
  kLaunch = 1,
  kAbandon = 2,
  kIgnore = 3,
  kMaxValue = kIgnore
};

void LogError(Error error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.UserEvent.Error", error);
}

std::string GetHistogramSuffix(ash::AppListNotifier::Location location,
                               const base::string16& raw_query) {
  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
  if (location == ash::AppListNotifier::Location::kList) {
    return query.empty() ? "ListZeroState" : "ListSearch";
  } else if (location == ash::AppListNotifier::Location::kTile) {
    return query.empty() ? "AppsZeroState" : "AppsSearch";
  } else if (location == ash::AppListNotifier::Location::kChip) {
    return "Chip";
  } else {
    LogError(Error::kUntrackedLocation);
    return "Untracked";
  }
}

void LogTypeAction(const std::string& histogram_prefix,
                   ash::AppListNotifier::Location location,
                   const base::string16& query,
                   const SearchMetricsObserver::Result& result) {
  const std::string histogram_name = base::StrCat(
      {histogram_prefix, ".", GetHistogramSuffix(location, query)});
  base::UmaHistogramEnumeration(histogram_name, result.type,
                                ash::SEARCH_RESULT_TYPE_BOUNDARY);
}

void LogOverallAction(ash::AppListNotifier::Location location,
                      const base::string16& query,
                      Action action) {
  const std::string histogram_name = base::StrCat(
      {"Apps.AppList.UserEvent.Overall.", GetHistogramSuffix(location, query)});
  base::UmaHistogramEnumeration(histogram_name, action);
}

}  // namespace

SearchMetricsObserver::SearchMetricsObserver(ash::AppListNotifier* notifier) {
  if (notifier) {
    observer_.Add(notifier);
  } else {
    LogError(Error::kMissingNotifier);
  }
}

SearchMetricsObserver::~SearchMetricsObserver() = default;

void SearchMetricsObserver::OnImpression(
    ash::AppListNotifier::Location location,
    const std::vector<Result>& results,
    const base::string16& query) {
  for (const Result& result : results) {
    LogTypeAction("Apps.AppList.UserEvent.TypeImpression", location, query,
                  result);
  }
  LogOverallAction(location, query, Action::kImpression);
}

void SearchMetricsObserver::OnAbandon(ash::AppListNotifier::Location location,
                                      const std::vector<Result>& results,
                                      const base::string16& query) {
  for (const auto& result : results) {
    LogTypeAction("Apps.AppList.UserEvent.TypeAbandon", location, query,
                  result);
  }
  LogOverallAction(location, query, Action::kAbandon);
}

void SearchMetricsObserver::OnLaunch(ash::AppListNotifier::Location location,
                                     const Result& launched,
                                     const std::vector<Result>& shown,
                                     const base::string16& query) {
  LogTypeAction("Apps.AppList.UserEvent.TypeLaunch", location, query, launched);
  LogOverallAction(location, query, Action::kLaunch);
}

void SearchMetricsObserver::OnIgnore(ash::AppListNotifier::Location location,
                                     const std::vector<Result>& results,
                                     const base::string16& query) {
  LogOverallAction(location, query, Action::kIgnore);
}

}  // namespace app_list
