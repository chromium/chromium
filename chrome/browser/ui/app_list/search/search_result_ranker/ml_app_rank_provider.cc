// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ml_app_rank_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_helpers.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger_helper.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"

using ::chromeos::machine_learning::mojom::BuiltinModelId;
using ::chromeos::machine_learning::mojom::BuiltinModelSpec;
using ::chromeos::machine_learning::mojom::BuiltinModelSpecPtr;
using ::chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using ::chromeos::machine_learning::mojom::ExecuteResult;
using ::chromeos::machine_learning::mojom::FloatList;
using ::chromeos::machine_learning::mojom::Int64List;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::Tensor;
using ::chromeos::machine_learning::mojom::TensorPtr;
using ::chromeos::machine_learning::mojom::ValueList;

namespace app_list {

namespace {

void LoadModelCallback(LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    LOG(ERROR) << "Failed to load Top Cat model.";
  }
}

void CreateGraphExecutorCallback(CreateGraphExecutorResult result) {
  if (result != CreateGraphExecutorResult::OK) {
    LOG(ERROR) << "Failed to create a Top Cat Graph Executor.";
  }
}

// Returns: true if preprocessor config loaded, false if it could not be loaded.
bool LoadExamplePreprocessorConfig(
    assist_ranker::ExamplePreprocessorConfig* preprocessor_config) {
  DCHECK(preprocessor_config);

  const int resource_id = IDR_TOP_CAT_20190722_EXAMPLE_PREPROCESSOR_CONFIG_PB;
  const scoped_refptr<base::RefCountedMemory> raw_config =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          resource_id);
  if (!raw_config || !raw_config->front()) {
    LOG(ERROR) << "Failed to load TopCatModel example preprocessor config.";
    return false;
  }

  if (!preprocessor_config->ParseFromArray(raw_config->front(),
                                           raw_config->size())) {
    LOG(ERROR) << "Failed to parse TopCatModel example preprocessor config.";
    return false;
  }
  return true;
}

// Perform the inference given the |features| and |app_id| of an app.
// Posts |callback| to |task_runner| to perform the actual inference.
void DoInference(const std::string& app_id,
                 const std::vector<float>& features,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 const base::RepeatingCallback<
                     void(base::flat_map<std::string, TensorPtr> inputs,
                          const std::vector<std::string> outputs,
                          const std::string app_id)> callback) {
  // Prepare the input tensor.
  base::flat_map<std::string, TensorPtr> inputs;
  auto tensor = Tensor::New();
  tensor->shape = Int64List::New();
  tensor->shape->value = std::vector<int64_t>({1, features.size()});
  tensor->data = ValueList::New();
  tensor->data->set_float_list(FloatList::New());
  tensor->data->get_float_list()->value =
      std::vector<double>(std::begin(features), std::end(features));
  inputs.emplace(std::string("input"), std::move(tensor));

  const std::vector<std::string> outputs({std::string("output")});
  DCHECK(task_runner);
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, std::move(inputs),
                                                  std::move(outputs), app_id));
}

// Process the RankerExample to vectorize the feature list for inference.
// Returns true on success.
bool RankerExampleToVectorizedFeatures(
    const assist_ranker::ExamplePreprocessorConfig& preprocessor_config,
    assist_ranker::RankerExample* example,
    std::vector<float>* vectorized_features) {
  int preprocessor_error = assist_ranker::ExamplePreprocessor::Process(
      preprocessor_config, example, true);
  // kNoFeatureIndexFound can occur normally (e.g., when the app URL
  // isn't known to the model or a rarely seen enum value is used).
  if (preprocessor_error != assist_ranker::ExamplePreprocessor::kSuccess &&
      preprocessor_error !=
          assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound) {
    // TODO: Log to UMA.
    return false;
  }

  const auto& extracted_features =
      example->features()
          .at(assist_ranker::ExamplePreprocessor::kVectorizedFeatureDefaultName)
          .float_list()
          .float_value();
  vectorized_features->assign(extracted_features.begin(),
                              extracted_features.end());
  return true;
}

