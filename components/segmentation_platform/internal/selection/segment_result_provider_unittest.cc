// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/segment_result_provider.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_executor_impl.h"
#include "components/segmentation_platform/internal/execution/processing/mock_feature_list_query_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/signals/signal_handler.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

const SegmentId kTestSegment =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
const SegmentId kTestSegment2 =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE;

constexpr float kTestScore = 0.1;
constexpr float kDatabaseScore = 0.6;
constexpr float kTestRank = 0;
constexpr int kDatabaseRank = 1;

class TestModelProvider : public DefaultModelProvider {
 public:
  static constexpr int64_t kVersion = 10;
  explicit TestModelProvider(SegmentId segment)
      : DefaultModelProvider(segment) {}

  std::unique_ptr<DefaultModelProvider::ModelConfig> GetModelConfig() override {
    proto::SegmentationModelMetadata metadata;
    MetadataWriter writer(&metadata);
    writer.SetDefaultSegmentationMetadataConfig();
    std::pair<float, int> mapping[] = {{kTestScore + 0.1, kTestRank},
                                       {kDatabaseScore - 0.1, kDatabaseRank}};
    writer.AddDiscreteMappingEntries("test_key", mapping, 2);
    return std::make_unique<ModelConfig>(std::move(metadata), kVersion);
  }

  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override {
    std::move(callback).Run(ModelProvider::Response(1, kTestScore));
  }

  // Returns true if a model is available.
  bool ModelAvailable() override { return true; }
};

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

  void SetUpGetModelProviderResponse(proto::SegmentId segment_id,
                                     proto::ModelSource model_source,
                                     ModelProvider* model_provider) {
    ON_CALL(*this, GetModelProvider(segment_id, model_source))
        .WillByDefault([=]() { return model_provider; });
  }
};

}  // namespace

class SegmentResultProviderTest : public testing::Test {
 public:
  SegmentResultProviderTest() : provider_factory_(&model_providers_) {}
  ~SegmentResultProviderTest() override = default;

  void SetUp() override {
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    execution_service_ = std::make_unique<ExecutionService>();
    auto query_processor =
        std::make_unique<processing::MockFeatureListQueryProcessor>();
    mock_query_processor_ = query_processor.get();
    mock_model_manager_ = std::make_unique<MockModelManager>();
    execution_service_->InitForTesting(
        std::move(query_processor),
        std::make_unique<ModelExecutorImpl>(&clock_, segment_database_.get(),
                                            mock_query_processor_),
        nullptr, mock_model_manager_.get());
    score_provider_ = SegmentResultProvider::Create(
        segment_database_.get(), &signal_storage_config_,
        execution_service_.get(), &clock_,
        /*force_refresh_results=*/false);
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    LocalStateHelper::GetInstance().Initialize(&prefs_);
  }

  void TearDown() override {
    score_provider_.reset();
    execution_service_.reset();
    segment_database_.reset();
  }

  void ExpectSegmentResultOnGet(
      SegmentId segment_id,
      bool ignore_db_scores,
      SegmentResultProvider::ResultState expected_state,
      std::optional<float> expected_rank) {
    base::RunLoop wait_for_result;
    auto options = std::make_unique<SegmentResultProvider::GetResultOptions>();
    options->segment_id = segment_id;
    options->discrete_mapping_key = "test_key";
    options->ignore_db_scores = ignore_db_scores;
    options->callback = base::BindOnce(
        [](SegmentResultProvider::ResultState expected_state,
           std::optional<float> expected_rank, base::OnceClosure quit,
           std::unique_ptr<SegmentResultProvider::SegmentResult> result) {
          EXPECT_EQ(result->state, expected_state);
          if (expected_rank) {
            EXPECT_NEAR(*expected_rank, *result->rank, 0.01);
          } else {
            EXPECT_FALSE(result->rank);
          }
          std::move(quit).Run();
        },
        expected_state, expected_rank, wait_for_result.QuitClosure());
    score_provider_->GetSegmentResult(std::move(options));
    wait_for_result.Run();
  }

