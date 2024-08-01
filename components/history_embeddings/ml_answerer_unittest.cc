// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_answerer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

using optimization_guide::OptimizationGuideModelExecutionError;
using optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using optimization_guide::proto::HistoryAnswerResponse;
using testing::_;

namespace {

constexpr char kAnswerResponseTypeURL[] =
    "type.googleapis.com/optimization_guide.proto.HistoryAnswerResponse";

class MockModelExecutor
    : public optimization_guide::OptimizationGuideModelExecutor {
 public:
  MOCK_METHOD(bool,
              CanCreateOnDeviceSession,
              (optimization_guide::ModelBasedCapabilityKey feature,
               raw_ptr<optimization_guide::OnDeviceModelEligibilityReason>
                   debug_reason));

  MOCK_METHOD(std::unique_ptr<Session>,
              StartSession,
              (optimization_guide::ModelBasedCapabilityKey feature,
               const std::optional<optimization_guide::SessionConfigParams>&
                   config_params));

  MOCK_METHOD(void,
              ExecuteModel,
              (optimization_guide::ModelBasedCapabilityKey feature,
               const google::protobuf::MessageLite& request_metadata,
               optimization_guide::OptimizationGuideModelExecutionResultCallback
                   callback));

  size_t GetCounter() { return counter_; }

  void IncrementCounter() { counter_++; }

  void Reset() { counter_ = 0; }

 private:
  size_t counter_ = 0;
};

class MockSession
    : public optimization_guide::OptimizationGuideModelExecutor::Session {
 public:
  MOCK_METHOD(void,
              AddContext,
              (const google::protobuf::MessageLite& request_metadata));
  MOCK_METHOD(
      void,
      Score,
      (const std::string& text,
       optimization_guide::OptimizationGuideModelScoreCallback callback));
  MOCK_METHOD(
      void,
      ExecuteModel,
      (const google::protobuf::MessageLite& request_metadata,
       optimization_guide::
           OptimizationGuideModelExecutionResultStreamingCallback callback));
  MOCK_METHOD(
      void,
      GetSizeInTokens,
      (const std::string& text,
       optimization_guide::OptimizationGuideModelSizeInTokenCallback callback));
};

// A wrapper that passes through calls to the underlying MockSession. Allows for
// easily mocking calls with a single session object.
class MockSessionWrapper
    : public optimization_guide::OptimizationGuideModelExecutor::Session {
 public:
  explicit MockSessionWrapper(MockSession* session) : session_(session) {}

  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override {
    session_->AddContext(request_metadata);
  }

  void Score(const std::string& text,
             optimization_guide::OptimizationGuideModelScoreCallback callback)
      override {
    session_->Score(text, std::move(callback));
  }

  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback) override {
    session_->ExecuteModel(request_metadata, std::move(callback));
  }

  void GetSizeInTokens(
      const std::string& text,
      optimization_guide::OptimizationGuideModelSizeInTokenCallback callback)
      override {
    session_->GetSizeInTokens(text, std::move(callback));
  }

 private:
  raw_ptr<MockSession> session_;
};

}  // namespace

class MlAnswererTest : public testing::Test {
 public:
  void SetUp() override {
    ml_answerer_ = std::make_unique<MlAnswerer>(&model_executor_);
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    ml_answerer_ = nullptr;
    model_executor_.Reset();
  }

 protected:
  optimization_guide::StreamingResponse MakeResponse(
      const std::string& answer_text,
      bool is_complete = true) {
    HistoryAnswerResponse answer_response;
    answer_response.mutable_answer()->set_text(answer_text);
    optimization_guide::proto::Any any;
    any.set_type_url(kAnswerResponseTypeURL);
    answer_response.SerializeToString(any.mutable_value());
    return optimization_guide::StreamingResponse{
        .response = any,
        .is_complete = is_complete,
    };
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MlAnswerer> ml_answerer_;
  testing::NiceMock<MockModelExecutor> model_executor_;
  testing::NiceMock<MockSession> session_1_, session_2_;
};

TEST_F(MlAnswererTest, ComputeAnswerNoSession) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return nullptr;
  });

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  ComputeAnswerCallback callback =
      base::BindOnce([](AnswererResult answer_result) {
        EXPECT_EQ(ComputeAnswerStatus::MODEL_UNAVAILABLE, answer_result.status);
      });
  ml_answerer_->ComputeAnswer("query", context, std::move(callback));
}

#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(MlAnswererTest, ComputeAnswerExecutionFailure) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return std::make_unique<MockSessionWrapper>(&session_1_);
  });

  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  ON_CALL(session_1_, ExecuteModel(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    OptimizationGuideModelStreamingExecutionResult(
                        base::unexpected(
                            OptimizationGuideModelExecutionError::
                                FromModelExecutionError(
                                    OptimizationGuideModelExecutionError::
                                        ModelExecutionError::kGenericFailure)),
                        /*provided_by_on_device=*/true, nullptr)));
          })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  ComputeAnswerCallback callback =
      base::BindOnce([](AnswererResult answer_result) {
        EXPECT_EQ(ComputeAnswerStatus::EXECUTION_FAILURE, answer_result.status);
      });
  ml_answerer_->ComputeAnswer("query", context, std::move(callback));
}
#endif

TEST_F(MlAnswererTest, ComputeAnswerSingleUrl) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return std::make_unique<MockSessionWrapper>(&session_1_);
  });

  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  ON_CALL(session_1_, ExecuteModel(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               OptimizationGuideModelStreamingExecutionResult(
                                   base::ok(MakeResponse("Answer_1")),
                                   /*provided_by_on_device=*/true, nullptr)));
          })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  ComputeAnswerCallback callback =
      base::BindOnce([](AnswererResult answer_result) {
        EXPECT_EQ("Answer_1", answer_result.answer.text());
        EXPECT_EQ("url_1", answer_result.url);
      });

  ml_answerer_->ComputeAnswer("query", context, std::move(callback));
}

TEST_F(MlAnswererTest, ComputeAnswerMultipleUrls) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    if (model_executor_.GetCounter() == 0) {
      model_executor_.IncrementCounter();
      return std::make_unique<MockSessionWrapper>(&session_1_);
    } else if (model_executor_.GetCounter() == 1) {
      model_executor_.IncrementCounter();
      return std::make_unique<MockSessionWrapper>(&session_2_);
    }
    return std::unique_ptr<MockSessionWrapper>(nullptr);
  });

  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  ON_CALL(session_2_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) {
            std::move(callback).Run(0.9);
          })));  // Speculative decoding should continue with this session.

  ON_CALL(session_2_, ExecuteModel(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               OptimizationGuideModelStreamingExecutionResult(
                                   base::ok(MakeResponse("Answer_2")),
                                   /*provided_by_on_device=*/true, nullptr)));
          })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  context.url_passages_map.insert({"url_2", {"passage_21", "passage_22"}});
  ComputeAnswerCallback callback =
      base::BindOnce([](AnswererResult answer_result) {
        EXPECT_EQ("Answer_2", answer_result.answer.text());
        EXPECT_EQ("url_2", answer_result.url);
      });

  ml_answerer_->ComputeAnswer("query", context, std::move(callback));
}

}  // namespace history_embeddings