// Does the CPU-intensive part of CreateRankings (preparing the Tensor inputs
// from |app_features_map|, intended to be called on a low-priority
// background thread. Invokes |callback| on |task_runner| once for each app in
// |app_features_map|.
void CreateRankingsImpl(
    base::flat_map<std::string, AppLaunchFeatures> app_features_map,
    int total_hours,
    int all_clicks_last_hour,
    int all_clicks_last_24_hours,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::RepeatingCallback<
        void(base::flat_map<std::string, TensorPtr> inputs,
             const std::vector<std::string> outputs,
             const std::string app_id)>& callback) {
  const base::Time now(base::Time::Now());
  const int hour = HourOfDay(now);
  const int day = DayOfWeek(now);

  assist_ranker::ExamplePreprocessorConfig preprocessor_config;
  if (!LoadExamplePreprocessorConfig(&preprocessor_config)) {
    return;
  }
  for (auto& app : app_features_map) {
    assist_ranker::RankerExample example(
        CreateRankerExample(app.second,
                            now.ToDeltaSinceWindowsEpoch().InSeconds() -
                                app.second.time_of_last_click_sec(),
                            total_hours, day, hour, all_clicks_last_hour,
                            all_clicks_last_24_hours));
    std::vector<float> vectorized_features;
    if (RankerExampleToVectorizedFeatures(preprocessor_config, &example,
                                          &vectorized_features)) {
      DoInference(app.first, vectorized_features, task_runner, callback);
    }
  }
}

}  // namespace

assist_ranker::RankerExample CreateRankerExample(
    const AppLaunchFeatures& features,
    int time_since_last_click,
    int total_hours,
    int day_of_week,
    int hour_of_day,
    int all_clicks_last_hour,
    int all_clicks_last_24_hours) {
  assist_ranker::RankerExample example;
  auto& ranker_example_features = *example.mutable_features();

  ranker_example_features["DayOfWeek"].set_int32_value(day_of_week);
  ranker_example_features["HourOfDay"].set_int32_value(hour_of_day);
  ranker_example_features["AllClicksLastHour"].set_int32_value(
      all_clicks_last_hour);
  ranker_example_features["AllClicksLast24Hours"].set_int32_value(
      all_clicks_last_24_hours);

  ranker_example_features["AppType"].set_int32_value(features.app_type());
  ranker_example_features["ClickRank"].set_int32_value(features.click_rank());
  ranker_example_features["ClicksLastHour"].set_int32_value(
      features.clicks_last_hour());
  ranker_example_features["ClicksLast24Hours"].set_int32_value(
      features.clicks_last_24_hours());
  ranker_example_features["LastLaunchedFrom"].set_int32_value(
      features.last_launched_from());
  ranker_example_features["HasClick"].set_bool_value(
      features.has_most_recently_used_index());
  ranker_example_features["MostRecentlyUsedIndex"].set_int32_value(
      features.most_recently_used_index());
  ranker_example_features["TimeSinceLastClick"].set_int32_value(
      Bucketize(time_since_last_click, kTimeSinceLastClickBuckets));
  ranker_example_features["TotalClicks"].set_int32_value(
      features.total_clicks());
  ranker_example_features["TotalClicksPerHour"].set_float_value(
      static_cast<float>(features.total_clicks()) / (total_hours + 1));
  ranker_example_features["TotalHours"].set_int32_value(total_hours);

  // Calculate FourHourClicksN and SixHourClicksN, which sum clicks for four
  // and six hour periods respectively.
  int four_hour_count = 0;
  int six_hour_count = 0;
  // Apps that have been clicked will have 24 clicks_each_hour values. Apps that
  // have not been clicked will have no clicks_each_hour values, so can skip
  // the FourHourClicksN and SixHourClicksN calculations.
  if (features.clicks_each_hour_size() == 24) {
    for (int hour = 0; hour < 24; hour++) {
      int clicks = Bucketize(features.clicks_each_hour(hour), kClickBuckets);
      ranker_example_features["ClicksEachHour" +
                              base::StringPrintf("%02d", hour)]
          .set_int32_value(clicks);
      ranker_example_features["ClicksPerHour" +
                              base::StringPrintf("%02d", hour)]
          .set_float_value(static_cast<float>(clicks) / (total_hours + 1));
      four_hour_count += clicks;
      six_hour_count += clicks;
      // Divide day into periods of 4 hours each.
      if (hour % 4 == 3 && four_hour_count != 0) {
        ranker_example_features["FourHourClicks" +
                                base::StringPrintf("%01d", hour / 4)]
            .set_int32_value(four_hour_count);
        four_hour_count = 0;
      }
      // Divide day into periods of 6 hours each.
      if (hour % 6 == 5 && six_hour_count != 0) {
        ranker_example_features["SixHourClicks" +
                                base::StringPrintf("%01d", hour / 6)]
            .set_int32_value(six_hour_count);
        six_hour_count = 0;
      }
    }
  }

  if (features.app_type() == AppLaunchEvent_AppType_CHROME) {
    ranker_example_features["URL"].set_string_value(
        kExtensionSchemeWithDelimiter + features.app_id());
  } else if (features.app_type() == AppLaunchEvent_AppType_PWA) {
    ranker_example_features["URL"].set_string_value(features.pwa_url());
  } else if (features.app_type() == AppLaunchEvent_AppType_PLAY) {
    ranker_example_features["URL"].set_string_value(
        kAppScheme +
        crx_file::id_util::GenerateId(features.arc_package_name()));
  } else {
    LOG(ERROR) << "Unknown app type: " << features.app_type();
  }
  return example;
}

