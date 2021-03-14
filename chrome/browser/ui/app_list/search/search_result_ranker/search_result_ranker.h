// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

namespace app_list {

class RecurrenceRanker;
enum class RankingItemType;

// SearchResultRanker re-ranks launcher search and zero-state results using a
// collection of on-device models. It can be provided training signals via the
// Train method, which are then forwarded to the appropriate model.
// FetchRankings queries each model for ranking results. Rank modifies the
// scores of provided search results, which are intended to be the output of a
// search provider.
class SearchResultRanker : file_manager::file_tasks::FileTasksObserver {
 public:
  explicit SearchResultRanker(Profile* profile);
  ~SearchResultRanker() override;

  // Performs all setup of rankers. This is separated from the constructor for
  // testing reasons.
  void InitializeRankers(SearchController* search_controller);

  // Queries each model contained with the SearchResultRanker for its results,
  // and saves them for use on subsequent calls to Rank(). The given query may
  // be used as a feature for ranking search results provided to Rank(), but is
  // not used to create new search results. If this is a zero-state scenario,
  // the query should be empty.
  void FetchRankings(const std::u16string& query);

  // Modifies the scores of |results| using the saved rankings. This should be
  // called after rankings have been queried with a call to FetchRankings().
  // Only the scores of elements in |results| are modified, not the
  // ChromeSearchResults themselves.
  void Rank(Mixer::SortedResults* results);

  // Forwards the given training signal to the relevant models contained within
  // the SearchResultRanker.
  void Train(const AppLaunchData& app_launch_data);

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;

  // Sets a testing-only closure to inform tests when a JSON config has been
  // parsed.
  void set_json_config_parsed_for_testing(base::OnceClosure closure) {
    json_config_parsed_for_testing_ = std::move(closure);
  }

  // Called when some zero state |results| have likely been seen by the user.
  // Updates the cache of recently shown results.
  void ZeroStateResultsDisplayed(
      const ash::SearchResultIdWithPositionIndices& results);

  // Given a search results list containing zero-state results, ensure that at
  // least one result from each result group will be displayed if that group has
  // a result shown to the user only a few times.
  void OverrideZeroStateResults(Mixer::SortedResults* results);

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchResultRankerTest,
                           ZeroStateGroupRankerUsesFinchConfig);

  // Calculates the final score for the given zero state |result|, sets
  // |result.score|, and increments the related entry in the  |type_counts| map.
  void ScoreZeroStateItem(
      Mixer::SortData* result,
      RankingItemType type,
      base::flat_map<RankingItemType, int>* type_counts) const;

  // Logs the result score received from a zero state search provider. Results
  // of other types are ignored.
  void LogZeroStateResultScore(RankingItemType type, float score);

  // Records the time of the last call to FetchRankings() and is used to
  // limit the number of queries to the models within a short timespan.
  base::Time time_of_last_fetch_;

  // The query last provided to FetchRankings.
  std::u16string last_query_;

  // Ranks the kinds of results possible in the zero state results list.
  std::unique_ptr<RecurrenceRanker> zero_state_group_ranker_;
  std::map<std::string, float> zero_state_group_ranks_;
  std::unique_ptr<JsonConfigConverter> zero_state_config_converter_;
  // Stores the id of the most recent highest-scoring zero state result from
  // each relevant provider, along with how many times it has been shown to the
  // user.
  base::flat_map<RankingItemType, std::pair<std::string, int>>
      zero_state_results_cache_;

  // Coefficients that control the weighting between different parts of the
  // score of a result in the zero-state results list.
  float zero_state_item_coeff_ = 1.0f;
  float zero_state_group_coeff_ = 0.75f;
  float zero_state_paired_coeff_ = 0.0f;

  // Ranks apps.
  std::unique_ptr<RecurrenceRanker> app_ranker_;
  std::map<std::string, float> app_ranks_;

  // Testing-only closure to inform tests once a JSON config has been parsed.
  base::OnceClosure json_config_parsed_for_testing_;

  // Logs launch events and stores feature data for aggregated model.
  std::unique_ptr<AppLaunchEventLogger> app_launch_event_logger_;

  // Stores the time of the last histogram logging event for each zero state
  // search provider. Used to prevent scores from being logged multiple times
  // for each user action.
  // TODO(959679): Remove these timers once the multiple-call issue is fixed.
  base::Time time_of_last_omnibox_log_;
  base::Time time_of_last_local_file_log_;
  base::Time time_of_last_drive_log_;

  Profile* profile_;

  base::WeakPtrFactory<SearchResultRanker> weak_factory_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_
