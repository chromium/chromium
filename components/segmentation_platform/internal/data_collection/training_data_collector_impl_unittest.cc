// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"

#include <map>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/cached_result_writer.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/signals/mock_histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/mock_user_action_signal_handler.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using Segmentation_ModelExecution =
    ::ukm::builders::Segmentation_ModelExecution;

constexpr auto kTestOptimizationTarget0 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
constexpr auto kTestOptimizationTarget1 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;
constexpr auto kTestOptimizationTarget2 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR;
constexpr char kHistogramName0[] = "histogram0";
constexpr char kHistogramName1[] = "histogram1";
constexpr char kSegmentationKey[] = "test_key";
constexpr char kSegmentationKey2[] = "test_key_2";
constexpr int64_t kModelVersion = 123;
constexpr int kSample = 1;
constexpr DecisionType kOnDemandDecisionType =
    proto::TrainingOutputs::TriggerConfig::ONDEMAND;
constexpr DecisionType kPeriodicDecisionType =
    proto::TrainingOutputs::TriggerConfig::PERIODIC;

class MockModelManager : public ModelManager {
 public:
  MOCK_METHOD(ModelProvider*,
              GetModelProvider,
              (proto::SegmentId segment_id, proto::ModelSource model_source));

  MOCK_METHOD(void, Initialize, ());

  MOCK_METHOD(
      void,
      SetSegmentationModelUpdatedCallbackForTesting,
      (ModelManager::SegmentationModelUpdatedCallback model_updated_callback));
};

class TrainingDataCollectorImplTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  TrainingDataCollectorImplTest()
      : task_environment_{base::test::TaskEnvironment::TimeSource::MOCK_TIME} {}
  ~TrainingDataCollectorImplTest() override = default;

  void SetUp() override {
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    SegmentationPlatformService::RegisterProfilePrefs(prefs_.registry());
    LocalStateHelper::GetInstance().Initialize(&prefs_);
    LocalStateHelper::GetInstance().SetPrefTime(
        kSegmentationLastCollectionTimePref, base::Time::Now());
    // Set UKM allowed 30 days ago
    LocalStateHelper::GetInstance().SetPrefTime(
        kSegmentationUkmMostRecentAllowedTimeKey,
        base::Time::Now() - base::Days(30));
    clock_.SetNow(base::Time::Now());
    test_recorder_.Purge();

    // Setup behavior for |feature_list_processor_|.
    ModelProvider::Request inputs({1.f});
    ON_CALL(feature_list_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
        .WillByDefault(
            RunOnceCallback<6>(false, inputs, ModelProvider::Response()));

    auto test_segment_info_db =
        std::make_unique<test::TestSegmentInfoDatabase>();
    test_segment_info_db_ = test_segment_info_db.get();

    auto signal_storage_config =
        std::make_unique<NiceMock<MockSignalStorageConfig>>();
    signal_storage_config_ = signal_storage_config.get();

    ON_CALL(*signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
        .WillByDefault(Return(true));

    configs_.emplace_back(std::make_unique<Config>());
    configs_[0]->segmentation_key = kSegmentationKey;
    configs_[0]->segments.insert(
        {kTestOptimizationTarget0,
         std::make_unique<Config::SegmentMetadata>("UmaNameNewTab")});
    configs_[0]->segments.insert(
        {kTestOptimizationTarget1,
         std::make_unique<Config::SegmentMetadata>("UmaNameShare")});

    SegmentationResultPrefs result_prefs(&prefs_);
    SelectedSegment selected_segment(kTestOptimizationTarget1, 10);
    selected_segment.selection_time = base::Time::Now() - base::Days(1);
    result_prefs.SaveSegmentationResultToPref(kSegmentationKey,
                                              selected_segment);

    // Add another configuration under kTestOptimizationTarget2 that uses the
    // new output config with a multi class classifier.
    configs_.emplace_back(std::make_unique<Config>());
    configs_[1]->segmentation_key = kSegmentationKey2;
    configs_[1]->segments.insert(
        {kTestOptimizationTarget2,
         std::make_unique<Config::SegmentMetadata>("UmaNameNewTab")});

    // Create a ClientResult object to store in prefs, it'll be used when
    // recording the training data for kTestOptimizationTarget2.
    proto::ClientResult client_2_result;
    proto::PredictionResult* client_2_prediction_result =
        client_2_result.mutable_client_result();
    proto::OutputConfig* client_2_output_config =
        client_2_prediction_result->mutable_output_config();
    auto* client_2_classifier = client_2_output_config->mutable_predictor()
                                    ->mutable_multi_class_classifier();
    client_2_classifier->add_class_labels("Foo");
    client_2_classifier->add_class_labels("Bar");
    client_2_classifier->add_class_labels("Baz");
    client_2_classifier->add_class_labels("Foo");
    client_2_classifier->add_class_labels("Bar");

    client_2_prediction_result->add_result(0.f);
    client_2_prediction_result->add_result(0.f);
    client_2_prediction_result->add_result(1.f);
    client_2_prediction_result->add_result(0.f);
    client_2_prediction_result->add_result(0.f);
    client_2_prediction_result->set_timestamp_us(
        (base::Time::Now() - base::Days(3))
            .ToDeltaSinceWindowsEpoch()
            .InMicroseconds());

    result_prefs_ = std::make_unique<ClientResultPrefs>(&prefs_);
    CachedResultWriter(result_prefs_.get(), &clock_)
        .UpdatePrefsIfExpired(configs_[1].get(), client_2_result,
                              PlatformOptions::CreateDefault());
    storage_service_ = std::make_unique<StorageService>(
        std::move(test_segment_info_db), nullptr,
        std::move(signal_storage_config),
        std::make_unique<MockModelManager>(),
        std::make_unique<ConfigHolder>(std::move(configs_)),
        &ukm_data_manager_);

    cached_result_provider_ = std::make_unique<CachedResultProvider>(
        result_prefs_.get(), storage_service_->config_holder()->configs());

    collector_ = std::make_unique<TrainingDataCollectorImpl>(
        PlatformOptions::CreateDefault(), &feature_list_processor_,
        &histogram_signal_handler_, &user_action_signal_handler_,
        storage_service_.get(), &prefs_, &clock_,
        cached_result_provider_.get());

    collector_->SetSamplingRateForTesting(1);
  }

 protected:
  TrainingDataCollectorImpl* collector() { return collector_.get(); }
  test::TestSegmentInfoDatabase* test_segment_db() {
    return test_segment_info_db_;
  }
  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  base::SimpleTestClock* clock() { return &clock_; }
  MockSignalStorageConfig* signal_storage_config() {
    return signal_storage_config_;
  }
  processing::MockFeatureListQueryProcessor* feature_list_processor() {
    return &feature_list_processor_;
  }

  void RefreshCollector() {
    collector_.reset();
    collector_ = std::make_unique<TrainingDataCollectorImpl>(
        PlatformOptions::CreateDefault(), &feature_list_processor_,
        &histogram_signal_handler_, &user_action_signal_handler_,
        storage_service_.get(), &prefs_, &clock_,
        cached_result_provider_.get());
  }

  proto::SegmentInfo* CreateSegmentInfo(
      SegmentId segment_id,
      DecisionType type,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE) {
    test_segment_db()->AddUserActionFeature(
        segment_id, "action", 1, 1, proto::Aggregation::COUNT, model_source);
    // Segment 0 contains 1 immediate collection uma output for
    // |kHistogramName0|, 1 uma output collection with delay for
    // |kHistogramName1|.
    auto* segment_info = CreateSegment(segment_id, model_source);

    auto* trigger = segment_info->mutable_model_metadata()
                        ->mutable_training_outputs()
                        ->mutable_trigger_config();
    trigger->set_decision_type(type);

    if (type == kOnDemandDecisionType) {
      // Add a uma feature trigger based on |kHistogramName0| if trigger type is
      // ONDEMAND.
      auto* uma_trigger = trigger->add_observation_trigger();
      auto* uma_feature =
          uma_trigger->mutable_uma_trigger()->mutable_uma_feature();
      uma_feature->set_name(kHistogramName0);
      uma_feature->set_name_hash(base::HashMetricName(kHistogramName0));
      uma_feature->set_type(proto::SignalType::HISTOGRAM_ENUM);
      uma_feature->add_enum_ids(kSample);
    } else if (type == kPeriodicDecisionType) {
      // Add a uma feature output based on |kHistogramName0| if trigger type is
      // PERIODIC.
      AddOutput(segment_info, kHistogramName0);
    }
    return segment_info;
  }

  void AddUserActionTrigger(proto::SegmentInfo* segment_info,
                            std::string name) {
    auto* trigger = segment_info->mutable_model_metadata()
                        ->mutable_training_outputs()
                        ->mutable_trigger_config();
    auto* uma_trigger = trigger->add_observation_trigger();
    auto* uma_feature =
        uma_trigger->mutable_uma_trigger()->mutable_uma_feature();
    uma_feature->set_name(name);
    uma_feature->set_name_hash(base::HashMetricName(name));
    uma_feature->set_type(proto::SignalType::USER_ACTION);
  }

  void AddTimeTrigger(proto::SegmentInfo* segment_info, base::TimeDelta delay) {
    // Add a time delay trigger.
    auto* trigger = segment_info->mutable_model_metadata()
                        ->mutable_training_outputs()
                        ->mutable_trigger_config();

    auto* delay_trigger = trigger->add_observation_trigger();
    delay_trigger->set_delay_sec(delay.InSeconds());
  }

  proto::SegmentInfo* CreateSegment(
      SegmentId segment_id,
      ModelSource model_source = proto::ModelSource::SERVER_MODEL_SOURCE) {
    auto* segment_info =
        test_segment_db()->FindOrCreateSegment(segment_id, model_source);
    auto* model_metadata = segment_info->mutable_model_metadata();
    model_metadata->set_upload_tensors(true);
    model_metadata->set_time_unit(proto::TimeUnit::DAY);
    model_metadata->set_signal_storage_length(7);
    segment_info->set_model_version(kModelVersion);
    auto model_update_time = clock()->Now() - base::Days(365);
    segment_info->set_model_update_time_s(
        model_update_time.ToDeltaSinceWindowsEpoch().InSeconds());
    auto* prediction_result = segment_info->mutable_prediction_result();
    prediction_result->add_result(0.6);
    return segment_info;
  }

  proto::TrainingOutput* AddOutput(proto::SegmentInfo* segment_info,
                                   const std::string& histgram_name) {
    auto* output = segment_info->mutable_model_metadata()
                       ->mutable_training_outputs()
                       ->add_outputs();
    auto* uma_feature = output->mutable_uma_output()->mutable_uma_feature();
    uma_feature->set_name(histgram_name);
    uma_feature->set_name_hash(base::HashMetricName(histgram_name));
    uma_feature->set_tensor_length(1);

    return output;
  }

  void SetupFeatureProcessorResult(proto::SegmentId segment_id,
                                   base::Time prediction,
                                   std::optional<base::Time> observation,
                                   bool skip_input_processing = false) {
    if (!skip_input_processing) {
      EXPECT_CALL(
          *feature_list_processor(),
          ProcessFeatureList(
              _, _, segment_id, prediction, base::Time(),
              processing::FeatureListQueryProcessor::ProcessOption::kInputsOnly,
              _))
          .WillOnce(RunOnceCallback<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));
    }
    if (observation) {
      EXPECT_CALL(*feature_list_processor(),
                  ProcessFeatureList(_, _, segment_id, prediction, *observation,
                                     processing::FeatureListQueryProcessor::
                                         ProcessOption::kOutputsOnly,
                                     _))
          .WillOnce(RunOnceCallback<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));
    }
  }

  void ExpectResult1Ukm() {
    ExpectUkm({Segmentation_ModelExecution::kOptimizationTargetName,
               Segmentation_ModelExecution::kModelVersionName,
               Segmentation_ModelExecution::kInput0Name,
               Segmentation_ModelExecution::kActualResultName,
               Segmentation_ModelExecution::kActualResult2Name},
              {kTestOptimizationTarget0, kModelVersion,
               SegmentationUkmHelper::FloatToInt64(1.f),
               SegmentationUkmHelper::FloatToInt64(2.f),
               SegmentationUkmHelper::FloatToInt64(3.f)});
  }

  void ExpectResult1UkmWithSample(int sample) {
    ExpectUkm({Segmentation_ModelExecution::kOptimizationTargetName,
               Segmentation_ModelExecution::kModelVersionName,
               Segmentation_ModelExecution::kInput0Name,
               Segmentation_ModelExecution::kActualResultName,
               Segmentation_ModelExecution::kActualResult2Name,
               Segmentation_ModelExecution::kActualResult3Name},
              {kTestOptimizationTarget0, kModelVersion,
               SegmentationUkmHelper::FloatToInt64(1.f),
               SegmentationUkmHelper::FloatToInt64(2.f),
               SegmentationUkmHelper::FloatToInt64(3.f),
               SegmentationUkmHelper::FloatToInt64(sample)});
  }

  // TODO(xingliu): Share this test code with SegmentationUkmHelperTest, or test
  // with mock SegmentationUkmHelperTest.
  void ExpectUkm(std::vector<std::string_view> metric_names,
                 std::vector<int64_t> expected_values) {
    const auto& entries = test_recorder_.GetEntriesByName(
        Segmentation_ModelExecution::kEntryName);
    ASSERT_EQ(1u, entries.size());
    for (size_t i = 0; i < metric_names.size(); ++i) {
      test_recorder_.ExpectEntryMetric(entries[0], metric_names[i],
                                       expected_values[i]);
    }
  }

  void ExpectUkmCount(size_t count) {
    const auto& entries = test_recorder_.GetEntriesByName(
        Segmentation_ModelExecution::kEntryName);
    ASSERT_EQ(count, entries.size());
  }

  void Init() {
    collector()->OnServiceInitialized();
    task_environment()->RunUntilIdle();
  }

  void WaitForHistogramSignalUpdated(const std::string& histogram_name,
                                     base::HistogramBase::Sample sample) {
    base::RunLoop run_loop;
    test_recorder_.SetOnAddEntryCallback(
        Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
    collector_->OnHistogramSignalUpdated(histogram_name, sample);
    run_loop.Run();
  }

  void WaitForUserActionSignalUpdated(const std::string& user_action_name,
                                      base::TimeTicks action_time) {
    base::RunLoop run_loop;
    test_recorder_.SetOnAddEntryCallback(
        Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
    collector_->OnUserAction(user_action_name, action_time);
    run_loop.Run();
  }

  void WaitForContinuousCollection() {
    base::RunLoop run_loop;
    test_recorder_.SetOnAddEntryCallback(
        Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
    collector_->ReportCollectedContinuousTrainingData();
    run_loop.Run();
  }

  ukm::TestAutoSetUkmRecorder* test_recorder() { return &test_recorder_; }

  ModelSource GetModelSource(bool is_default_model) {
    return is_default_model ? ModelSource::DEFAULT_MODEL_SOURCE
                            : ModelSource::SERVER_MODEL_SOURCE;
  }

 private:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  NiceMock<processing::MockFeatureListQueryProcessor> feature_list_processor_;
  NiceMock<MockHistogramSignalHandler> histogram_signal_handler_;
  NiceMock<MockUserActionSignalHandler> user_action_signal_handler_;
  raw_ptr<NiceMock<MockSignalStorageConfig>, DanglingUntriaged>
      signal_storage_config_;
  raw_ptr<test::TestSegmentInfoDatabase, DanglingUntriaged>
      test_segment_info_db_;
  std::unique_ptr<TrainingDataCollectorImpl> collector_;
  TestingPrefServiceSimple prefs_;
  std::vector<std::unique_ptr<Config>> configs_;
  NiceMock<MockUkmDataManager> ukm_data_manager_;
  std::unique_ptr<StorageService> storage_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
  std::unique_ptr<CachedResultProvider> cached_result_provider_;
};

INSTANTIATE_TEST_SUITE_P(IsDefaultModel,
                         TrainingDataCollectorImplTest,
                         ::testing::Bool());

// No segment info in database. Do nothing.
TEST_P(TrainingDataCollectorImplTest, NoSegment) {
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Histogram not in the output list will not trigger a training data report..
TEST_P(TrainingDataCollectorImplTest, IrrelevantHistogramNotReported) {
  ModelSource model_source = GetModelSource(GetParam());
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    model_source);
  Init();
  collector()->OnHistogramSignalUpdated("irrelevant_histogram", kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Continuous collection histogram |kHistogramName1| should not be reported.
  collector()->OnHistogramSignalUpdated(kHistogramName1, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No UKM report due to minimum data collection time not met.
TEST_P(TrainingDataCollectorImplTest, SignalCollectionRequirementNotMet) {
  ModelSource model_source = GetModelSource(GetParam());
  EXPECT_CALL(*signal_storage_config(), MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(false));

  CreateSegmentInfo(kTestOptimizationTarget0, kPeriodicDecisionType,
                    model_source);
  clock()->Advance(base::Hours(24));
  Init();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No UKM report due to model updated recently.
TEST_P(TrainingDataCollectorImplTest, ModelUpdatedRecently) {
  ModelSource model_source = GetModelSource(GetParam());
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kPeriodicDecisionType, model_source);
  base::TimeDelta min_signal_collection_length =
      segment_info->model_metadata().min_signal_collection_length() *
      metadata_utils::GetTimeUnit(segment_info->model_metadata());
  // Set the model update timestamp to be closer to Now().
  segment_info->set_model_update_time_s(
      (clock()->Now() - min_signal_collection_length + base::Seconds(30))
          .ToDeltaSinceWindowsEpoch()
          .InSeconds());

  Init();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No report if UKM is enabled recently.
TEST_P(TrainingDataCollectorImplTest, PartialOutputNotAllowed) {
  ModelSource model_source = GetModelSource(GetParam());
  // Simulate that UKM is allowed 300 seconds ago.
  LocalStateHelper::GetInstance().SetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey,
      clock()->Now() - base::Seconds(300));
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    model_source);
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No training data recorded on startup if upload_tensor is set to false in
// continuous collection.
TEST_P(TrainingDataCollectorImplTest,
       ContinuousCollectionOnStartupWithoutUploadTensor) {
  ModelSource model_source = GetModelSource(GetParam());

  // Create segment info.
  test_segment_db()->AddUserActionFeature(kTestOptimizationTarget1, "action", 1,
                                          1, proto::Aggregation::COUNT,
                                          model_source);

  auto* segment_info = CreateSegment(kTestOptimizationTarget1, model_source);
  segment_info->mutable_model_metadata()->set_upload_tensors(false);
  segment_info->mutable_model_metadata()
      ->mutable_training_outputs()
      ->mutable_trigger_config()
      ->set_decision_type(kPeriodicDecisionType);

  // Add a uma feature output based on |kHistogramName0|.
  AddOutput(segment_info, kHistogramName0);

  clock()->Advance(base::Days(1));
  Init();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Tests that continuous collection happens on startup.
TEST_P(TrainingDataCollectorImplTest, ContinuousCollectionOnStartupNoDelay) {
  ModelSource model_source = GetModelSource(GetParam());
  CreateSegmentInfo(kTestOptimizationTarget0, kPeriodicDecisionType,
                    model_source);
  clock()->Advance(base::Days(1));

  base::Time current = clock()->Now();
  SetupFeatureProcessorResult(kTestOptimizationTarget0, current, base::Time());

  Init();
  task_environment()->RunUntilIdle();
  ExpectResult1Ukm();
}

// Tests that continuous collection do not collect for ondemand models on
// startup.
TEST_P(TrainingDataCollectorImplTest,
       OnDemandModelsDoNotTriggerPeriodicCollection) {
  ModelSource model_source = GetModelSource(GetParam());
  AddTimeTrigger(CreateSegmentInfo(kTestOptimizationTarget0,
                                   kOnDemandDecisionType, model_source),
                 base::Seconds(10));

  clock()->Advance(base::Days(1));
  collector()->OnDecisionTime(kTestOptimizationTarget0, nullptr,
                              kPeriodicDecisionType, std::nullopt);
  Init();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Tests that ReportCollectedContinuousTrainingData() works well later if
// no data is reported on start up.
TEST_P(TrainingDataCollectorImplTest,
       ReportCollectedContinuousTrainingData_LegacyConfig) {
  ModelSource model_source = GetModelSource(GetParam());
  base::Time prediction_time = clock()->Now() + base::Days(1);
  SetupFeatureProcessorResult(kTestOptimizationTarget0, prediction_time,
                              base::Time());
  CreateSegmentInfo(kTestOptimizationTarget0, kPeriodicDecisionType,
                    model_source);
  Init();
  clock()->Advance(base::Days(1));
  WaitForContinuousCollection();
  ExpectUkm(
      {Segmentation_ModelExecution::kOptimizationTargetName,
       Segmentation_ModelExecution::kModelVersionName,
       Segmentation_ModelExecution::kInput0Name,
       Segmentation_ModelExecution::kSelectionResultName,
       Segmentation_ModelExecution::kOutputDelaySecName,
       Segmentation_ModelExecution::kActualResultName,
       Segmentation_ModelExecution::kActualResult2Name},
      {kTestOptimizationTarget0, kModelVersion,
       SegmentationUkmHelper::FloatToInt64(1.f), kTestOptimizationTarget1,
       base::Days(1).InSeconds(), SegmentationUkmHelper::FloatToInt64(2.f),
       SegmentationUkmHelper::FloatToInt64(3.f)});
}

// Tests that ReportCollectedContinuousTrainingData() works well later if
// no data is reported on start up.
TEST_P(TrainingDataCollectorImplTest,
       ReportCollectedContinuousTrainingData_MultiOutputConfig) {
  ModelSource model_source = GetModelSource(GetParam());
  base::Time prediction_time = clock()->Now() + base::Days(1);
  SetupFeatureProcessorResult(kTestOptimizationTarget2, prediction_time,
                              base::Time());
  CreateSegmentInfo(kTestOptimizationTarget2, kPeriodicDecisionType,
                    model_source);
  Init();
  clock()->Advance(base::Days(1));
  WaitForContinuousCollection();
  // |kTestOptimizationTarget2| uses a multi-class classifier, so all the result
  // scores are recorded.
  ExpectUkm(
      {Segmentation_ModelExecution::kOptimizationTargetName,
       Segmentation_ModelExecution::kModelVersionName,
       Segmentation_ModelExecution::kInput0Name,
       Segmentation_ModelExecution::kPredictionResult1Name,
       Segmentation_ModelExecution::kPredictionResult2Name,
       Segmentation_ModelExecution::kPredictionResult3Name,
       Segmentation_ModelExecution::kPredictionResult4Name,
       Segmentation_ModelExecution::kPredictionResult5Name,
       Segmentation_ModelExecution::kOutputDelaySecName,
       Segmentation_ModelExecution::kActualResultName,
       Segmentation_ModelExecution::kActualResult2Name},
      {kTestOptimizationTarget2, kModelVersion,
       SegmentationUkmHelper::FloatToInt64(1.f),
       SegmentationUkmHelper::FloatToInt64(0.f),
       SegmentationUkmHelper::FloatToInt64(0.f),
       SegmentationUkmHelper::FloatToInt64(1.f),
       SegmentationUkmHelper::FloatToInt64(0.f),
       SegmentationUkmHelper::FloatToInt64(0.f), base::Days(3).InSeconds(),
       SegmentationUkmHelper::FloatToInt64(2.f),
       SegmentationUkmHelper::FloatToInt64(3.f)});
}

TEST_P(TrainingDataCollectorImplTest, ContinuousWithExactPredictionNotSet) {
  ModelSource model_source = GetModelSource(GetParam());
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kPeriodicDecisionType, model_source);

  AddTimeTrigger(segment_info, base::Days(7));
  const base::TimeDelta kNextUserSession = base::Days(10);

  Init();
  collector()->OnDecisionTime(kTestOptimizationTarget0, nullptr,
                              kPeriodicDecisionType, std::nullopt,
                              /*decision_result_update_trigger=*/true);
  task_environment()->RunUntilIdle();
  clock()->Advance(kNextUserSession);
  ExpectUkmCount(0);
}

TEST_P(TrainingDataCollectorImplTest, ContinuousWithExactPrediction) {
  ModelSource model_source = GetModelSource(GetParam());
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kPeriodicDecisionType, model_source);
  segment_info->mutable_model_metadata()
      ->mutable_training_outputs()
      ->mutable_trigger_config()
      ->set_use_exact_prediction_time(true);
  AddTimeTrigger(segment_info, base::Days(7));
  const base::TimeDelta kNextUserSession = base::Days(10);

  base::Time current = clock()->Now();
  SetupFeatureProcessorResult(kTestOptimizationTarget0, current,
                              current + base::Days(7));

  Init();
  collector()->OnDecisionTime(kTestOptimizationTarget0, nullptr,
                              kPeriodicDecisionType, std::nullopt,
                              /*decision_result_update_trigger=*/true);
  task_environment()->RunUntilIdle();
  clock()->Advance(kNextUserSession);
  WaitForContinuousCollection();
  ExpectResult1Ukm();
}

TEST_P(TrainingDataCollectorImplTest, ContinuousWithFlexibleObservation) {
  ModelSource model_source = GetModelSource(GetParam());

  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kPeriodicDecisionType, model_source);
  segment_info->mutable_model_metadata()
      ->mutable_training_outputs()
      ->mutable_trigger_config()
      ->set_use_exact_prediction_time(true);
  segment_info->mutable_model_metadata()
      ->mutable_training_outputs()
      ->mutable_trigger_config()
      ->set_use_flexible_observation_time(true);
  AddTimeTrigger(segment_info, base::Days(7));
  const base::TimeDelta kNextUserSession = base::Days(10);

  base::Time current = clock()->Now();
  SetupFeatureProcessorResult(kTestOptimizationTarget0, current,
                              current + kNextUserSession);

  Init();
  collector()->OnDecisionTime(kTestOptimizationTarget0, nullptr,
                              kPeriodicDecisionType, std::nullopt,
                              /*decision_result_update_trigger=*/true);
  task_environment()->RunUntilIdle();
  clock()->Advance(kNextUserSession);
  WaitForContinuousCollection();
  ExpectResult1Ukm();
}

TEST_P(TrainingDataCollectorImplTest, ContinuousWithDelay) {
  ModelSource model_source = GetModelSource(GetParam());
  clock()->Advance(base::Days(10));
  const base::TimeDelta kDelay = base::Days(7);
  const base::TimeDelta kNextUserSession = base::Days(10);
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kPeriodicDecisionType, model_source);
  AddTimeTrigger(segment_info, kDelay);

  base::Time current = clock()->Now();
  base::Time next_session = current + kNextUserSession;

  SetupFeatureProcessorResult(kTestOptimizationTarget0, current - base::Days(7),
                              current);
  SetupFeatureProcessorResult(kTestOptimizationTarget0,
                              next_session - base::Days(7), next_session);

  Init();
  task_environment()->RunUntilIdle();
  ExpectResult1Ukm();
  clock()->Advance(kNextUserSession);
  WaitForContinuousCollection();
  ExpectUkmCount(2u);
}

// Tests that after a data collection, another data collection won't happen
// immediately afterwards.
TEST_P(TrainingDataCollectorImplTest,
       NoImmediateDataCollectionAfterLastCollection) {
  ModelSource model_source = GetModelSource(GetParam());
  EXPECT_CALL(*feature_list_processor(),
              ProcessFeatureList(_, _, _, _, _, _, _))
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));
  CreateSegmentInfo(kTestOptimizationTarget0, kPeriodicDecisionType,
                    model_source);
  Init();
  clock()->Advance(base::Hours(24));
  WaitForContinuousCollection();
  test_recorder()->Purge();
  ExpectUkmCount(0u);

  // Nothing should be collected if collection just happen.
  collector()->ReportCollectedContinuousTrainingData();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Collect again after 24 hours and it should work.
  clock()->Advance(base::Hours(24));
  WaitForContinuousCollection();
  ExpectUkmCount(1u);
}

// Tests that if UKM allowed timestamp is not set in local state, data
// collection won't happen.
TEST_P(TrainingDataCollectorImplTest, NoDataCollectionIfUkmAllowedPrefNotSet) {
  ModelSource model_source = GetModelSource(GetParam());
  EXPECT_CALL(*feature_list_processor(),
              ProcessFeatureList(_, _, _, _, _, _, _))
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));
  LocalStateHelper::GetInstance().SetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey, base::Time());
  CreateSegmentInfo(kTestOptimizationTarget0, kPeriodicDecisionType,
                    model_source);
  Init();
  collector()->ReportCollectedContinuousTrainingData();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Tests that if uma histogram trigger is set, collection will happen when the
