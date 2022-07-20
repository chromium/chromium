// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_impl.h"

#include <algorithm>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/sequence_token.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/common/string_util.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/search_metrics_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"
#include "components/metrics/structured/structured_mojo_events.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {

SearchControllerImpl::SearchControllerImpl(
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    Profile* profile)
    : profile_(profile),
      mixer_(std::make_unique<Mixer>(model_updater, this)),
      metrics_observer_(
          std::make_unique<SearchMetricsObserver>(profile, notifier)),
      list_controller_(list_controller),
      notifier_(notifier) {
  DCHECK(!app_list_features::IsCategoricalSearchEnabled());
  if (notifier_)
    notifier_->AddObserver(this);
}

SearchControllerImpl::~SearchControllerImpl() {
  if (notifier_)
    notifier_->RemoveObserver(this);
}

void SearchControllerImpl::InitializeRankers() {
  mixer_->InitializeRankers(profile_);
}

void SearchControllerImpl::StartSearch(const std::u16string& query) {
  session_start_ = base::Time::Now();
  dispatching_query_ = true;
  ash::RecordLauncherIssuedSearchQueryLength(query.length());
  for (SearchController::Observer& observer : observer_list_) {
    observer.OnResultsCleared();
  }

  for (const auto& provider : providers_) {
    if (query.empty())
      provider->StartZeroState();
    else
      provider->Start(query);
  }

  dispatching_query_ = false;
  last_query_ = query;
  query_for_recommendation_ = query.empty();

  OnResultsChanged();
}

void SearchControllerImpl::StartZeroState(base::OnceClosure on_done,
                                          base::TimeDelta timeout) {
  // Only used for the productivity launcher.
  // TODO(crbug.com/1269115): Unimplemented.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(on_done), timeout);
}

void SearchControllerImpl::ViewClosing() {
  for (const auto& provider : providers_)
    provider->ViewClosing();
}

void SearchControllerImpl::OpenResult(ChromeSearchResult* result,
                                      int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  // Log the length of the last query that led to the clicked result.
  ash::RecordLauncherClickedSearchQueryLength(last_query_.length());

  const bool dismiss_view_on_open = result->dismiss_view_on_open();

  // Open() may cause |result| to be deleted.
  result->Open(event_flags);

  // Launching apps can take some time. It looks nicer to eagerly dismiss the
  // app list if |result| permits it. Do not close app list for home launcher.
  if (dismiss_view_on_open &&
      (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode())) {
    list_controller_->DismissView();
  }
}

void SearchControllerImpl::InvokeResultAction(
    ChromeSearchResult* result,
    ash::SearchResultActionType action) {
  // TODO(xiyuan): Hook up with user learning.
  result->InvokeAction(action);
}

size_t SearchControllerImpl::AddGroup(size_t max_results) {
  return mixer_->AddGroup(max_results);
}

void SearchControllerImpl::AddProvider(
    size_t group_id,
    std::unique_ptr<SearchProvider> provider) {
  mixer_->AddProviderToGroup(group_id, provider.get());
  provider->set_controller(this);
  provider->set_result_changed_callback(
      base::BindRepeating(&SearchControllerImpl::OnResultsChangedWithType,
                          base::Unretained(this), provider->ResultType()));
  providers_.emplace_back(std::move(provider));
}

void SearchControllerImpl::SetResults(const SearchProvider* provider,
                                      Results results) {
  // Should only be called when IsCategoricalSearchEnabled is true.
  NOTREACHED();
}

void SearchControllerImpl::Publish() {
  // Should only be called when IsCategoricalSearchEnabled is true.
  NOTREACHED();
}

void SearchControllerImpl::OnResultsChangedWithType(
    ash::AppListSearchResultType result_type) {
  OnResultsChanged();
  if (results_changed_callback_)
    results_changed_callback_.Run(result_type);
}

void SearchControllerImpl::OnResultsChanged() {
  if (dispatching_query_)
    return;

  size_t num_max_results =
      query_for_recommendation_
          ? ash::SharedAppListConfig::instance().num_start_page_tiles()
          : ash::SharedAppListConfig::instance().max_search_results();
  mixer_->MixAndPublish(num_max_results, last_query_);
}

