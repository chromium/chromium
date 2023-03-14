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
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/internal/selection/client_result_prefs.h"
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
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER;

class MockResultProvider : public SegmentResultProvider {
 public:
  MOCK_METHOD1(GetSegmentResult,
               void(std::unique_ptr<GetResultOptions> options));
};

class ResultRefreshManagerTest : public testing::Test {
 public:
  ResultRefreshManagerTest() = default;
  ~ResultRefreshManagerTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    configs_.emplace_back(
        test_utils::CreateTestConfig(kTestClient1, kSegmentId));
    configs_.emplace_back(
        test_utils::CreateTestConfig(kTestClient2, kSegmentId));
    cached_result_writer_ = SetupCachedResultWriter();

    client1_result_provider_ = std::make_unique<MockResultProvider>();
    client2_result_provider_ = std::make_unique<MockResultProvider>();

    result_refresh_manager_ = std::make_unique<ResultRefreshManager>(
        configs_, std::move(cached_result_writer_), PlatformOptions(false));
  }

  std::unique_ptr<CachedResultWriter> SetupCachedResultWriter() {
    pref_service.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                std::string());
    std::unique_ptr<ClientResultPrefs> result_prefs =
        std::make_unique<ClientResultPrefs>(&pref_service);
    client_result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service);
    clock_.SetNow(base::Time::Now());

    return std::make_unique<CachedResultWriter>(std::move(result_prefs),
                                                &clock_);
  }

  void ExpectSegmentResult(
      MockResultProvider* segment_result_provider,
      const proto::PredictionResult& result,
      const SegmentResultProvider::ResultState& result_state,
      bool ignore_db_scores) {
    EXPECT_CALL(*segment_result_provider, GetSegmentResult(_))
        .WillOnce(
            Invoke([result, result_state, ignore_db_scores](
                       std::unique_ptr<SegmentResultProvider::GetResultOptions>
                           options) {
              EXPECT_EQ(options->ignore_db_scores, ignore_db_scores);
              EXPECT_EQ(options->segment_id, kSegmentId);
              auto segment_result =
                  std::make_unique<SegmentResultProvider::SegmentResult>(
                      result_state, result, /*rank=*/1);
              std::move(options->callback).Run(std::move(segment_result));
            }));
  }

  void VerifyIfResultUpdatedInPrefs(const std::string& segmentation_key,
                                    proto::PredictionResult expected_result) {
    absl::optional<proto::ClientResult> client_result =
        client_result_prefs_->ReadClientResultFromPrefs(segmentation_key);
    EXPECT_TRUE(client_result.has_value());
    EXPECT_EQ(expected_result.SerializeAsString(),
              client_result.value().client_result().SerializeAsString());
  }

  void VerifyIfResultNotUpdatedInPrefs(const std::string& segmentation_key) {
    absl::optional<proto::ClientResult> client_result =
        client_result_prefs_->ReadClientResultFromPrefs(segmentation_key);
    EXPECT_FALSE(client_result.has_value());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<std::unique_ptr<Config>> configs_;
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
          /*timestamp=*/base::Time::Now());

  ExpectSegmentResult(client1_result_provider_.get(),
                      result_from_db_for_client1,
                      SegmentResultProvider::ResultState::kSuccessFromDatabase,
                      /*ignore_db_scores=*/false);

  // Client 2 gets model result by running the model.
  proto::PredictionResult result_from_model_for_client2 =
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{0.8},
          test_utils::GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now());

  ExpectSegmentResult(client2_result_provider_.get(),
                      result_from_model_for_client2,
                      SegmentResultProvider::ResultState::kTfliteModelScoreUsed,
                      /*ignore_db_scores=*/false);

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  result_providers[kTestClient1] = std::move(client1_result_provider_);
  result_providers[kTestClient2] = std::move(client2_result_provider_);

  result_refresh_manager_->RefreshModelResults(std::move(result_providers));

  VerifyIfResultUpdatedInPrefs(kTestClient1, result_from_db_for_client1);
  VerifyIfResultUpdatedInPrefs(kTestClient2, result_from_model_for_client2);
}

TEST_F(ResultRefreshManagerTest, TestRefreshModelResultWithNoResult) {
  // Client 1 tries to get model result from database, but signal not collected.
  proto::PredictionResult result_for_client =
      metadata_utils::CreatePredictionResult(
          /*model_scores=*/{},
          test_utils::GetTestOutputConfigForBinaryClassifier(),
          /*timestamp=*/base::Time::Now());
  ExpectSegmentResult(client1_result_provider_.get(), result_for_client,
                      SegmentResultProvider::ResultState::kSignalsNotCollected,
                      /*ignore_db_scores=*/false);

  // Client 2 tries gets model result by running the model and model execution
  // fails.
  ExpectSegmentResult(
      client2_result_provider_.get(), result_for_client,
      SegmentResultProvider::ResultState::kDefaultModelExecutionFailed,
      /*ignore_db_scores=*/false);

  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  result_providers[kTestClient1] = std::move(client1_result_provider_);
  result_providers[kTestClient2] = std::move(client2_result_provider_);

  result_refresh_manager_->RefreshModelResults(std::move(result_providers));

  VerifyIfResultNotUpdatedInPrefs(kTestClient1);
  VerifyIfResultNotUpdatedInPrefs(kTestClient2);
}

}  // namespace
}  // namespace segmentation_platform
