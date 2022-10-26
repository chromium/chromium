// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/app_search_data_source.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

namespace {

// Checks if current locale is non Latin locales.
bool IsNonLatinLocale(base::StringPiece locale) {
  // A set of of non Latin locales. This set is used to select appropriate
  // algorithm for app search.
  static constexpr char kNonLatinLocales[][6] = {
      "am", "ar", "be", "bg", "bn",    "el",    "fa",   "gu", "hi",
      "hy", "iw", "ja", "ka", "kk",    "km",    "kn",   "ko", "ky",
      "lo", "mk", "ml", "mn", "mr",    "my",    "pa",   "ru", "sr",
      "ta", "te", "th", "uk", "zh-CN", "zh-HK", "zh-TW"};
  return base::Contains(kNonLatinLocales, locale);
}

}  // namespace

AppSearchProvider::AppSearchProvider(Profile* profile,
                                     AppListControllerDelegate* list_controller,
                                     base::Clock* clock,
                                     AppListModelUpdater* model_updater)
    : model_updater_(model_updater) {
  data_source_ =
      std::make_unique<AppSearchDataSource>(profile, list_controller, clock);
  app_updates_subscription_ =
      data_source_->SubscribeToAppUpdates(base::BindRepeating(
          &AppSearchProvider::UpdateResults, base::Unretained(this)));
}

AppSearchProvider::~AppSearchProvider() = default;

void AppSearchProvider::Start(const std::u16string& query) {
  query_ = query;
  query_start_time_ = base::TimeTicks::Now();
  // We only need to record app search latency for queries started by user.
  // TODO(crbug.com/1258415): Is this needed?
  record_query_uma_ = true;

  {
    // Prevent `UpdateResults()` from running as a result of a data source
    // refresh callback to avoid double update.
    base::AutoReset<bool> auto_reset(&updates_blocked_, true);
    data_source_->RefreshIfNeeded();
  }

  UpdateResults();
}

void AppSearchProvider::StartZeroState() {
  query_.clear();
  query_start_time_ = base::TimeTicks::Now();
  record_query_uma_ = true;

  {
    // Prevent `UpdateResults()` from running as a result of a data source
    // refresh callback to avoid double update.
    base::AutoReset<bool> auto_reset(&updates_blocked_, true);
    data_source_->RefreshIfNeeded();
  }

  UpdateResults();
}

ash::AppListSearchResultType AppSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kInstalledApp;
}

bool AppSearchProvider::ShouldBlockZeroState() const {
  return true;
}

void AppSearchProvider::UpdateRecommendedResults(
    const base::flat_map<std::string, uint16_t>& id_to_app_list_index) {
  SearchProvider::Results new_results =
      data_source_->GetRecommendations(id_to_app_list_index);
  PublishQueriedResultsOrRecommendation(false, &new_results);
}

void AppSearchProvider::UpdateQueriedResults() {
  SearchProvider::Results new_results;

  const bool use_exact_match =
      app_list_features::IsExactMatchForNonLatinLocaleEnabled() &&
      IsNonLatinLocale(base::i18n::GetConfiguredLocale());

  if (use_exact_match) {
    new_results = data_source_->GetExactMatches(query_);
  } else {
    new_results = data_source_->GetFuzzyMatches(query_);
  }

  PublishQueriedResultsOrRecommendation(true, &new_results);
}

void AppSearchProvider::PublishQueriedResultsOrRecommendation(
    bool is_queried_search,
    Results* new_results) {
  MaybeRecordQueryLatencyHistogram(is_queried_search);
  SwapResults(new_results);
}

void AppSearchProvider::MaybeRecordQueryLatencyHistogram(
    bool is_queried_search) {
  // Record the query latency only if search provider is queried by user
  // initiating a search or getting zero state suggestions.
  if (!record_query_uma_)
    return;

  if (is_queried_search) {
    UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.QueryTime",
                        base::TimeTicks::Now() - query_start_time_);
  } else {
    UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.ZeroStateLatency",
                        base::TimeTicks::Now() - query_start_time_);
  }
  record_query_uma_ = false;
}

void AppSearchProvider::UpdateResults() {
  if (updates_blocked_)
    return;

  const bool show_recommendations = query_.empty();

  if (show_recommendations) {
    // Get the map of app ids to their position in the app list, and then
    // update results.
    // Unretained is safe because the callback gets called synchronously.
    model_updater_->GetIdToAppListIndexMap(base::BindOnce(
        &AppSearchProvider::UpdateRecommendedResults, base::Unretained(this)));
  } else {
    UpdateQueriedResults();
  }
}

}  // namespace app_list