  void SetSegmentResult(SegmentId segment,
                        proto::ModelSource model_source,
                        std::optional<float> score) {
    std::optional<proto::PredictionResult> result;
    if (score) {
      result = proto::PredictionResult();
      result->add_result(*score);
    }
    base::RunLoop wait_for_save;
    segment_database_->SetBucketDuration(segment, 1, proto::TimeUnit::DAY,
                                         model_source);
    segment_database_->SaveSegmentResult(
        segment, model_source, std::move(result),
        base::BindOnce(
            [](base::OnceClosure quit, bool success) { std::move(quit).Run(); },
            wait_for_save.QuitClosure()));
    wait_for_save.Run();
  }

  void InitializeMetadata(
      SegmentId segment_id,
      ModelSource model_source = ModelSource::SERVER_MODEL_SOURCE) {
    segment_database_->FindOrCreateSegment(segment_id, model_source)
        ->mutable_model_metadata()
        ->set_result_time_to_live(7);
    segment_database_->SetBucketDuration(segment_id, 1, proto::TimeUnit::DAY,
                                         model_source);

    // Initialize metadata so that score from default model returns default rank
    // and score from model score returns model rank.
    float mapping[][2] = {{kTestScore + 0.1, kTestRank},
                          {kDatabaseScore - 0.1, kDatabaseRank}};
    segment_database_->AddDiscreteMapping(segment_id, mapping, 2, "test_key",
                                          model_source);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestModelProviderFactory::Data model_providers_;
  TestModelProviderFactory provider_factory_;
  MockSignalDatabase signal_database_;
  raw_ptr<processing::MockFeatureListQueryProcessor, DanglingUntriaged>
      mock_query_processor_ = nullptr;
  std::unique_ptr<MockModelManager> mock_model_manager_;
  SignalHandler signal_handler_;
  std::unique_ptr<ExecutionService> execution_service_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  MockSignalStorageConfig signal_storage_config_;
  std::unique_ptr<SegmentResultProvider> score_provider_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(SegmentResultProviderTest, GetServerModelSegmentNotAvailable) {
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kServerModelSegmentInfoNotAvailable,
      std::nullopt);

  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kServerModelSegmentInfoNotAvailable,
      std::nullopt);
}

TEST_F(SegmentResultProviderTest, GetServerModelSignalNotCollected) {
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  // Score doesn't exist in database.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(false));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kServerModelSignalsNotCollected,
      std::nullopt);

  // Ignoring DB.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(false));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kServerModelSignalsNotCollected,
      std::nullopt);
}

TEST_F(SegmentResultProviderTest, GetServerModelExecutionFailedNotIgnoringDb) {
  InitializeMetadata(kTestSegment);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  // No model available to execute.
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kServerModelExecutionFailed,
      std::nullopt);

  // Feature processing failed.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::SERVER_MODEL_SOURCE, &provider);

  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/true,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kServerModelExecutionFailed,
      std::nullopt);
}

TEST_F(SegmentResultProviderTest, GetServerModelExecutionFailedIgnoringDb) {
  InitializeMetadata(kTestSegment);
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   kDatabaseScore);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));

  // No model available to execute.
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kServerModelExecutionFailed,
      std::nullopt);

  // Feature processing failed.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::SERVER_MODEL_SOURCE, &provider);
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/true,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kServerModelExecutionFailed,
      std::nullopt);
}

TEST_F(SegmentResultProviderTest, GetScoreFromDatabaseForServerModel) {
  InitializeMetadata(kTestSegment);
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   kDatabaseScore);

  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed,
      kDatabaseRank);
}

TEST_F(SegmentResultProviderTest,
       GetScoreFromServerModelExecutionNotIgnoringDb) {
  // Score in database doesn't exist for server model, but exist for default
  // models.
  InitializeMetadata(kTestSegment);
  SetSegmentResult(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE,
                   kDatabaseScore);
  // Both models available for execution. Setting model providers for both
  // models.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::SERVER_MODEL_SOURCE, &provider);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));

  // Gets the rank from test model instead of database.
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed,
      kTestRank);
}

