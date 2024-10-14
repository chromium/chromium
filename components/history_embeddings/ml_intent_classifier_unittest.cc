// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_intent_classifier.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/history_query_intent.pb.h"

namespace history_embeddings {

namespace {

using optimization_guide::MockOptimizationGuideModelExecutor;
using optimization_guide::MockSession;
using optimization_guide::MockSessionWrapper;
using optimization_guide::
    OptimizationGuideModelExecutionResultStreamingCallback;
using optimization_guide::StreamingResponse;
using ExecResult =
    optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using testing::_;
using testing::NiceMock;

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

class MockClassifierSession : public MockSession {
 public:
  MockClassifierSession() {
    ON_CALL(*this, ExecuteModel(_, _))
        .WillByDefault(
            [&](const google::protobuf::MessageLite& request_metadata,
                OptimizationGuideModelExecutionResultStreamingCallback
                    callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback),
                                            FakeExecute(request_metadata)));
            });
  }
};

class MockExecutor : public MockOptimizationGuideModelExecutor {
 public:
  MockExecutor() {
    ON_CALL(*this, StartSession(_, _)).WillByDefault([&] {
      return std::make_unique<optimization_guide::MockSessionWrapper>(
          &session_);
    });
  }
  NiceMock<MockClassifierSession> session_;
};

class HistoryEmbeddingsMlIntentClassifierTest : public testing::Test {
 public:
  void SetUp() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistoryEmbeddingsMlIntentClassifierTest, IntentYes) {
  NiceMock<MockExecutor> executor_;
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
  NiceMock<MockExecutor> executor_;
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
  NiceMock<MockExecutor> executor_;
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
  MockOptimizationGuideModelExecutor executor_;
  EXPECT_CALL(executor_, StartSession(_, _)).WillRepeatedly([] {
    return nullptr;
  });
  MlIntentClassifier intent_classifier(&executor_);
  {
    base::test::TestFuture<ComputeIntentStatus, bool> future;
    intent_classifier.ComputeQueryIntent("query?", future.GetCallback());
    auto [status, is_query_answerable] = future.Take();
    EXPECT_EQ(status, ComputeIntentStatus::MODEL_UNAVAILABLE);
  }
  task_environment_.RunUntilIdle();  // Trigger DeleteSoon()
}

}  // namespace

}  // namespace history_embeddings
