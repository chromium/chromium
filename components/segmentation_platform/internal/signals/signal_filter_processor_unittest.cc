// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"

#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/mock_histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Contains;
using testing::Invoke;
using testing::IsEmpty;
using testing::SaveArg;

namespace segmentation_platform {
namespace {

UkmEventHash TestEvent(uint64_t val) {
  return UkmEventHash::FromUnsafeValue(val);
}

UkmMetricHash TestMetric(uint64_t val) {
  return UkmMetricHash::FromUnsafeValue(val);
}

}  // namespace
class MockUserActionSignalHandler : public UserActionSignalHandler {
 public:
  MockUserActionSignalHandler() : UserActionSignalHandler(nullptr) {}
  MOCK_METHOD(void, SetRelevantUserActions, (std::set<uint64_t>));
  MOCK_METHOD(void, EnableMetrics, (bool));
};

class MockHistoryObserver : public HistoryServiceObserver {
 public:
  MOCK_METHOD1(
      SetHistoryBasedSegments,
      void(base::flat_set<optimization_guide::proto::OptimizationTarget>&&
               history_based_segments));
};

// Noop version. For database calls, just passes the calls to the DB.
class TestDefaultModelManager : public DefaultModelManager {
 public:
  TestDefaultModelManager()
      : DefaultModelManager(nullptr, std::vector<OptimizationTarget>()) {}
  ~TestDefaultModelManager() override = default;

  void GetAllSegmentInfoFromDefaultModel(
      const std::vector<OptimizationTarget>& segment_ids,
      MultipleSegmentInfoCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  DefaultModelManager::SegmentInfoList()));
  }

  void GetAllSegmentInfoFromBothModels(
      const std::vector<OptimizationTarget>& segment_ids,
      SegmentInfoDatabase* segment_database,
      MultipleSegmentInfoCallback callback) override {
    segment_database->GetSegmentInfoForSegments(
        segment_ids,
        base::BindOnce(
            [](DefaultModelManager::MultipleSegmentInfoCallback callback,
               std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> db_list) {
              DefaultModelManager::SegmentInfoList list;
              for (auto& pair : *db_list) {
                list.push_back(std::make_unique<
                               DefaultModelManager::SegmentInfoWrapper>());
                list.back()->segment_source =
                    DefaultModelManager::SegmentSource::DATABASE;
                list.back()->segment_info.Swap(&pair.second);
              }
              std::move(callback).Run(std::move(list));
            },
            std::move(callback)));
  }
};

class SignalFilterProcessorTest : public testing::Test {
 public:
  SignalFilterProcessorTest() = default;
  ~SignalFilterProcessorTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    std::vector<OptimizationTarget> segment_ids(
        {OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
         OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE});
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
        std::make_unique<TestDefaultModelManager>(), ukm_data_manager_.get());

    signal_filter_processor_ = std::make_unique<SignalFilterProcessor>(
        storage_service_.get(), user_action_signal_handler_.get(),
        histogram_signal_handler_.get(), history_observer_.get(), segment_ids);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockUserActionSignalHandler> user_action_signal_handler_;
  std::unique_ptr<MockHistogramSignalHandler> histogram_signal_handler_;
  std::unique_ptr<SignalFilterProcessor> signal_filter_processor_;
  std::unique_ptr<MockHistoryObserver> history_observer_;
  std::unique_ptr<MockUkmDataManager> ukm_data_manager_;
  std::unique_ptr<StorageService> storage_service_;
  raw_ptr<test::TestSegmentInfoDatabase> segment_database_;
  raw_ptr<MockSignalStorageConfig> signal_storage_config_;
};

TEST_F(SignalFilterProcessorTest, UserActionRegistrationFlow) {
  std::string kUserActionName1 = "some_action_1";
  segment_database_->AddUserActionFeature(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      kUserActionName1, 0, 0, proto::Aggregation::COUNT);
  std::string kUserActionName2 = "some_action_2";
  segment_database_->AddUserActionFeature(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      kUserActionName2, 0, 0, proto::Aggregation::COUNT);

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
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      kHistogramName1, 1, 1, proto::Aggregation::COUNT);
  std::string kHistogramName2 = "some_histogram_2";
  segment_database_->AddHistogramValueFeature(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      kHistogramName2, 1, 1, proto::Aggregation::COUNT);
  std::string kHistogramName3 = "some_histogram_3";
  segment_database_->AddHistogramEnumFeature(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      kHistogramName3, 1, 1, proto::Aggregation::COUNT, {3, 4});

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
  const OptimizationTarget kSegmentId =
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
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
  config1.AddEvent(TestEvent(10),
                   {TestMetric(100), TestMetric(101), TestMetric(102)});
  config1.AddEvent(TestEvent(11),
                   {TestMetric(103), TestMetric(104), TestMetric(105)});
  segment_database_->AddSqlFeature(kSegmentId, "", config1);
  UkmConfig config2;
  config2.AddEvent(TestEvent(10),
                   {TestMetric(100), TestMetric(104), TestMetric(105)});
  segment_database_->AddSqlFeature(kSegmentId, "", config2);

  config2.Merge(config1);

  EXPECT_CALL(*ukm_data_manager_, StartObservingUkm(_))
      .Times(1)
      .WillOnce(Invoke([&config2](const UkmConfig& actual_config) {
        EXPECT_EQ(actual_config, config2);
      }));
  EXPECT_CALL(*history_observer_,
              SetHistoryBasedSegments(
                  base::flat_set<OptimizationTarget>({kSegmentId})));
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