// trigger histogram is observed.
TEST_P(TrainingDataCollectorImplTest, DataCollectionWithEnumHistogramTrigger) {
  ModelSource model_source = GetModelSource(GetParam());
  constexpr base::TimeDelta kTriggerDuration = base::Seconds(10);
  base::Time current = clock()->Now();
  SetupFeatureProcessorResult(kTestOptimizationTarget0, current,
                              current + kTriggerDuration);

  // Create a segment that contain a uma trigger.
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    model_source);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                              kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  clock()->Advance(kTriggerDuration);
  ExpectUkmCount(0u);

  // Expect to not trigger output collection if histogram is hit with an
  // unlisted enum id.
  collector()->OnHistogramSignalUpdated(kHistogramName0, 0);
  ExpectUkmCount(0u);

  // Trigger output collection and ukm data recording.
  WaitForHistogramSignalUpdated(kHistogramName0, kSample);
  ExpectResult1UkmWithSample(kSample);
}

// Tests that if uma user action trigger is set, collection will happen when the
// trigger user action is observed.
TEST_P(TrainingDataCollectorImplTest, DataCollectionWithUserActionTrigger) {
  ModelSource model_source = GetModelSource(GetParam());
  constexpr base::TimeDelta kTriggerDuration = base::Seconds(10);
  base::Time current = clock()->Now();
  SetupFeatureProcessorResult(kTestOptimizationTarget0, current,
                              current + kTriggerDuration,
                              /*skip_input_processing=*/true);

  // Create a segment that contain a uma trigger.
  AddUserActionTrigger(CreateSegmentInfo(kTestOptimizationTarget0,
                                         kOnDemandDecisionType, model_source),
                       kHistogramName1);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                              kOnDemandDecisionType,
                              ModelProvider::Request{1.f});
  task_environment()->RunUntilIdle();
  clock()->Advance(kTriggerDuration);
  ExpectUkmCount(0u);

  // Trigger output collection and ukm data recording.
  WaitForUserActionSignalUpdated(kHistogramName1, base::TimeTicks());
  ExpectResult1Ukm();
}

