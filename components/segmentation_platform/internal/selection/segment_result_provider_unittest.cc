// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/execution/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_executor_impl.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

const OptimizationTarget kTestSegment =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
const OptimizationTarget kTestSegment2 =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_VOICE;

constexpr float kDefaultScore = 0.1;
constexpr float kModelScore = 0.6;
constexpr int kDefaultRank = 0;
constexpr int kModelRank = 1;

class DefaultProvider : public ModelProvider {
 public:
  static constexpr int64_t kVersion = 10;
  explicit DefaultProvider(OptimizationTarget segment)
      : ModelProvider(segment) {}

  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override {
    proto::SegmentationModelMetadata metadata;
    metadata.set_time_unit(proto::TimeUnit::DAY);
    model_updated_callback.Run(optimization_target_, metadata, kVersion);
  }

  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override {
    std::move(callback).Run(kDefaultScore);
  }

  // Returns true if a model is available.
  bool ModelAvailable() override { return true; }
};

}  // namespace

class SegmentResultProviderTest : public testing::Test {
 public:
  SegmentResultProviderTest() : provider_factory_(&model_providers_) {}
  ~SegmentResultProviderTest() override = default;

  void SetUp() override {
    default_manager_ = std::make_unique<DefaultModelManager>(
        &provider_factory_,
        std::vector<OptimizationTarget>({kTestSegment, kTestSegment2}));
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    execution_service_ = std::make_unique<ExecutionService>();
    auto query_processor = std::make_unique<MockFeatureListQueryProcessor>();
    mock_query_processor_ = query_processor.get();
    execution_service_->InitForTesting(
        std::move(query_processor),
        std::make_unique<ModelExecutorImpl>(&clock_, mock_query_processor_),
        nullptr);
    score_provider_ = SegmentResultProvider::Create(
        segment_database_.get(), &signal_storage_config_,
        default_manager_.get(), execution_service_.get(), &clock_,
        /*force_refresh_results=*/false);
  }

  void TearDown() override {
    score_provider_.reset();
    segment_database_.reset();
    default_manager_.reset();
  }

  void ExpectSegmentResultOnGet(
      OptimizationTarget segment_id,
      SegmentResultProvider::ResultState expected_state,
      absl::optional<int> expected_rank) {
    base::RunLoop wait_for_result;
    score_provider_->GetSegmentResult(
        segment_id, "test_key",
        base::BindOnce(
            [](SegmentResultProvider::ResultState expected_state,
               absl::optional<int> expected_rank, base::OnceClosure quit,
               std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
              EXPECT_EQ(result->state, expected_state);
              if (expected_rank) {
                EXPECT_EQ(*expected_rank, result->rank);
              } else {
                EXPECT_FALSE(result->rank);
              }
              std::move(quit).Run();
            },
            expected_state, expected_rank, wait_for_result.QuitClosure()));
    wait_for_result.Run();
  }

  void SetSegmentResult(OptimizationTarget segment,
                        absl::optional<float> score) {
    absl::optional<proto::PredictionResult> result;
    if (score) {
      result = proto::PredictionResult();
      result->set_result(*score);
    }
    base::RunLoop wait_for_save;
    segment_database_->SaveSegmentResult(
        segment, std::move(result),
        base::BindOnce(
            [](base::OnceClosure quit, bool success) { std::move(quit).Run(); },
            wait_for_save.QuitClosure()));
    wait_for_save.Run();
  }

  void InitializeMetadata(OptimizationTarget segment_id) {
    segment_database_->FindOrCreateSegment(segment_id)
        ->mutable_model_metadata()
        ->set_result_time_to_live(7);
    segment_database_->SetBucketDuration(segment_id, 1, proto::TimeUnit::DAY);

    // Initialize metadata so that score from default model returns default rank
    // and score from model score returns model rank.
    float mapping[][2] = {{kDefaultScore + 0.1, kDefaultRank},
                          {kModelScore - 0.1, kModelRank}};
    segment_database_->AddDiscreteMapping(segment_id, mapping, 2, "test_key");
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestModelProviderFactory::Data model_providers_;
  TestModelProviderFactory provider_factory_;
  MockSignalDatabase signal_database_;
  MockFeatureListQueryProcessor* mock_query_processor_ = nullptr;
  SignalHandler signal_handler_;
  std::unique_ptr<DefaultModelManager> default_manager_;
  std::unique_ptr<ExecutionService> execution_service_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  MockSignalStorageConfig signal_storage_config_;
  std::unique_ptr<SegmentResultProvider> score_provider_;
};

TEST_F(SegmentResultProviderTest, GetScoreWithoutInfo) {
  ExpectSegmentResultOnGet(
      kTestSegment, SegmentResultProvider::ResultState::kSegmentNotAvailable,
      absl::nullopt);
}

TEST_F(SegmentResultProviderTest, GetScoreFromDbWithoutResult) {
  SetSegmentResult(kTestSegment, absl::nullopt);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true));
  ExpectSegmentResultOnGet(
      kTestSegment, SegmentResultProvider::ResultState::kDatabaseScoreNotReady,
      absl::nullopt);
}

