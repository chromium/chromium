// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/mock_histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/mock_user_action_signal_handler.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Contains;
using testing::Invoke;
using testing::IsEmpty;
using testing::SaveArg;

namespace segmentation_platform {
namespace {

constexpr UkmEventHash TestEvent(uint64_t val) {
  return UkmEventHash::FromUnsafeValue(val);
}

constexpr UkmMetricHash TestMetric(uint64_t val) {
  return UkmMetricHash::FromUnsafeValue(val);
}

}  // namespace

class MockHistoryObserver : public HistoryServiceObserver {
 public:
  MOCK_METHOD1(SetHistoryBasedSegments,
               void(base::flat_set<proto::SegmentId> history_based_segments));
};

class SignalFilterProcessorTest : public testing::Test {
 public:
  SignalFilterProcessorTest() = default;
  ~SignalFilterProcessorTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    base::flat_set<SegmentId> segment_ids(
        {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
         SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE});
    user_action_signal_handler_ =
        std::make_unique<MockUserActionSignalHandler>();
    histogram_signal_handler_ = std::make_unique<MockHistogramSignalHandler>();
    history_observer_ = std::make_unique<MockHistoryObserver>();

    auto moved_segment_database =
        std::make_unique<test::TestSegmentInfoDatabase>();
    segment_database_ = moved_segment_database.get();
    auto moved_signal_config = std::make_unique<MockSignalStorageConfig>();
    signal_storage_config_ = moved_signal_config.get();
    ukm_data_manager_ = std::make_unique<MockUkmDataManager>();
    storage_service_ = std::make_unique<StorageService>(
        std::move(moved_segment_database), nullptr,
        std::move(moved_signal_config),
        std::make_unique<ModelManagerImpl>(segment_ids, nullptr, nullptr,
                                           segment_database_,
                                           base::DoNothing()),
        nullptr, ukm_data_manager_.get());

