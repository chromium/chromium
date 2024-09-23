// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/result_refresh_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace segmentation_platform {
namespace {

// Test clients.
const char kTestClient1[] = "client_1";
const char kTestClient2[] = "client_2";

// Test Ids.
const proto::SegmentId kSegmentId1 =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;
const proto::SegmentId kSegmentId2 =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_TABLET_PRODUCTIVITY_USER;

class MockResultProvider : public SegmentResultProvider {
 public:
  MOCK_METHOD1(GetSegmentResult,
               void(std::unique_ptr<GetResultOptions> options));
};

class MockTrainingDataCollector : public TrainingDataCollector {
 public:
  MOCK_METHOD0(OnModelMetadataUpdated, void());
  MOCK_METHOD0(OnServiceInitialized, void());
  MOCK_METHOD0(ReportCollectedContinuousTrainingData, void());
  MOCK_METHOD5(OnDecisionTime,
               TrainingRequestId(proto::SegmentId id,
                                 scoped_refptr<InputContext> input_context,
                                 DecisionType type,
                                 std::optional<ModelProvider::Request> inputs,
                                 bool decision_result_update_trigger));
  MOCK_METHOD5(CollectTrainingData,
               void(SegmentId segment_id,
                    TrainingRequestId request_id,
                    ukm::SourceId ukm_source_id,
                    const TrainingLabels& param,
                    SuccessCallback callback));
};

class ResultRefreshManagerTest : public testing::Test {
 public:
  ResultRefreshManagerTest() = default;
  ~ResultRefreshManagerTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    auto training_data_collector =
        std::make_unique<MockTrainingDataCollector>();
    training_data_collector_ = training_data_collector.get();
    execution_service_ = std::make_unique<ExecutionService>();
    execution_service_->set_training_data_collector_for_testing(
        std::move(training_data_collector));

    std::vector<std::unique_ptr<Config>> configs;
    configs.emplace_back(
        test_utils::CreateTestConfig(kTestClient1, kSegmentId1));
    configs.emplace_back(
        test_utils::CreateTestConfig(kTestClient2, kSegmentId2));
    config_holder_ = std::make_unique<ConfigHolder>(std::move(configs));

    cached_result_writer_ = SetupCachedResultWriter();

    client1_result_provider_ = std::make_unique<MockResultProvider>();
    client2_result_provider_ = std::make_unique<MockResultProvider>();

    result_refresh_manager_ = std::make_unique<ResultRefreshManager>(
        config_holder_.get(), cached_result_writer_.get(),
        PlatformOptions(/*force_refresh_results=*/false,
                        /*disable_model_execution_delay=*/true));
  }

  std::unique_ptr<CachedResultWriter> SetupCachedResultWriter() {
    pref_service.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                std::string());
    std::unique_ptr<ClientResultPrefs> result_prefs =
        std::make_unique<ClientResultPrefs>(&pref_service);
    client_result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service);
    clock_.SetNow(base::Time::Now());

    return std::make_unique<CachedResultWriter>(client_result_prefs_.get(),
                                                &clock_);
  }

  void ExpectSegmentResult(
      const proto::SegmentId segment_id,
      MockResultProvider* segment_result_provider,
      const proto::PredictionResult& result,
      const SegmentResultProvider::ResultState& result_state,
      bool ignore_db_scores) {
    EXPECT_CALL(*segment_result_provider, GetSegmentResult(_))
        .WillOnce(
            Invoke([segment_id, result, result_state, ignore_db_scores](
                       std::unique_ptr<SegmentResultProvider::GetResultOptions>
                           options) {
              EXPECT_EQ(options->ignore_db_scores, ignore_db_scores);
              EXPECT_EQ(options->segment_id, segment_id);
              auto segment_result =
                  std::make_unique<SegmentResultProvider::SegmentResult>(
                      result_state, result, /*rank=*/1);
              std::move(options->callback).Run(std::move(segment_result));
            }));
  }

  void VerifyIfResultUpdatedInPrefs(const std::string& segmentation_key,
                                    proto::PredictionResult expected_result) {
    const proto::ClientResult* client_result =
        client_result_prefs_->ReadClientResultFromPrefs(segmentation_key);
    EXPECT_TRUE(client_result);
    EXPECT_EQ(expected_result.SerializeAsString(),
              client_result->client_result().SerializeAsString());
  }

  void VerifyIfResultNotUpdatedInPrefs(const std::string& segmentation_key) {
    const proto::ClientResult* client_result =
        client_result_prefs_->ReadClientResultFromPrefs(segmentation_key);
    EXPECT_FALSE(client_result);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ExecutionService> execution_service_;
  raw_ptr<MockTrainingDataCollector> training_data_collector_;
  std::unique_ptr<ConfigHolder> config_holder_;
  std::unique_ptr<MockResultProvider> client1_result_provider_;
  std::unique_ptr<MockResultProvider> client2_result_provider_;
  std::unique_ptr<ResultRefreshManager> result_refresh_manager_;
  std::unique_ptr<ClientResultPrefs> client_result_prefs_;
  std::unique_ptr<CachedResultWriter> cached_result_writer_;
  TestingPrefServiceSimple pref_service;
  base::SimpleTestClock clock_;
};