TEST_F(SegmentResultProviderTest, GetScoreFromServerModelExecutionIgnoringDb) {
  InitializeMetadata(kTestSegment);
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   kDatabaseScore);

  // Both models available for execution. Setting model providers for both
  // models.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::SERVER_MODEL_SOURCE, &provider);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));

  // Gets the rank from test model instead of database.
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kServerModelExecutionScoreUsed,
      kTestRank);
}

TEST_F(SegmentResultProviderTest, GetDefaultModelSignalsNotCollected) {
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  InitializeMetadata(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE);

  // Only default model available for execution. Setting server model provider
  // as null.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  // First call is to check opt guide model, and second is to check default
  // model signals.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  ExpectSegmentResultOnGet(
      kTestSegment,
      /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kDefaultModelSignalsNotCollected,
      std::nullopt);
}

TEST_F(SegmentResultProviderTest, GetDefaultModelFailedExecution) {
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  InitializeMetadata(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE);

  // Only default model available for execution. Setting server model provider
  // as null.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(true));

  // Set error while computing features.
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/true,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));
  ExpectSegmentResultOnGet(
      kTestSegment,
      /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kDefaultModelExecutionFailed,
      std::nullopt);
}

TEST_F(SegmentResultProviderTest, GetScoreFromDatabaseForDefaultModel) {
  // Server model segment info doesn't exist.
  InitializeMetadata(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE);
  SetSegmentResult(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE,
                   kDatabaseScore);
  // Only default model available for execution. Setting server model provider
  // as null.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed,
      kDatabaseRank);

  // Server model segment info exists but signal not collected.
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(false));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed,
      kDatabaseRank);

  // Server model execution failed.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillRepeatedly(Return(true));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kDefaultModelDatabaseScoreUsed,
      kDatabaseRank);
}

TEST_F(SegmentResultProviderTest, GetScoreFromDefaultModelExecution) {
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  InitializeMetadata(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE);
  SetSegmentResult(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE,
                   std::nullopt);

  // Only default model available for execution. Setting server model provider
  // as null.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed,
      kTestRank);
}

TEST_F(SegmentResultProviderTest, GetScoreFromDefaultModelExecutionIgnoringDb) {
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  InitializeMetadata(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE);
  SetSegmentResult(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE,
                   kDatabaseScore);

  // Only default model available for execution. Setting server model provider
  // as null.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/true,
      SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed,
      kTestRank);
}

TEST_F(SegmentResultProviderTest, MultipleRequests) {
  InitializeMetadata(kTestSegment, proto::ModelSource::DEFAULT_MODEL_SOURCE);
  SetSegmentResult(kTestSegment, proto::ModelSource::SERVER_MODEL_SOURCE,
                   std::nullopt);
  InitializeMetadata(kTestSegment2);
  SetSegmentResult(kTestSegment2, proto::ModelSource::SERVER_MODEL_SOURCE,
                   kDatabaseScore);

  // Only default model available for execution. Setting server model provider
  // as null.
  TestModelProvider provider(kTestSegment);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment, ModelSource::DEFAULT_MODEL_SOURCE, &provider);

  // Both models available for execution. Setting model providers for both
  // models.
  TestModelProvider provider2(kTestSegment2);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment2, ModelSource::SERVER_MODEL_SOURCE, &provider2);
  mock_model_manager_->SetUpGetModelProviderResponse(
      kTestSegment2, ModelSource::DEFAULT_MODEL_SOURCE, &provider2);

  // For the first request, the database does not have valid result, and default
  // provider fails execution.
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .WillOnce(RunOnceCallback<6>(/*error=*/false,
                                   ModelProvider::Request{{1, 2}},
                                   ModelProvider::Response()));
  ExpectSegmentResultOnGet(
      kTestSegment, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kDefaultModelExecutionScoreUsed,
      kTestRank);

  // For the second request the database has valid result.
  EXPECT_CALL(*mock_query_processor_, ProcessFeatureList(_, _, _, _, _, _, _))
      .Times(0);
  ExpectSegmentResultOnGet(
      kTestSegment2, /*ignore_db_scores=*/false,
      SegmentResultProvider::ResultState::kServerModelDatabaseScoreUsed,
      kDatabaseRank);
}

}  // namespace segmentation_platform
