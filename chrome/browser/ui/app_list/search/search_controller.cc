// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"

using metrics::ChromeOSAppListLaunchEventProto;

namespace app_list {

namespace {

constexpr char kLogDisplayTypeClickedResultZeroState[] =
    "Apps.LogDisplayTypeClickedResultZeroState";

// TODO(931149): Move the string manipulation utilities into a helper class.

// Normalizes training targets by removing any scheme prefix and trailing slash:
// "arc://[id]/" to "[id]". This is necessary because apps launched from
// different parts of the launcher have differently formatted IDs.
std::string NormalizeId(const std::string& id) {
  std::string result(id);
  // No existing scheme names include the delimiter string "://".
  std::size_t delimiter_index = result.find("://");
  if (delimiter_index != std::string::npos)
    result.erase(0, delimiter_index + 3);
  if (!result.empty() && result.back() == '/')
    result.pop_back();
  return result;
}

// Remove the Arc app shortcut label from an app ID, if it exists, so that
// "[app]/[label]" becomes "[app]".
std::string RemoveAppShortcutLabel(const std::string& id) {
  std::string result(id);
  std::size_t delimiter_index = result.find_last_of('/');
  if (delimiter_index != std::string::npos)
    result.erase(delimiter_index);
  return result;
}

}  // namespace

SearchController::SearchController(AppListModelUpdater* model_updater,
                                   AppListControllerDelegate* list_controller,
                                   Profile* profile)
    : profile_(profile),
      mixer_(std::make_unique<Mixer>(model_updater)),
      list_controller_(list_controller) {}

SearchController::~SearchController() {}

void SearchController::InitializeRankers() {
  std::unique_ptr<SearchResultRanker> ranker =
      std::make_unique<SearchResultRanker>(
          profile_, HistoryServiceFactory::GetForProfile(
                        profile_, ServiceAccessType::EXPLICIT_ACCESS));
  ranker->InitializeRankers(this);
  mixer_->SetNonAppSearchResultRanker(std::move(ranker));
}

void SearchController::Start(const base::string16& query) {
  dispatching_query_ = true;
  ash::RecordLauncherIssuedSearchQueryLength(query.length());
  for (const auto& provider : providers_)
    provider->Start(query);

  dispatching_query_ = false;
  last_query_ = query;
  query_for_recommendation_ = query.empty();

  OnResultsChanged();
}

void SearchController::ViewClosing() {
  for (const auto& provider : providers_)
    provider->ViewClosing();
}

void SearchController::OpenResult(ChromeSearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  // Log the display type of the clicked result in zero-state
  if (query_for_recommendation_) {
    UMA_HISTOGRAM_ENUMERATION(kLogDisplayTypeClickedResultZeroState,
                              result->display_type(),
                              ash::SearchResultDisplayType::kLast);
  }

  result->Open(event_flags);

  // Launching apps can take some time. It looks nicer to dismiss the app list.
  // Do not close app list for home launcher.
  if (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode())
    list_controller_->DismissView();
}

void SearchController::InvokeResultAction(ChromeSearchResult* result,
                                          int action_index,
                                          int event_flags) {
  // TODO(xiyuan): Hook up with user learning.
  result->InvokeAction(action_index, event_flags);
}

size_t SearchController::AddGroup(size_t max_results,
                                  double multiplier,
                                  double boost) {
  return mixer_->AddGroup(max_results, multiplier, boost);
}

void SearchController::AddProvider(size_t group_id,
                                   std::unique_ptr<SearchProvider> provider) {
  provider->set_result_changed_callback(
      base::Bind(&SearchController::OnResultsChanged, base::Unretained(this)));
  mixer_->AddProviderToGroup(group_id, provider.get());
  providers_.emplace_back(std::move(provider));
}

void SearchController::OnResultsChanged() {
  if (dispatching_query_)
    return;

  size_t num_max_results =
      query_for_recommendation_
          ? ash::AppListConfig::instance().num_start_page_tiles()
          : ash::AppListConfig::instance().max_search_results();
  mixer_->MixAndPublish(num_max_results, last_query_);
}

ChromeSearchResult* SearchController::FindSearchResult(
    const std::string& result_id) {
  for (const auto& provider : providers_) {
    for (const auto& result : provider->results()) {
      if (result->id() == result_id)
        return result.get();
    }
  }
  return nullptr;
}

void SearchController::OnSearchResultsDisplayed(
    const base::string16& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int launched_index) {
  // Log the impression.
  mixer_->GetNonAppSearchResultRanker()->LogSearchResults(
      trimmed_query, results, launched_index);

  if (trimmed_query.empty()) {
    mixer_->GetNonAppSearchResultRanker()->ZeroStateResultsDisplayed(results);

    // Extract result types for logging.
    std::vector<RankingItemType> result_types;
    for (const auto& result : results) {
      result_types.push_back(
          RankingItemTypeFromSearchResult(*FindSearchResult(result.id)));
    }
    LogZeroStateResultsListMetrics(result_types, launched_index);
  }
}

ChromeSearchResult* SearchController::GetResultByTitleForTest(
    const std::string& title) {
  base::string16 target_title = base::ASCIIToUTF16(title);
  for (const auto& provider : providers_) {
    for (const auto& result : provider->results()) {
      if (result->title() == target_title &&
          result->result_type() ==
              ash::AppListSearchResultType::kInstalledApp &&
          result->display_type() !=
              ash::SearchResultDisplayType::kRecommendation) {
        return result.get();
      }
    }
  }
  return nullptr;
}

SearchResultRanker* SearchController::GetNonAppSearchResultRanker() {
  return mixer_->GetNonAppSearchResultRanker();
}

int SearchController::GetLastQueryLength() const {
  return last_query_.size();
}

void SearchController::Train(AppLaunchData&& app_launch_data) {
  if (app_list_features::IsAppListLaunchRecordingEnabled()) {
    // TODO(crbug.com/951287): Add hashed logging once framework is done.

    // Only record the last launched app if the hashed logging feature flag is
    // enabled, because it is only used by hashed logging.
    if (app_launch_data.ranking_item_type == RankingItemType::kApp) {
      last_launched_app_id_ = NormalizeId(app_launch_data.id);
    } else if (app_launch_data.ranking_item_type ==
               RankingItemType::kArcAppShortcut) {
      last_launched_app_id_ =
          RemoveAppShortcutLabel(NormalizeId(app_launch_data.id));
    }
  }

  // CrOS action recorder.
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {base::StrCat(
          {"SearchResultLaunched-", NormalizeId(app_launch_data.id)})},
      {{"ResultType", static_cast<int>(app_launch_data.ranking_item_type)},
       {"Query", static_cast<int>(
                     base::HashMetricName(base::UTF16ToUTF8(last_query_)))}});

  for (const auto& provider : providers_)
    provider->Train(app_launch_data.id, app_launch_data.ranking_item_type);
  app_launch_data.query = base::UTF16ToUTF8(last_query_);
  mixer_->Train(app_launch_data);
}

void SearchController::AppListShown() {
  for (const auto& provider : providers_)
    provider->AppListShown();
}

}  // namespace app_list