MlAppRankProvider::MlAppRankProvider()
    : creation_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      background_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

MlAppRankProvider::~MlAppRankProvider() = default;

void MlAppRankProvider::CreateRankings(
    const base::flat_map<std::string, AppLaunchFeatures>& app_features_map,
    int total_hours,
    int all_clicks_last_hour,
    int all_clicks_last_24_hours) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  // TODO(jennyz): Add start-to-end latency metrics for the work on each
  // sequence.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateRankingsImpl, app_features_map, total_hours,
                     all_clicks_last_hour, all_clicks_last_24_hours,
                     creation_task_runner_,
                     base::BindRepeating(&MlAppRankProvider::RunExecutor,
                                         weak_factory_.GetWeakPtr())));
}

std::map<std::string, float> MlAppRankProvider::RetrieveRankings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  return ranking_map_;
}

void MlAppRankProvider::RunExecutor(
    base::flat_map<std::string, TensorPtr> inputs,
    const std::vector<std::string> outputs,
    const std::string app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  BindGraphExecutorIfNeeded();
  executor_->Execute(std::move(inputs), std::move(outputs),
                     base::BindOnce(&MlAppRankProvider::ExecuteCallback,
                                    base::Unretained(this), app_id));
}

void MlAppRankProvider::ExecuteCallback(
    std::string app_id,
    ExecuteResult result,
    const base::Optional<std::vector<TensorPtr>> outputs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  if (result != ExecuteResult::OK) {
    LOG(ERROR) << "Top Cat inference execution failed.";
    return;
  }
  ranking_map_[app_id] = outputs.value()[0]->data->get_float_list()->value[0];
}

void MlAppRankProvider::BindGraphExecutorIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  if (!model_) {
    // Load the model.
    BuiltinModelSpecPtr spec =
        BuiltinModelSpec::New(BuiltinModelId::TOP_CAT_20190722);
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->LoadBuiltinModel(std::move(spec), model_.BindNewPipeAndPassReceiver(),
                           base::BindOnce(&LoadModelCallback));
  }

  if (!executor_) {
    // Get the graph executor.
    model_->CreateGraphExecutor(executor_.BindNewPipeAndPassReceiver(),
                                base::BindOnce(&CreateGraphExecutorCallback));
    executor_.set_disconnect_handler(base::BindOnce(
        &MlAppRankProvider::OnConnectionError, base::Unretained(this)));
  }
}

void MlAppRankProvider::OnConnectionError() {
  LOG(WARNING) << "Mojo connection for ML service closed.";
  executor_.reset();
  model_.reset();
}

}  // namespace app_list
