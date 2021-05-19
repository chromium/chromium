// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_impl_new.h"

#include <algorithm>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
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
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "chrome/browser/ui/app_list/search/ranking/category_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/filtering_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"
#include "chrome/browser/ui/app_list/search/ranking/score_normalizing_ranker.h"
#include "chrome/browser/ui/app_list/search/ranking/top_match_ranker.h"
#include "chrome/browser/ui/app_list/search/search_metrics_observer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"
#include "components/metrics/structured/structured_events.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {

namespace {

constexpr char kLauncherSearchQueryLengthJumped[] =
    "Apps.LauncherSearchQueryLengthJumped";

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

SearchControllerImplNew::SearchControllerImplNew(
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    Profile* profile)
    : profile_(profile),
      ranker_(std::make_unique<RankerDelegate>(profile, this)),
      metrics_observer_(std::make_unique<SearchMetricsObserver>(notifier)),
      model_updater_(model_updater),
      list_controller_(list_controller) {
  DCHECK(app_list_features::IsCategoricalSearchEnabled());
}

SearchControllerImplNew::~SearchControllerImplNew() {}

void SearchControllerImplNew::InitializeRankers() {
  ranker_->AddRanker(std::make_unique<ScoreNormalizingRanker>(profile_));
  ranker_->AddRanker(std::make_unique<CategoryRanker>(profile_));
  ranker_->AddRanker(std::make_unique<TopMatchRanker>());
  ranker_->AddRanker(std::make_unique<FilteringRanker>());
}

void SearchControllerImplNew::Start(const std::u16string& query) {
  session_start_ = base::Time::Now();

  // TODO(crbug.com/1199206): We should move this histogram logic somewhere
  // else.
  ash::RecordLauncherIssuedSearchQueryLength(query.length());
  if (query.length() > 0) {
    const int length_diff = query.length() >= last_query_.length()
                                ? query.length() - last_query_.length()
                                : last_query_.length() - query.length();

    UMA_HISTOGRAM_BOOLEAN(kLauncherSearchQueryLengthJumped, length_diff > 1);
  }

  last_query_ = query;
  results_.clear();
  ranker_->Start(query);
  for (const auto& provider : providers_)
    provider->Start(query);
}

void SearchControllerImplNew::OpenResult(ChromeSearchResult* result,
                                         int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  // Log the length of the last query that led to the clicked result.
  // TODO(crbug.com/1199206): This histogram logic should be moved somewhere
  // else.
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

void SearchControllerImplNew::InvokeResultAction(ChromeSearchResult* result,
                                                 int action_index) {
  if (!result)
    return;
  result->InvokeAction(action_index);
}

size_t SearchControllerImplNew::AddGroup(size_t max_results) {
  // Unused.
  return 0ul;
}

void SearchControllerImplNew::AddProvider(
    size_t group_id,
    std::unique_ptr<SearchProvider> provider) {
  provider->set_controller(this);
  providers_.emplace_back(std::move(provider));
}

void SearchControllerImplNew::SetResults(
    const ash::AppListSearchResultType provider_type,
    Results results) {
  DCHECK(ranker_);

  // Re-post onto the UI sequence if not called from there.
  auto ui_thread = content::GetUIThreadTaskRunner({});
  if (!ui_thread->RunsTasksInCurrentSequence()) {
    ui_thread->PostTask(FROM_HERE,
                        base::BindOnce(&SearchControllerImplNew::SetResults,
                                       base::Unretained(this), provider_type,
                                       std::move(results)));
    return;
  }

  results_[provider_type] = std::move(results);

  // Update ranking of all results.
  ranker_->Rank(results_, provider_type);

  // Compile a single list of results and sort by their relevance.
  std::vector<ChromeSearchResult*> all_results;
  for (const auto& type_results : results_) {
    for (const auto& result : type_results.second) {
      // TODO(crbug.com/1199206): Category-based search combines apps into the
      // results list, so redirect any kTile results to kList before updating
      // the UI. Once SearchControllerImplNew is the only search controller,
      // this can be removed and all results can be created as kList.
      if (result->display_type() == ash::SearchResultDisplayType::kTile) {
        result->SetDisplayType(ash::SearchResultDisplayType::kList);
      }

      // Filter out results with negative relevance, which is the rankers'
      // signal that a result should not be displayed at all.
      if (result->relevance() > 0.0) {
        all_results.push_back(result.get());
      }
    }
  }
  std::sort(all_results.begin(), all_results.end(),
            [](const ChromeSearchResult* a, const ChromeSearchResult* b) {
              return a->relevance() > b->relevance();
            });

  // TODO(crbug.com/1199206): Remove this debug output once we no longer need to
  // inspect launcher scores so closely, and before the categorical search flag
  // is enabled for any users.
  LOG(ERROR) << "(categorical search) updating results to:";
  for (const auto* result : all_results) {
    LOG(ERROR) << "(categorical search) - " << result->relevance() << "  "
               << result->id();
  }

  model_updater_->PublishSearchResults(all_results);
}

ChromeSearchResult* SearchControllerImplNew::FindSearchResult(
    const std::string& result_id) {
  for (const auto& provider_results : results_) {
    for (const auto& result : provider_results.second) {
      if (result->id() == result_id)
        return result.get();
    }
  }
  return nullptr;
}

void SearchControllerImplNew::OnSearchResultsImpressionMade(
    const std::u16string& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int launched_index) {
  if (trimmed_query.empty()) {
    // Extract result types for logging.
    std::vector<RankingItemType> result_types;
    for (const auto& result : results) {
      result_types.push_back(
          RankingItemTypeFromSearchResult(*FindSearchResult(result.id)));
    }
  }
}

ChromeSearchResult* SearchControllerImplNew::GetResultByTitleForTest(
    const std::string& title) {
  std::u16string target_title = base::ASCIIToUTF16(title);
  for (const auto& provider_results : results_) {
    for (const auto& result : provider_results.second) {
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

int SearchControllerImplNew::GetLastQueryLength() const {
  return last_query_.size();
}

void SearchControllerImplNew::Train(LaunchData&& launch_data) {
  launch_data.query = base::UTF16ToUTF8(last_query_);

  // TODO(crbug.com/1199206): This logging code should move elsewhere.
  if (app_list_features::IsAppListLaunchRecordingEnabled()) {
    // Record a structured metrics event.
    const base::Time now = base::Time::Now();
    base::Time::Exploded now_exploded;
    now.LocalExplode(&now_exploded);

    metrics::structured::events::launcher_usage::LauncherUsage()
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
  ranker_->Train(launch_data);
}

void SearchControllerImplNew::AppListShown() {
  for (const auto& provider : providers_)
    provider->AppListShown();
}

void SearchControllerImplNew::ViewClosing() {
  for (const auto& provider : providers_)
    provider->ViewClosing();
}

std::u16string SearchControllerImplNew::get_query() {
  return last_query_;
}

base::Time SearchControllerImplNew::session_start() {
  return session_start_;
}

void SearchControllerImplNew::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {
  // Unused.
}

}  // namespace app_list