// Tests that if uma user action trigger is set, collection will not happen
// without upload_tensor = true.
TEST_P(TrainingDataCollectorImplTest,
       DataCollectionWithTriggerWithoutUploadTensor) {
  ModelSource model_source = GetModelSource(GetParam());

  // Create a segment that contain a uma trigger.
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget1,
                                         kOnDemandDecisionType, model_source);
  segment_info->mutable_model_metadata()->set_upload_tensors(false);
  AddUserActionTrigger(segment_info, kHistogramName1);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  collector()->OnDecisionTime(kTestOptimizationTarget1, input_context,
                              kOnDemandDecisionType,
                              ModelProvider::Request{1.f});
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Trigger output collection and check that no ukm was recorded.
  collector()->OnUserAction(kHistogramName1, base::TimeTicks());
  ExpectUkmCount(0u);
}

// A histogram interested by multiple model will trigger multiple UKM reports.
TEST_P(TrainingDataCollectorImplTest,
       DataCollectionWithUMATrigger_MultipleModels) {
  ModelSource model_source = GetModelSource(GetParam());
  EXPECT_CALL(*feature_list_processor(),
              ProcessFeatureList(_, _, _, _, _, _, _))
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));

  // Create a segment that contain a uma trigger.
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    model_source);

  // Create a second segment that contain the same uma trigger.
  test_segment_db()->AddUserActionFeature(kTestOptimizationTarget1, "action", 1,
                                          1, proto::Aggregation::COUNT,
                                          model_source);
  auto* segment_info = CreateSegment(kTestOptimizationTarget1, model_source);

  auto* trigger = segment_info->mutable_model_metadata()
                      ->mutable_training_outputs()
                      ->mutable_trigger_config();
  trigger->set_decision_type(kOnDemandDecisionType);
  auto* uma_trigger = trigger->add_observation_trigger();
  auto* uma_feature = uma_trigger->mutable_uma_trigger()->mutable_uma_feature();
  uma_feature->set_name(kHistogramName0);
  uma_feature->set_name_hash(base::HashMetricName(kHistogramName0));
  uma_feature->set_type(proto::SignalType::HISTOGRAM_VALUE);

  // Wait for input collection to be done and cached in memory.
  Init();
  auto input_context = base::MakeRefCounted<InputContext>();
  collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                              kOnDemandDecisionType, std::nullopt);
  collector()->OnDecisionTime(kTestOptimizationTarget1, input_context,
                              kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Trigger output collection and ukm data recording.
  WaitForHistogramSignalUpdated(kHistogramName0, kSample);
  ExpectUkmCount(2u);
}