    signal_filter_processor_ = std::make_unique<SignalFilterProcessor>(
        storage_service_.get(), user_action_signal_handler_.get(),
        histogram_signal_handler_.get(), history_observer_.get(), segment_ids);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockUserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<MockHistogramSignalHandler> histogram_signal_handler_;
  std::unique_ptr<MockHistoryObserver> history_observer_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;
  std::unique_ptr<MockUkmDataManager> ukm_data_manager_;
  std::unique_ptr<StorageService> storage_service_;
  raw_ptr<test::TestSegmentInfoDatabase> segment_database_;
  raw_ptr<MockSignalStorageConfig> signal_storage_config_;
};

TEST_F(SignalFilterProcessorTest, UserActionRegistrationFlow) {
  std::string kUserActionName1 = "some_action_1";
  segment_database_->AddUserActionFeature(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, kUserActionName1, 0,
      0, proto::Aggregation::COUNT);
  std::string kUserActionName2 = "some_action_2";
  segment_database_->AddUserActionFeature(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, kUserActionName2, 0, 0,
      proto::Aggregation::COUNT);

  std::set<uint64_t> actions;
  EXPECT_CALL(*user_action_signal_handler_, SetRelevantUserActions(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&actions));
  EXPECT_CALL(*signal_storage_config_, OnSignalCollectionStarted(_)).Times(2);

  signal_filter_processor_->OnSignalListUpdated();
  ASSERT_THAT(actions, Contains(base::HashMetricName(kUserActionName1)));
  ASSERT_THAT(actions, Contains(base::HashMetricName(kUserActionName2)));
}

TEST_F(SignalFilterProcessorTest, HistogramRegistrationFlow) {
  std::string kHistogramName1 = "some_histogram_1";
  segment_database_->AddHistogramValueFeature(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, kHistogramName1, 1,
      1, proto::Aggregation::COUNT);
  std::string kHistogramName2 = "some_histogram_2";
  segment_database_->AddHistogramValueFeature(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, kHistogramName2, 1, 1,
      proto::Aggregation::COUNT);
  std::string kHistogramName3 = "some_histogram_3";
  segment_database_->AddHistogramEnumFeature(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, kHistogramName3, 1, 1,
      proto::Aggregation::COUNT, {3, 4});

  std::set<std::pair<std::string, proto::SignalType>> histograms;
  EXPECT_CALL(*histogram_signal_handler_, SetRelevantHistograms(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&histograms));
  EXPECT_CALL(*signal_storage_config_, OnSignalCollectionStarted(_)).Times(2);

  signal_filter_processor_->OnSignalListUpdated();
  ASSERT_THAT(histograms,
              Contains(std::make_pair(kHistogramName1,
                                      proto::SignalType::HISTOGRAM_VALUE)));
  ASSERT_THAT(histograms,
              Contains(std::make_pair(kHistogramName2,
                                      proto::SignalType::HISTOGRAM_VALUE)));
  ASSERT_THAT(histograms,
              Contains(std::make_pair(kHistogramName3,
                                      proto::SignalType::HISTOGRAM_ENUM)));
}

TEST_F(SignalFilterProcessorTest, UkmMetricsConfig) {
  const SegmentId kSegmentId =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
  const UkmEventHash kEvent1 = TestEvent(10);
  const UkmEventHash kEvent2 = TestEvent(11);
  const std::array<UkmMetricHash, 3> kMetrics1_1{
      TestMetric(100), TestMetric(101), TestMetric(102)};
  const std::array<UkmMetricHash, 3> kMetrics1_2{
      TestMetric(100), TestMetric(104), TestMetric(105)};
  const std::array<UkmMetricHash, 3> kMetrics2{TestMetric(103), TestMetric(104),
                                               TestMetric(105)};
  const std::array<MetadataWriter::SqlFeature::EventAndMetrics, 2> kConfig1{
      MetadataWriter::SqlFeature::EventAndMetrics{
          .event_hash = kEvent1,
          .metrics = kMetrics1_1.data(),
          .metrics_size = kMetrics1_1.size()},
      MetadataWriter::SqlFeature::EventAndMetrics{
          .event_hash = kEvent2,
          .metrics = kMetrics2.data(),
          .metrics_size = kMetrics2.size()}};
  const std::array<MetadataWriter::SqlFeature::EventAndMetrics, 1> kConfig2{
      MetadataWriter::SqlFeature::EventAndMetrics{
          .event_hash = kEvent1,
          .metrics = kMetrics1_2.data(),
          .metrics_size = kMetrics1_2.size()}};
  MetadataWriter::SqlFeature feature1{
      .sql = "", .events = kConfig1.data(), .events_size = kConfig1.size()};
  MetadataWriter::SqlFeature feature2{
      .sql = "", .events = kConfig2.data(), .events_size = kConfig2.size()};

  EXPECT_CALL(*histogram_signal_handler_,
              SetRelevantHistograms(
                  std::set<std::pair<std::string, proto::SignalType>>()))
      .Times(2);
  EXPECT_CALL(*user_action_signal_handler_,
              SetRelevantUserActions(std::set<uint64_t>()))
      .Times(2);

  EXPECT_CALL(*ukm_data_manager_, StartObservingUkm(_))
      .Times(1)
      .WillOnce(Invoke([](const UkmConfig& actual_config) {
        EXPECT_EQ(actual_config, UkmConfig());
      }));
  EXPECT_CALL(*history_observer_, SetHistoryBasedSegments(IsEmpty()));
  signal_filter_processor_->OnSignalListUpdated();

  UkmConfig config1;
  config1.AddEvent(kEvent1, base::flat_set<UkmMetricHash>(kMetrics1_1.begin(),
                                                          kMetrics1_1.end()));
  config1.AddEvent(kEvent2, base::flat_set<UkmMetricHash>(kMetrics2.begin(),
                                                          kMetrics2.end()));
  segment_database_->AddSqlFeature(kSegmentId, feature1);

  UkmConfig config2;
  config2.AddEvent(kEvent1, base::flat_set<UkmMetricHash>(kMetrics1_2.begin(),
                                                          kMetrics1_2.end()));
  segment_database_->AddSqlFeature(kSegmentId, feature2);

  config2.Merge(config1);

  EXPECT_CALL(*ukm_data_manager_, StartObservingUkm(_))
      .Times(1)
      .WillOnce(Invoke([&config2](const UkmConfig& actual_config) {
        EXPECT_EQ(actual_config, config2);
      }));
  EXPECT_CALL(*history_observer_,
              SetHistoryBasedSegments(base::flat_set<SegmentId>({kSegmentId})));
  EXPECT_CALL(*signal_storage_config_, OnSignalCollectionStarted(_));
  signal_filter_processor_->OnSignalListUpdated();
}

TEST_F(SignalFilterProcessorTest, EnableMetrics) {
  EXPECT_CALL(*user_action_signal_handler_, EnableMetrics(true));
  EXPECT_CALL(*histogram_signal_handler_, EnableMetrics(true));
  EXPECT_CALL(*ukm_data_manager_, PauseOrResumeObservation(/*pause=*/false));
  signal_filter_processor_->EnableMetrics(true);
  EXPECT_CALL(*user_action_signal_handler_, EnableMetrics(false));
  EXPECT_CALL(*histogram_signal_handler_, EnableMetrics(false));
  EXPECT_CALL(*ukm_data_manager_, PauseOrResumeObservation(/*pause=*/true));
  signal_filter_processor_->EnableMetrics(false);
}

}  // namespace segmentation_platform
