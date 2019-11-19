// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_ML_APP_RANK_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_ML_APP_RANK_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.pb.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace app_list {

// Creates a RankerExample with the given |features| and provided parameters.
// Calculates other features (ClicksEachHour, ClickPerHour, FourHourClicks,
// SixHourClicks). Converts the app id into the URL format used in the ML model.
assist_ranker::RankerExample CreateRankerExample(
    const AppLaunchFeatures& features,
    int time_since_last_click,
    int total_hours,
    int day_of_week,
    int hour_of_day,
    int all_clicks_last_hour,
    int all_clicks_last_24_hours);

// Provide the app ranking using an ML model.
// Rankings are created asynchronously using the ML Service and retrieved
// synchronously at any time.
// Sequencing: Must be created and used on the same sequence (typically the UI
// thread).
class MlAppRankProvider {
 public:
  MlAppRankProvider();
  ~MlAppRankProvider();

  // Asynchronously generates ranking scores for the apps in |app_features_map|.
  void CreateRankings(
      const base::flat_map<std::string, AppLaunchFeatures>& app_features_map,
      int total_hours,
      int all_clicks_last_hour,
      int all_clicks_last_24_hours);

  // Returns a map of the ranking scores keyed by app id.
  // This will return an empty map until some time after the first call to
  // CreateRankings().
  std::map<std::string, float> RetrieveRankings();

 private:
  // Execute the |executor_| on the creation thread.
  void RunExecutor(
      base::flat_map<std::string,
                     ::chromeos::machine_learning::mojom::TensorPtr> inputs,
      std::vector<std::string> outputs,
      std::string app_id);

  // Stores the ranking score for an |app_id| in the |ranking_map_|.
  // Executed by the ML Service when an Execute call is complete.
  void ExecuteCallback(
      std::string app_id,
      ::chromeos::machine_learning::mojom::ExecuteResult result,
      base::Optional<
          std::vector<::chromeos::machine_learning::mojom::TensorPtr>> outputs);

  // Initializes the graph executor for the ML service if it's not already
  // available.
  void BindGraphExecutorIfNeeded();

  void OnConnectionError();

  // Remotes used to execute functions in the ML service server end.
  mojo::Remote<::chromeos::machine_learning::mojom::Model> model_;
  mojo::Remote<::chromeos::machine_learning::mojom::GraphExecutor> executor_;

  // Map from app id to ranking score.
  std::map<std::string, float> ranking_map_;

  // Runner for tasks that should run on the creation sequence.
  scoped_refptr<base::SequencedTaskRunner> creation_task_runner_;

  // Runner for low priority background tasks.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Sequence checker for methods that must run on the creation sequence.
  SEQUENCE_CHECKER(creation_sequence_checker_);

  base::WeakPtrFactory<MlAppRankProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MlAppRankProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_ML_APP_RANK_PROVIDER_H_