ChromeSearchResult* SearchControllerImpl::FindSearchResult(
    const std::string& result_id) {
  for (const auto& provider : providers_) {
    for (const auto& result : provider->results()) {
      if (result->id() == result_id)
        return result.get();
    }
  }
  return nullptr;
}

void SearchControllerImpl::OnImpression(
    ash::AppListNotifier::Location location,
    const std::vector<ash::AppListNotifier::Result>& results,
    const std::u16string& query) {
  if (query.empty() && location == ash::kList && mixer_) {
    ash::SearchResultIdWithPositionIndices results_with_indices;
    for (size_t i = 0; i < results.size(); ++i) {
      results_with_indices.emplace_back(results[i].id, i);
    }
    mixer_->search_result_ranker()->ZeroStateResultsDisplayed(
        results_with_indices);
  }
}

ChromeSearchResult* SearchControllerImpl::GetResultByTitleForTest(
    const std::string& title) {
  std::u16string target_title = base::ASCIIToUTF16(title);
  for (const auto& provider : providers_) {
    for (const auto& result : provider->results()) {
      if (result->title() == target_title &&
          result->result_type() ==
              ash::AppListSearchResultType::kInstalledApp &&
          !result->is_recommendation()) {
        return result.get();
      }
    }
  }
  return nullptr;
}

int SearchControllerImpl::GetLastQueryLength() const {
  return last_query_.size();
}

void SearchControllerImpl::Train(LaunchData&& launch_data) {
  launch_data.query = base::UTF16ToUTF8(last_query_);

  if (app_list_features::IsAppListLaunchRecordingEnabled()) {
    // Record a structured metrics event.
    const base::Time now = base::Time::Now();
    base::Time::Exploded now_exploded;
    now.LocalExplode(&now_exploded);

    metrics::structured::events::v2::launcher_usage::LauncherUsage()
        .SetTarget(NormalizeId(launch_data.id))
        .SetApp(last_launched_app_id_)
        .SetSearchQuery(base::UTF16ToUTF8(last_query_))
        .SetSearchQueryLength(last_query_.size())
        .SetProviderType(static_cast<int>(launch_data.ranking_item_type))
        .SetHour(now_exploded.hour)
        .SetScore(launch_data.score)
        .Record();

    // Only record the last launched app if the hashed logging feature flag is
    // enabled, because it is only used by hashed logging.
    if (launch_data.ranking_item_type == RankingItemType::kApp) {
      last_launched_app_id_ = NormalizeId(launch_data.id);
    } else if (launch_data.ranking_item_type ==
               RankingItemType::kArcAppShortcut) {
      last_launched_app_id_ =
          RemoveAppShortcutLabel(NormalizeId(launch_data.id));
    }
  }

  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kLauncherResultEverLaunched,
                                   true);

  // CrOS action recorder.
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat({"SearchResultLaunched-", NormalizeId(launch_data.id)})},
      {{"ResultType", static_cast<int>(launch_data.ranking_item_type)},
       {"Query", static_cast<int>(
                     base::HashMetricName(base::UTF16ToUTF8(last_query_)))}});

  // Train all search result ranking models.
  mixer_->Train(launch_data);
}

void SearchControllerImpl::AddObserver(SearchController::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchControllerImpl::RemoveObserver(
    SearchController::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SearchControllerImpl::NotifyResultsAdded(
    const std::vector<ChromeSearchResult*>& results) {
  if (observer_list_.empty())
    return;

  std::vector<const ChromeSearchResult*> observer_results;
  for (auto* result : results)
    observer_results.push_back(const_cast<const ChromeSearchResult*>(result));
  for (SearchController::Observer& observer : observer_list_)
    observer.OnResultsAdded(last_query_, observer_results);
}

std::u16string SearchControllerImpl::get_query() {
  return last_query_;
}

base::Time SearchControllerImpl::session_start() {
  return session_start_;
}

void SearchControllerImpl::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {
  results_changed_callback_ = std::move(callback);
}

void SearchControllerImpl::disable_ranking_for_test() {
  // Only called for the productivity launcher.
  NOTREACHED();
}

}  // namespace app_list
