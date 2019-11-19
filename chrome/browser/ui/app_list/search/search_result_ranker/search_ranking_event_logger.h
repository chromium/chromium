// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RANKING_EVENT_LOGGER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RANKING_EVENT_LOGGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/search_ranking_event.pb.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

class ChromeSearchResult;

namespace app_list {

// TODO(crbug.com/1006133): This is class logs and does inference on a search
// ranking event. The class name doesn't reflect what it does. We should
// refactor it in future CLs.
class SearchRankingEventLogger {
 public:
  SearchRankingEventLogger(Profile* profile,
                           SearchController* search_controller);
  ~SearchRankingEventLogger();
  // Called if a search result item got clicked, or a list of search result has
  // been shown to the user after a certain amount of time. |raw_query| is the
  // raw query that produced the results, |results| is a list of items that were
  // being shown to the users and their corresponding position indices of them
  // (see |SearchResultIdWithPositionIndex| for more details),
  // |position_index| is the position index of the clicked item (if no item got
  // clicked, |position_index| will be -1).
  void Log(const base::string16& trimmed_query,
           const ash::SearchResultIdWithPositionIndices& search_results,
           int launched_index);

  // Sets a testing-only closure to inform tests when a UKM event has been
  // recorded.
  void SetEventRecordedForTesting(base::OnceClosure closure);

  // Computes scores for a list of search result item using ML Service.
  void CreateRankings(Mixer::SortedResults* results, int query_length);
  // Retrieve the scores.
  std::map<std::string, float> RetrieveRankings();

 private:
  // Stores state necessary for logging a given search result that is
  // accumulated throughout the session.
  struct ResultState {
    ResultState();
    ~ResultState();

    base::Optional<base::Time> last_launch = base::nullopt;
    // Initialises all elements to 0.
    int launches_per_hour[24] = {};
    int launches_this_session = 0;
  };

  // Populate a SearchRankingItem proto for an item. |use_for_logging| is a
  // bool to determine the proto will be used for logging or inference
  // purpose.
  void PopulateSearchRankingItem(SearchRankingItem* proto,
                                 ChromeSearchResult* search_result,
                                 int query_length,
                                 bool use_for_logging);

  // Calls the UKM API for a source ID relevant to |result|, and then begins the
  // logging process by calling LogEvent.
  void GetBackgroundSourceIdAndLogEvent(const SearchRankingItem& result);

  // Logs the given event to UKM. If |source_id| is nullopt then use a blank
  // source ID.
  void LogEvent(const SearchRankingItem& result,
                base::Optional<ukm::SourceId> source_id);

  // Create vectorized features from SearchRankingItem. Returns true if
  // |vectorized_features| is successfully populated.
  bool PreprocessInput(const SearchRankingItem::Features& features,
                       std::vector<float>* vectorized_features);

  // Call ML Service to do the inference.
  void DoInference(const std::vector<float>& features, const std::string& id);

  // Stores the ranking score for an |app_id| in the |ranking_map_|.
  // Executed by the ML Service when an Execute call is complete.
  void ExecuteCallback(
      const std::string& id,
      ::chromeos::machine_learning::mojom::ExecuteResult result,
      base::Optional<
          std::vector<::chromeos::machine_learning::mojom::TensorPtr>> outputs);

  void LazyInitialize();

  // Initializes the graph executor for the ML service if it's not already
  // available.
  void BindGraphExecutorIfNeeded();

  void OnConnectionError();

  std::map<std::string, float> prediction_;

  // Remotes used to execute functions in the ML service server end.
  mojo::Remote<::chromeos::machine_learning::mojom::Model> model_;
  mojo::Remote<::chromeos::machine_learning::mojom::GraphExecutor> executor_;

  std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
      preprocessor_config_;

  SearchController* search_controller_;
  // Some events do not have an associated URL and so are logged directly with
  // |ukm_recorder_| using a blank source ID. Other events need to validate the
  // URL before recording, and use |ukm_background_recorder_|.
  ukm::UkmRecorder* ukm_recorder_;
  ukm::UkmBackgroundRecorderService* ukm_background_recorder_;

  // TODO(972817): Zero-state previous query results change their URL based on
  // the position they should be displayed at in the launcher. Because their
  // ID changes, we lose information on their result state. If, in future, we
  // want to rank these results and want more information, we should normalize
  // their IDs to remove the position information.
  std::map<std::string, ResultState> id_to_result_state_;

  // The next, unused, event ID.
  int next_event_id_ = 1;

  // Testing-only closure to inform tests when a UKM event has been recorded.
  base::OnceClosure event_recorded_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SearchRankingEventLogger> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchRankingEventLogger);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RANKING_EVENT_LOGGER_H_