// Tests that if no uma histogram trigger is set, collection will happen when
// the time delay passes.
TEST_P(TrainingDataCollectorImplTest, DataCollectionWithTimeTrigger) {
  ModelSource model_source = GetModelSource(GetParam());
  EXPECT_CALL(*feature_list_processor(),
              ProcessFeatureList(_, _, _, _, _, _, _))
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));

  // Create a segment that contain a time delay trigger and a uma trigger.
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kOnDemandDecisionType, model_source);
  AddTimeTrigger(segment_info, base::Seconds(10));
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  base::RunLoop run_loop;
  test_recorder()->SetOnAddEntryCallback(
      Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
  collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                              kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Trigger output collection and ukm data recording.
  run_loop.Run();
  ExpectUkmCount(1u);
  ExpectResult1Ukm();
}

TEST_P(TrainingDataCollectorImplTest, DataCollectionWithStoreToDisk) {
  ModelSource model_source = GetModelSource(GetParam());
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget0,
                                         kPeriodicDecisionType, model_source);
  segment_info->mutable_model_metadata()
      ->mutable_training_outputs()
      ->mutable_trigger_config()
      ->set_use_exact_prediction_time(true);
  AddTimeTrigger(segment_info, base::Days(7));
  const base::TimeDelta kNextUserSession = base::Days(10);

  base::Time current = clock()->Now();
  SetupFeatureProcessorResult(kTestOptimizationTarget0, current,
                              current + base::Days(7));

  // Trigger decision time with the collector and wait for the database to store
  // the training data.
  Init();
  collector()->OnDecisionTime(kTestOptimizationTarget0, nullptr,
                              kPeriodicDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0);
  clock()->Advance(kNextUserSession);

  // Delete and create a new collector to ensure data is stored to disk.
  RefreshCollector();

  // At startup the new collector should fetch the training data, finish the
  // training request, trigger observation and record the ukm.
  Init();
  ExpectResult1Ukm();
}