TEST_F(SegmentResultProviderTest, GetScoreNotEnoughSignals) {
  SetSegmentResult(kTestSegment, absl::nullopt);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(false));
  ExpectSegmentResultOnGet(
      kTestSegment, SegmentResultProvider::ResultState::kSignalsNotCollected,
      absl::nullopt);
}

TEST_F(SegmentResultProviderTest, GetScoreFromDb) {
  InitializeMetadata(kTestSegment);
  SetSegmentResult(kTestSegment, kModelScore);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true));
  ExpectSegmentResultOnGet(
      kTestSegment, SegmentResultProvider::ResultState::kSuccessFromDatabase,
      kModelRank);
}

TEST_F(SegmentResultProviderTest, DefaultNeedsSignal) {
  SetSegmentResult(kTestSegment, absl::nullopt);
  std::map<OptimizationTarget, std::unique_ptr<ModelProvider>> p;
  p.emplace(kTestSegment, std::make_unique<DefaultProvider>(kTestSegment));
  default_manager_->SetDefaultProvidersForTesting(std::move(p));

  // First call is to check opt guide model, and second is to check default
  // model signals.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  ExpectSegmentResultOnGet(
      kTestSegment,
      SegmentResultProvider::ResultState::kDefaultModelSignalNotCollected,
      absl::nullopt);
}

TEST_F(SegmentResultProviderTest, DefaultModelFailedExecution) {
  SetSegmentResult(kTestSegment, absl::nullopt);
  std::map<OptimizationTarget, std::unique_ptr<ModelProvider>> p;
  p.emplace(kTestSegment, std::make_unique<DefaultProvider>(kTestSegment));
  default_manager_->SetDefaultProvidersForTesting(std::move(p));

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true))
      .WillOnce(Return(true));

  // Set error while computing features.
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(/*error=*/true, std::vector<float>{{1, 2}}));
  ExpectSegmentResultOnGet(
      kTestSegment,
      SegmentResultProvider::ResultState::kDefaultModelExecutionFailed,
      absl::nullopt);
}

TEST_F(SegmentResultProviderTest, GetFromDefault) {
  SetSegmentResult(kTestSegment, absl::nullopt);
  std::map<OptimizationTarget, std::unique_ptr<ModelProvider>> p;
  p.emplace(kTestSegment, std::make_unique<DefaultProvider>(kTestSegment));
  default_manager_->SetDefaultProvidersForTesting(std::move(p));

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _))
      .WillOnce(
          RunOnceCallback<3>(/*error=*/false, std::vector<float>{{1, 2}}));
  ExpectSegmentResultOnGet(
      kTestSegment, SegmentResultProvider::ResultState::kDefaultModelScoreUsed,
      kDefaultRank);
}

TEST_F(SegmentResultProviderTest, MultipleRequests) {
  InitializeMetadata(kTestSegment);
  SetSegmentResult(kTestSegment, absl::nullopt);
  InitializeMetadata(kTestSegment2);
  SetSegmentResult(kTestSegment2, kModelScore);

  std::map<OptimizationTarget, std::unique_ptr<ModelProvider>> p;
  p.emplace(kTestSegment, std::make_unique<DefaultProvider>(kTestSegment));
  p.emplace(kTestSegment2, std::make_unique<DefaultProvider>(kTestSegment2));
  default_manager_->SetDefaultProvidersForTesting(std::move(p));

  // For the first request, the database does not have valid result, and default
  // provider fails execution.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _))
      .WillOnce(
          RunOnceCallback<3>(/*error=*/false, std::vector<float>{{1, 2}}));
  ExpectSegmentResultOnGet(
      kTestSegment, SegmentResultProvider::ResultState::kDefaultModelScoreUsed,
      kDefaultRank);

  // For the second request the database has valid result.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _)).Times(0);
  ExpectSegmentResultOnGet(
      kTestSegment2, SegmentResultProvider::ResultState::kSuccessFromDatabase,
      kModelRank);
}

}  // namespace segmentation_platform
