// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"

#include <map>

#include "base/metrics/metrics_hashes.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/internal/signals/mock_histogram_signal_handler.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using Segmentation_ModelExecution =
    ::ukm::builders::Segmentation_ModelExecution;

constexpr auto kTestOptimizationTarget0 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
constexpr auto kTestOptimizationTarget1 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
constexpr char kHistogramName0[] = "histogram0";
constexpr char kHistogramName1[] = "histogram1";
constexpr char kSegmentationKey[] = "test_key";
constexpr int64_t kModelVersion = 123;
constexpr int kSample = 1;

class TrainingDataCollectorImplTest : public ::testing::Test {
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

    // Allow two models to collect training data.
    std::map<std::string, std::string> params = {
        {kSegmentIdsAllowedForReportingKey, "4,5"}};
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSegmentationStructuredMetricsFeature, params);

    // Setup behavior for |feature_list_processor_|.
    ModelProvider::Request inputs({1.f});
    ON_CALL(feature_list_processor_, ProcessFeatureList(_, _, _, _, _, _))
        .WillByDefault(
            RunOnceCallback<5>(false, inputs, ModelProvider::Response()));

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
        {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
         std::make_unique<Config::SegmentMetadata>("UmaNameNewTab")});
    configs_[0]->segments.insert(
        {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
         std::make_unique<Config::SegmentMetadata>("UmaNameShare")});

    SegmentationResultPrefs result_prefs(&prefs_);
    SelectedSegment selected_segment(
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, 10);
    selected_segment.selection_time = base::Time::Now() - base::Days(1);
    result_prefs.SaveSegmentationResultToPref(kSegmentationKey,
                                              selected_segment);

    storage_service_ = std::make_unique<StorageService>(
        std::move(test_segment_info_db), nullptr,
        std::move(signal_storage_config),
        std::make_unique<DefaultModelManager>(nullptr,
                                              base::flat_set<SegmentId>()),
        &ukm_data_manager_);

    collector_ = std::make_unique<TrainingDataCollectorImpl>(
        &feature_list_processor_, &histogram_signal_handler_,
        storage_service_.get(), &configs_, &prefs_, &clock_);
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

  proto::SegmentInfo* CreateSegmentInfo() {
    test_segment_db()->AddUserActionFeature(kTestOptimizationTarget0, "action",
                                            1, 1, proto::Aggregation::COUNT);
    // Segment 0 contains 1 immediate collection uma output for
    // |kHistogramName0|, 1 uma output collection with delay for
    // |kHistogramName1|.
    auto* segment_info = CreateSegment(kTestOptimizationTarget0);
    AddOutput(segment_info, kHistogramName0);
    proto::TrainingOutput* output1 = AddOutput(segment_info, kHistogramName1);
    output1->mutable_uma_output()->mutable_uma_feature()->set_tensor_length(1);
    return segment_info;
  }

  proto::SegmentInfo* CreateSegmentInfoWithTriggers(int delay_sec) {
    test_segment_db()->AddUserActionFeature(kTestOptimizationTarget0, "action",
                                            1, 1, proto::Aggregation::COUNT);

    auto* segment_info = CreateSegment(kTestOptimizationTarget0);

    // Add triggers.
    auto* trigger = segment_info->mutable_model_metadata()
                        ->mutable_training_outputs()
                        ->mutable_trigger_config();
    trigger->set_decision_type(
        proto::TrainingOutputs_TriggerConfig_DecisionType_ONDEMAND);

    // Add a time delay trigger of 1 second.
    auto* delay_trigger = trigger->add_observation_trigger();
    delay_trigger->set_delay_sec(delay_sec);
    auto* uma_trigger = trigger->add_observation_trigger();

    // Add a uma feature trigger based on |kHistogramName0|.
    auto* uma_feature =
        uma_trigger->mutable_uma_trigger()->mutable_uma_feature();
    uma_feature->set_name(kHistogramName0);
    uma_feature->set_name_hash(base::HashMetricName(kHistogramName0));

    return segment_info;
  }

  proto::SegmentInfo* CreateSegment(SegmentId segment_id) {
    auto* segment_info = test_segment_db()->FindOrCreateSegment(segment_id);
    auto* model_metadata = segment_info->mutable_model_metadata();
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
    return output;
  }

  // TODO(xingliu): Share this test code with SegmentationUkmHelperTest, or test
  // with mock SegmentationUkmHelperTest.
  void ExpectUkm(std::vector<base::StringPiece> metric_names,
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

  void WaitForContinousCollection() {
    base::RunLoop run_loop;
    test_recorder_.SetOnAddEntryCallback(
        Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
    collector_->ReportCollectedContinuousTrainingData();
    run_loop.Run();
  }

  ukm::TestAutoSetUkmRecorder* test_recorder() { return &test_recorder_; }

 private:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  NiceMock<processing::MockFeatureListQueryProcessor> feature_list_processor_;
  NiceMock<MockHistogramSignalHandler> histogram_signal_handler_;
  raw_ptr<NiceMock<MockSignalStorageConfig>> signal_storage_config_;
  test::TestSegmentInfoDatabase* test_segment_info_db_;
  std::unique_ptr<TrainingDataCollectorImpl> collector_;
  TestingPrefServiceSimple prefs_;
  std::vector<std::unique_ptr<Config>> configs_;
  NiceMock<MockUkmDataManager> ukm_data_manager_;
  std::unique_ptr<StorageService> storage_service_;
};

// No segment info in database. Do nothing.
TEST_F(TrainingDataCollectorImplTest, NoSegment) {
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Histogram not in the output list will not trigger a training data report..
TEST_F(TrainingDataCollectorImplTest, IrrelevantHistogramNotReported) {
  CreateSegmentInfo();
  Init();
  collector()->OnHistogramSignalUpdated("irrelevant_histogram", kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Continuous collection histogram |kHistogramName1| should not be reported.
  collector()->OnHistogramSignalUpdated(kHistogramName1, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Immediate training data collection for a certain histogram will be reported
// as a UKM.
TEST_F(TrainingDataCollectorImplTest, HistogramImmediatelyReported) {
  CreateSegmentInfo();
  Init();
  WaitForHistogramSignalUpdated(kHistogramName0, kSample);
  ExpectUkm({Segmentation_ModelExecution::kOptimizationTargetName,
             Segmentation_ModelExecution::kModelVersionName,
             Segmentation_ModelExecution::kActualResultName},
            {kTestOptimizationTarget0, kModelVersion,
             SegmentationUkmHelper::FloatToInt64(kSample)});
}

// A histogram interested by multiple model will trigger multiple UKM reports.
TEST_F(TrainingDataCollectorImplTest,
       HistogramImmediatelyReported_MultipleModel) {
  CreateSegmentInfo();
  // Segment 1 contains 1 immediate collection uma output for for
  // |kHistogramName0|
  auto* segment_info = CreateSegment(kTestOptimizationTarget1);
  AddOutput(segment_info, kHistogramName0);
  Init();
  WaitForHistogramSignalUpdated(kHistogramName0, kSample);
  ExpectUkmCount(2u);
}

// No UKM report due to minimum data collection time not met.
TEST_F(TrainingDataCollectorImplTest, SignalCollectionRequirementNotMet) {
  EXPECT_CALL(*signal_storage_config(), MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(false));

  CreateSegmentInfo();
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No UKM report due to model updated recently.
TEST_F(TrainingDataCollectorImplTest, ModelUpdatedRecently) {
  auto* segment_info = CreateSegmentInfo();
  base::TimeDelta min_signal_collection_length =
      segment_info->model_metadata().min_signal_collection_length() *
      metadata_utils::GetTimeUnit(segment_info->model_metadata());
  // Set the model update timestamp to be closer to Now().
  segment_info->set_model_update_time_s(
      (clock()->Now() - min_signal_collection_length + base::Seconds(30))
          .ToDeltaSinceWindowsEpoch()
          .InSeconds());

  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No report if UKM is enabled recently.
TEST_F(TrainingDataCollectorImplTest, PartialOutputNotAllowed) {
  // Simulate that UKM is allowed 300 seconds ago.
  LocalStateHelper::GetInstance().SetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey,
      clock()->Now() - base::Seconds(300));
  CreateSegmentInfo();
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Tests that continuous collection happens on startup.
TEST_F(TrainingDataCollectorImplTest, ContinousCollectionOnStartup) {
  ON_CALL(*feature_list_processor(), ProcessFeatureList(_, _, _, _, _, _))
      .WillByDefault(RunOnceCallback<5>(false, ModelProvider::Request{1.f},
                                        ModelProvider::Response{2.f, 3.f}));
  CreateSegmentInfo();
  clock()->Advance(base::Hours(24));
  Init();
  task_environment()->RunUntilIdle();
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

// Tests that ReportCollectedContinuousTrainingData() works well later if
// no data is reported on start up.
TEST_F(TrainingDataCollectorImplTest, ReportCollectedContinuousTrainingData) {
  ON_CALL(*feature_list_processor(), ProcessFeatureList(_, _, _, _, _, _))
      .WillByDefault(RunOnceCallback<5>(false, ModelProvider::Request{1.f},
                                        ModelProvider::Response{2.f, 3.f}));
  CreateSegmentInfo();
  Init();
  clock()->Advance(base::Hours(24));
  WaitForContinousCollection();
  ExpectUkm(
      {Segmentation_ModelExecution::kOptimizationTargetName,
       Segmentation_ModelExecution::kModelVersionName,
       Segmentation_ModelExecution::kInput0Name,
       Segmentation_ModelExecution::kPredictionResultName,
       Segmentation_ModelExecution::kSelectionResultName,
       Segmentation_ModelExecution::kOutputDelaySecName,
       Segmentation_ModelExecution::kActualResultName,
       Segmentation_ModelExecution::kActualResult2Name},
      {kTestOptimizationTarget0, kModelVersion,
       SegmentationUkmHelper::FloatToInt64(1.f),
       SegmentationUkmHelper::FloatToInt64(0.6f),
       SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
       base::Days(1).InSeconds(), SegmentationUkmHelper::FloatToInt64(2.f),
       SegmentationUkmHelper::FloatToInt64(3.f)});
}

// Tests that after a data collection, another data collection won't happen
// immediately afterwards.
TEST_F(TrainingDataCollectorImplTest,
       NoImmediateDataCollectionAfterLastCollection) {
  ON_CALL(*feature_list_processor(), ProcessFeatureList(_, _, _, _, _, _))
      .WillByDefault(RunOnceCallback<5>(false, ModelProvider::Request{1.f},
                                        ModelProvider::Response{2.f, 3.f}));
  CreateSegmentInfo();
  Init();
  clock()->Advance(base::Hours(24));
  WaitForContinousCollection();
  test_recorder()->Purge();
  ExpectUkmCount(0u);

  // Nothing should be collected if collection just happen.
  collector()->ReportCollectedContinuousTrainingData();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Collect again after 24 hours and it should work.
  clock()->Advance(base::Hours(24));
  WaitForContinousCollection();
  ExpectUkmCount(1u);
}

// Tests that if UKM allowed timestamp is not set in local state, data
// collection won't happen.
TEST_F(TrainingDataCollectorImplTest, NoDataCollectionIfUkmAllowedPrefNotSet) {
  ON_CALL(*feature_list_processor(), ProcessFeatureList(_, _, _, _, _, _))
      .WillByDefault(RunOnceCallback<5>(false, ModelProvider::Request{1.f},
                                        ModelProvider::Response{2.f, 3.f}));
  LocalStateHelper::GetInstance().SetPrefTime(
      kSegmentationUkmMostRecentAllowedTimeKey, base::Time());
  CreateSegmentInfo();
  Init();
  collector()->ReportCollectedContinuousTrainingData();
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Tests that if uma histogram trigger is set, collection will happen when the
// trigger histogram is observed.
TEST_F(TrainingDataCollectorImplTest, DataCollectionWithUMATrigger) {
  ON_CALL(*feature_list_processor(), ProcessFeatureList(_, _, _, _, _, _))
      .WillByDefault(RunOnceCallback<5>(false, ModelProvider::Request{1.f},
                                        ModelProvider::Response{2.f, 3.f}));

  // Create a segment that contain a time delay trigger and a uma trigger.
  CreateSegmentInfoWithTriggers(10);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  base::RunLoop run_loop;
  test_recorder()->SetOnAddEntryCallback(
      Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
  collector()->OnDecisionTime(
      kTestOptimizationTarget0, input_context,
      proto::TrainingOutputs_TriggerConfig_DecisionType_ONDEMAND);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Trigger output collection and ukm data recording.
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  run_loop.Run();
  ExpectUkmCount(1u);
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

// Tests that if no uma histogram trigger is set, collection will happen when
// the time delay passes.
TEST_F(TrainingDataCollectorImplTest, DataCollectionWithTimeTrigger) {
  ON_CALL(*feature_list_processor(), ProcessFeatureList(_, _, _, _, _, _))
      .WillByDefault(RunOnceCallback<5>(false, ModelProvider::Request{1.f},
                                        ModelProvider::Response{2.f, 3.f}));

  // Create a segment that contain a time delay trigger and a uma trigger.
  CreateSegmentInfoWithTriggers(10);
  Init();

  // Wait for input collection to be done and cached in memory.
  auto input_context = base::MakeRefCounted<InputContext>();
  base::RunLoop run_loop;
  test_recorder()->SetOnAddEntryCallback(
      Segmentation_ModelExecution::kEntryName, run_loop.QuitClosure());
  collector()->OnDecisionTime(
      kTestOptimizationTarget0, input_context,
      proto::TrainingOutputs_TriggerConfig_DecisionType_ONDEMAND);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);

  // Trigger output collection and ukm data recording.
  run_loop.Run();
  ExpectUkmCount(1u);
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

}  // namespace
}  // namespace segmentation_platform