TEST_P(TrainingDataCollectorImplTest, DataCollectionWithTriggerAPI) {
  ModelSource model_source = GetModelSource(GetParam());
  EXPECT_CALL(*feature_list_processor(),
              ProcessFeatureList(_, _, _, _, _, _, _))
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));

  // Create a segment.
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    model_source);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  base::RunLoop run_loop;
  test_recorder()->SetOnAddEntryCallback(
      Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
  auto request_id =
      collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                                  kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  TrainingLabels label;
  label.output_metric = {{kHistogramName0, kSample}};
  // Trigger output collection and ukm data recording.
  collector()->CollectTrainingData(kTestOptimizationTarget0, request_id,
                                   ukm::kInvalidSourceId, label,
                                   base::DoNothing());
  run_loop.Run();
  ExpectUkmCount(1u);
  ExpectResult1UkmWithSample(kSample);
}

// No training data recorded if upload_tensor is set to false in on-demand
// collection using trigger API.
TEST_P(TrainingDataCollectorImplTest,
       DataCollectionTriggerAPIWithoutUploadTensor) {
  ModelSource model_source = GetModelSource(GetParam());
  base::HistogramTester tester;

  // Create segment info.
  auto* segment_info = CreateSegmentInfo(kTestOptimizationTarget1,
                                         kOnDemandDecisionType, model_source);
  segment_info->mutable_model_metadata()->set_upload_tensors(false);

  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  auto request_id =
      collector()->OnDecisionTime(kTestOptimizationTarget1, input_context,
                                  kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  TrainingLabels label;
  label.output_metric = {{kHistogramName0, kSample}};
  // Trigger output collection and ukm data recording.
  collector()->CollectTrainingData(kTestOptimizationTarget1, request_id,
                                   ukm::kInvalidSourceId, label,
                                   base::DoNothing());

  // No histogram recorded for data collection.
  EXPECT_EQ(0,
            tester.GetBucketCount(
                "SegmentationPlatform.TrainingDataCollectionEvents.SearchUser",
                stats::TrainingDataCollectionEvent::kImmediateCollectionStart));
  EXPECT_EQ(0,
            tester.GetBucketCount(
                "SegmentationPlatform.TrainingDataCollectionEvents.SearchUser",
                stats::TrainingDataCollectionEvent::kObservationTimeReached));
  ExpectUkmCount(0);
}

TEST_P(TrainingDataCollectorImplTest,
       DataCollectionWithTriggerAPIForPreferredSegment) {
  EXPECT_CALL(*feature_list_processor(),
              ProcessFeatureList(_, _, _, _, _, _, _))
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<6>(false, ModelProvider::Request{1.f},
                                       ModelProvider::Response{2.f, 3.f}));

  // Create a segment.
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType);
  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    proto::ModelSource::DEFAULT_MODEL_SOURCE);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  base::RunLoop run_loop;
  test_recorder()->SetOnAddEntryCallback(
      Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
  auto request_id =
      collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                                  kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  TrainingLabels label;
  label.output_metric = {{kHistogramName0, kSample}};
  // Trigger output collection and ukm data recording.
  collector()->CollectTrainingData(kTestOptimizationTarget0, request_id,
                                   ukm::kInvalidSourceId, label,
                                   base::DoNothing());
  run_loop.Run();
  ExpectUkmCount(1u);
  ExpectResult1UkmWithSample(kSample);
}

