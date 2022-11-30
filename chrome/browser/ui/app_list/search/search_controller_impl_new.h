// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_NEW_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_NEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/app_list/search/burnin_controller.h"
#include "chrome/browser/ui/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ui/app_list/search/ranking/ranker_delegate.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace ash {
class AppListNotifier;
enum class AppListSearchResultType;
}  // namespace ash

namespace app_list {

class SearchMetricsManager;
class SearchProvider;
enum class RankingItemType;

// TODO(crbug.com/1199206): This is the new implementation of the search
// controller. Once we have fully migrated to the new system, this can replace
// SearchController.
class SearchControllerImplNew : public SearchController {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

  SearchControllerImplNew(AppListModelUpdater* model_updater,
                          AppListControllerDelegate* list_controller,
                          ash::AppListNotifier* notifier,
                          Profile* profile);
  ~SearchControllerImplNew() override;

  SearchControllerImplNew(const SearchControllerImplNew&) = delete;
  SearchControllerImplNew& operator=(const SearchControllerImplNew&) = delete;

  // SearchController:
  void StartSearch(const std::u16string& query) override;
  void StartZeroState(base::OnceClosure on_done,
                      base::TimeDelta timeout) override;
  void OpenResult(ChromeSearchResult* result, int event_flags) override;
  void InvokeResultAction(ChromeSearchResult* result,
                          ash::SearchResultActionType action) override;
  size_t AddGroup(size_t max_results) override;
  void AddProvider(size_t group_id,
                   std::unique_ptr<SearchProvider> provider) override;
  void SetResults(const SearchProvider* provider, Results results) override;
  void Publish() override;
  ChromeSearchResult* FindSearchResult(const std::string& result_id) override;
  ChromeSearchResult* GetResultByTitleForTest(
      const std::string& title) override;
  void Train(LaunchData&& launch_data) override;
  void AppListClosing() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) override;
  std::u16string get_query() override;
  base::Time session_start() override;
  void disable_ranking_for_test() override;

  void set_ranker_delegate_for_test(
      std::unique_ptr<RankerDelegate> ranker_delegate) {
    ranker_ = std::move(ranker_delegate);
  }

 private:
  friend class SearchControllerImplNewTest;

  // Rank the results of |provider_type|.
  void Rank(ash::AppListSearchResultType provider_type);

  void SetSearchResults(const SearchProvider* provider);

  void SetZeroStateResults(const SearchProvider* provider);

  void OnZeroStateTimedOut();

  void OnBurnInPeriodElapsed();

  void OnResultsChangedWithType(ash::AppListSearchResultType result_type);

  Profile* profile_;
  std::unique_ptr<BurnInController> burnin_controller_;

  // The query associated with the most recent search.
  std::u16string last_query_;

  // How many search providers should block zero-state until they return
  // results.
  int total_zero_state_blockers_ = 0;

  // How many zero-state blocking providers have returned for this search.
  int returned_zero_state_blockers_ = 0;

  // A timer to trigger a Publish at the end of the timeout period passed to
  // StartZeroState.
  base::OneShotTimer zero_state_timeout_;

  // The callback to indicate zero-state should be published. It is reset after
  // calling, and has_value is used as a flag for whether zero-state has
  // published.
  absl::optional<base::OnceClosure> on_zero_state_done_;

  // The time when StartSearch was most recently called.
  base::Time session_start_;

  // The ID of the most recently launched app. This is used for app list launch
  // recording.
  std::string last_launched_app_id_;

  // Top-level result ranker.
  std::unique_ptr<RankerDelegate> ranker_;

  bool disable_ranking_for_test_ = false;

  // Storage for all search results for the current query.
  ResultsMap results_;

  // Storage for category scores for the current query.
  CategoriesList categories_;

  // If set, called when results set by a provider change.
  ResultsChangedCallback results_changed_callback_;

  std::unique_ptr<SearchMetricsManager> metrics_manager_;
  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  AppListModelUpdater* const model_updater_;
  AppListControllerDelegate* const list_controller_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_NEW_H_
