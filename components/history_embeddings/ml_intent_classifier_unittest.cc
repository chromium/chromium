// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_intent_classifier.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/history_query_intent.pb.h"
#include "components/optimization_guide/proto/history_query_intent_model_metadata.pb.h"

namespace history_embeddings {

namespace {

using optimization_guide::MockOptimizationGuideModelExecutor;
using optimization_guide::MockSession;
using optimization_guide::
    OptimizationGuideModelExecutionResultStreamingCallback;
using optimization_guide::StreamingResponse;
using ExecResult =
    optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using testing::_;
using testing::NiceMock;

using optimization_guide::proto::Any;
using optimization_guide::proto::HistoryQueryIntentModelMetadata;
using optimization_guide::proto::HistoryQueryIntentRequest;
using optimization_guide::proto::HistoryQueryIntentResponse;

auto FakeExecute(const google::protobuf::MessageLite& request_metadata) {
  const HistoryQueryIntentRequest* request =
      static_cast<const HistoryQueryIntentRequest*>(&request_metadata);
  if (request->text().ends_with("!")) {
    return MockSession::FailResult();
  }
  HistoryQueryIntentResponse response;
  response.set_is_answer_seeking(request->text().ends_with("?"));
  return MockSession::SuccessResult(optimization_guide::AnyWrapProto(response));
}

Any CreateModelMetadata() {
  HistoryQueryIntentModelMetadata metadata;
  metadata.set_score_token("True");
  metadata.set_score_threshold(0.5);
  return optimization_guide::AnyWrapProto(metadata);
}

class MockClassifierSession : public MockSession {
 public:
  MockClassifierSession() {
    any_metadata_ = CreateModelMetadata();

    ON_CALL(*this, ExecuteModel(_, _))
        .WillByDefault(
            [&](const google::protobuf::MessageLite& request_metadata,
                OptimizationGuideModelExecutionResultStreamingCallback
                    callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback),
                                            FakeExecute(request_metadata)));
            });

    ON_CALL(*this, GetOnDeviceFeatureMetadata())
        .WillByDefault([&]() -> const Any& { return any_metadata_; });
  }

 protected:
  Any any_metadata_;
};

class MockExecutor : public MockOptimizationGuideModelExecutor {
 public:
  MockExecutor() {
  }
};

class HistoryEmbeddingsMlIntentClassifierTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(executor_, StartSession(_, _)).WillByDefault([&] {
      return std::make_unique<NiceMock<MockSession>>(&session_);
    });
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  NiceMock<MockClassifierSession> session_;
  MockExecutor executor_;
};

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, IntentYes) {
  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query?", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, true);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, IntentNo) {
  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, false);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, ExecutionFails) {
  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query!", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::EXECUTION_FAILURE);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, FailToCreateSession) {
  MockOptimizationGuideModelExecutor executor;
  EXPECT_CALL(executor, StartSession(_, _)).WillRepeatedly([] {
    return nullptr;
  });
  MlIntentClassifier intent_classifier(&executor);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query?", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::MODEL_UNAVAILABLE);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, ScoreTrue) {
  FeatureParameters feature_parameters = GetFeatureParameters();
  feature_parameters.enable_ml_intent_classifier_score = true;
  SetFeatureParametersForTesting(feature_parameters);

  // Above threshold.
  ON_CALL(session_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, true);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, ScoreFalse) {
  FeatureParameters feature_parameters = GetFeatureParameters();
  feature_parameters.enable_ml_intent_classifier_score = true;
  SetFeatureParametersForTesting(feature_parameters);

  // below threshold.
  ON_CALL(session_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.4); })));

  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::SUCCESS);
    EXPECT_EQ(is_query_answerable, false);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, ScoreFailure) {
  FeatureParameters feature_parameters = GetFeatureParameters();
  feature_parameters.enable_ml_intent_classifier_score = true;
  SetFeatureParametersForTesting(feature_parameters);

  // Null score
  ON_CALL(session_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(std::nullopt); })));

  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::EXECUTION_FAILURE);
    EXPECT_EQ(is_query_answerable, false);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}
}  // namespace

}  // namespace history_embeddings