// Tests that we don't collect training data if input processing fails.
TEST_P(TrainingDataCollectorImplTest,
       DataCollectionSkippedWhenInputProcessingFails) {
  ModelSource model_source = GetModelSource(GetParam());
  base::HistogramTester tester;

  // Set feature_list_processor to return an error when processing input data.
  EXPECT_CALL(
      *feature_list_processor(),
      ProcessFeatureList(
          _, _, _, _, _,
          processing::FeatureListQueryProcessor::ProcessOption::kInputsOnly, _))
      .WillOnce(RunOnceCallback<6>(/* has_error= */ true,
                                   ModelProvider::Request{},
                                   ModelProvider::Response{}));

  CreateSegmentInfo(kTestOptimizationTarget0, kOnDemandDecisionType,
                    model_source);
  Init();

  auto input_context = base::MakeRefCounted<InputContext>();
  auto request_id =
      collector()->OnDecisionTime(kTestOptimizationTarget0, input_context,
                                  kOnDemandDecisionType, std::nullopt);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  TrainingLabels label;
  label.output_metric = {{kHistogramName0, kSample}};
  // Trigger output collection and ukm data recording.
  collector()->CollectTrainingData(kTestOptimizationTarget0, request_id,
                                   ukm::kInvalidSourceId, label,
                                   base::DoNothing());
  ExpectUkmCount(0u);
  // A histogram should have been recorded.
  EXPECT_EQ(1, tester.GetBucketCount(
                   "SegmentationPlatform.TrainingDataCollectionEvents.NewTab",
                   stats::TrainingDataCollectionEvent::kGetInputTensorsFailed));
}

}  // namespace
}  // namespace segmentation_platform