TEST_F(ResultRefreshManagerTest, TestRefreshModelResultsSuccess) {
  // Client 1 gets model result from database.
  proto::PredictionResult result_from_db_for_client1 =
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.8},
          test_utils::GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  ExpectSegmentResult(
      kSegmentId1, client1_result_provider_.get(), result_from_db_for_client1,
      SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed,
      /*ignore_db_scores=*/false);

  // Client 2 gets model result by running the model.
  proto::PredictionResult result_from_model_for_client2 =
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.8},
          test_utils::GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  ExpectSegmentResult(
      kSegmentId2, client2_result_provider_.get(),
      result_from_model_for_client2,
      SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed,
      /*ignore_db_scores=*/false);

  EXPECT_CALL(
      *training_data_collector_,
      OnDecisionTime(_, _, proto::TrainingOutputs::TriggerConfig::PERIODIC, _,
                     true))
      .Times(2);

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  result_providers[kTestClient1] = std::move(client1_result_provider_);
  result_providers[kTestClient2] = std::move(client2_result_provider_);
  result_refresh_manager_->Initialize(std::move(result_providers),
                                      execution_service_.get());
  result_refresh_manager_->RefreshModelResults(/*is_startup=*/true);
  VerifyIfResultUpdatedInPrefs(kTestClient1, result_from_db_for_client1);
  VerifyIfResultUpdatedInPrefs(kTestClient2, result_from_model_for_client2);
}

TEST_F(ResultRefreshManagerTest, TestRefreshModelResultWithNoResult) {
  // Client 1 tries to get model result from database, but signal not collected.
  proto::PredictionResult result_for_client =
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{},
          test_utils::GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now(), /*model_version=*/1);
  ExpectSegmentResult(
      kSegmentId1, client1_result_provider_.get(), result_for_client,
      SegmentResultProvider::ResultState::kServerModelSignalsNotCollected,
      /*ignore_db_scores=*/false);

  // Client 2 tries gets model result by running the model and model execution
  // fails.
  ExpectSegmentResult(
      kSegmentId2, client2_result_provider_.get(), result_for_client,
      SegmentResultProvider::ResultState::kDefaultModelExecutionFailed,
      /*ignore_db_scores=*/false);

  EXPECT_CALL(
      *training_data_collector_,
      OnDecisionTime(_, _, proto::TrainingOutputs::TriggerConfig::PERIODIC, _,
                     true))
      .Times(0);

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  result_providers[kTestClient1] = std::move(client1_result_provider_);
  result_providers[kTestClient2] = std::move(client2_result_provider_);
  result_refresh_manager_->Initialize(std::move(result_providers),
                                      execution_service_.get());
  result_refresh_manager_->RefreshModelResults(/*is_startup=*/true);
  VerifyIfResultNotUpdatedInPrefs(kTestClient1);
  VerifyIfResultNotUpdatedInPrefs(kTestClient2);
}

TEST_F(ResultRefreshManagerTest, TestOnModelUpdated) {
  // Client 1 gets model result from database.
  proto::PredictionResult result_from_db_for_client1 =
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.8},
          test_utils::GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  ExpectSegmentResult(
      kSegmentId1, client1_result_provider_.get(), result_from_db_for_client1,
      SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed,
      /*ignore_db_scores=*/false);

  EXPECT_CALL(
      *training_data_collector_,
      OnDecisionTime(kSegmentId1, _,
                     proto::TrainingOutputs::TriggerConfig::PERIODIC, _, true))
      .Times(1);

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  result_providers[kTestClient1] = std::move(client1_result_provider_);
  result_refresh_manager_->Initialize(std::move(result_providers),
                                      execution_service_.get());
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(kSegmentId1);
  result_refresh_manager_->OnModelUpdated(&segment_info);
  VerifyIfResultUpdatedInPrefs(kTestClient1, result_from_db_for_client1);
}

TEST_F(ResultRefreshManagerTest, TestOnModelUpdatedWithDelay) {
  result_refresh_manager_ = std::make_unique<ResultRefreshManager>(
      config_holder_.get(), cached_result_writer_.get(),
      PlatformOptions(/*force_refresh_results=*/false));

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  result_providers[kTestClient1] = std::move(client1_result_provider_);
  result_refresh_manager_->Initialize(std::move(result_providers),
                                      execution_service_.get());
  proto::SegmentInfo segment_info;
  segment_info.set_segment_id(kSegmentId1);
  result_refresh_manager_->OnModelUpdated(&segment_info);
  VerifyIfResultNotUpdatedInPrefs(kTestClient1);
}

}  // namespace
}  // namespace segmentation_platform
