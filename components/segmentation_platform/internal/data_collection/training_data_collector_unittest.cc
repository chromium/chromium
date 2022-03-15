// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include <map>

#include "base/metrics/metrics_hashes.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/segmentation_ukm_helper.h"
#include "components/segmentation_platform/internal/signals/mock_histogram_signal_handler.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using Segmentation_ModelExecution =
    ::ukm::builders::Segmentation_ModelExecution;

constexpr auto kTestOptimizationTarget0 =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
constexpr auto kTestOptimizationTarget1 =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;
constexpr char kHistogramName0[] = "histogram0";
constexpr char kHistogramName1[] = "histogram1";
constexpr int64_t kModelVersion = 123;
constexpr int kSample = 1;

namespace segmentation_platform {
namespace {

class TrainingDataCollectorTest : public ::testing::Test {
 public:
  TrainingDataCollectorTest() = default;
  ~TrainingDataCollectorTest() override = default;

  void SetUp() override {
    test_recorder_.Purge();

    // Allow two models to collect training data.
    std::map<std::string, std::string> params = {
        {kSegmentIdsAllowedForReportingKey, "4,5"}};
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSegmentationStructuredMetricsFeature, params);

    // Setup behavior for |feature_list_processor_|.
    std::vector<float> inputs({1.f});
    ON_CALL(feature_list_processor_, ProcessFeatureList(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(true, inputs));
    ON_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
        .WillByDefault(Return(true));

    test_segment_info_db_ = std::make_unique<test::TestSegmentInfoDatabase>();
    collector_ = TrainingDataCollector::Create(
        test_segment_info_db_.get(), &feature_list_processor_,
        &histogram_signal_handler_, &signal_storage_config_, &clock_);
  }

 protected:
  TrainingDataCollector* collector() { return collector_.get(); }
  test::TestSegmentInfoDatabase* test_segment_db() {
    return test_segment_info_db_.get();
  }
  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  base::SimpleTestClock* clock() { return &clock_; }
  MockSignalStorageConfig* signal_storage_config() {
    return &signal_storage_config_;
  }

  proto::SegmentInfo* CreateSegmentInfo() {
    test_segment_db()->AddUserActionFeature(kTestOptimizationTarget0, "action",
                                            1, 1, proto::Aggregation::COUNT);
    // Segment 0 contains 1 immediate collection uma output for for
    // |kHistogramName0|, 1 continuous collection output for for
    // |kHistogramName1|.
    auto* segment_info = CreateSegment(kTestOptimizationTarget0);
    AddOutput(segment_info, kHistogramName0);
    proto::TrainingOutput* output1 = AddOutput(segment_info, kHistogramName1);
    output1->mutable_uma_output()->set_duration(1u);
    return segment_info;
  }

  proto::SegmentInfo* CreateSegment(OptimizationTarget optimization_target) {
    auto* segment_info =
        test_segment_db()->FindOrCreateSegment(optimization_target);
    segment_info->mutable_model_metadata()->set_time_unit(proto::TimeUnit::DAY);
    segment_info->set_model_version(kModelVersion);
    auto model_update_time = clock()->Now() - base::Days(365);
    segment_info->set_model_update_time_s(
        model_update_time.ToDeltaSinceWindowsEpoch().InSeconds());
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

 private:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  ukm::TestAutoSetUkmRecorder test_recorder_;
  NiceMock<MockFeatureListQueryProcessor> feature_list_processor_;
  NiceMock<MockHistogramSignalHandler> histogram_signal_handler_;
  NiceMock<MockSignalStorageConfig> signal_storage_config_;
  std::unique_ptr<test::TestSegmentInfoDatabase> test_segment_info_db_;
  std::unique_ptr<TrainingDataCollector> collector_;
};

// No segment info in database. Do nothing.
TEST_F(TrainingDataCollectorTest, NoSegment) {
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// Histogram not in the output list will not trigger a training data report..
TEST_F(TrainingDataCollectorTest, IrrelevantHistogramNotReported) {
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
TEST_F(TrainingDataCollectorTest, HistogramImmediatelyReported) {
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
TEST_F(TrainingDataCollectorTest, HistogramImmediatelyReported_MultipleModel) {
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
TEST_F(TrainingDataCollectorTest, SignalCollectionRequirementNotMet) {
  EXPECT_CALL(*signal_storage_config(), MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(false));

  CreateSegmentInfo();
  Init();
  collector()->OnHistogramSignalUpdated(kHistogramName0, kSample);
  task_environment()->RunUntilIdle();
  ExpectUkmCount(0u);
}

// No UKM report due to model updated recently.
TEST_F(TrainingDataCollectorTest, ModelUpdatedRecently) {
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

}  // namespace
}  // namespace segmentation_platform
