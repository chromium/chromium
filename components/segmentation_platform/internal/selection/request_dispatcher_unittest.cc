// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_dispatcher.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
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

class MockRequestHandler : public RequestHandler {
 public:
  MockRequestHandler() = default;
  ~MockRequestHandler() override = default;

  MOCK_METHOD3(GetClassificationResult,
               void(const PredictionOptions& options,
                    scoped_refptr<InputContext> input_context,
                    ClassificationResultCallback callback));
};

class RequestDispatcherTest : public testing::Test {
 public:
  RequestDispatcherTest() = default;
  ~RequestDispatcherTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    configs_.emplace_back(test_utils::CreateTestConfig(
        kTestClient1, SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB));
    configs_.emplace_back(test_utils::CreateTestConfig(
        kTestClient2, SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB));

    request_dispatcher_ =
        std::make_unique<RequestDispatcher>(configs_, nullptr);

    auto handler1 = std::make_unique<MockRequestHandler>();
    request_handler1_ = handler1.get();
    request_dispatcher_->set_request_handler_for_testing(kTestClient1,
                                                         std::move(handler1));

    auto handler2 = std::make_unique<MockRequestHandler>();
    request_handler2_ = handler2.get();
    request_dispatcher_->set_request_handler_for_testing(kTestClient2,
                                                         std::move(handler2));
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 const ClassificationResult& expected,
                                 const ClassificationResult& actual) {
    EXPECT_EQ(expected.ordered_labels, actual.ordered_labels);
    EXPECT_EQ(expected.status, actual.status);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<std::unique_ptr<Config>> configs_;
  raw_ptr<MockRequestHandler> request_handler1_ = nullptr;
  raw_ptr<MockRequestHandler> request_handler2_ = nullptr;
  std::unique_ptr<RequestDispatcher> request_dispatcher_;
};

TEST_F(RequestDispatcherTest, TestRequestQueuingWithInitFailure) {
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_EQ(0, request_dispatcher_->get_pending_actions_size_for_testing());

  // Request handler will never be invoked if init fails.
  EXPECT_CALL(*request_handler1_, GetClassificationResult(_, _, _)).Times(0);

  base::RunLoop loop;
  request_dispatcher_->GetClassificationResult(
      kTestClient1, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(),
                     ClassificationResult(PredictionStatus::kFailed)));
  EXPECT_EQ(1, request_dispatcher_->get_pending_actions_size_for_testing());

  // Finish platform initialization with failure. The request queue is flushed
  // and callbacks are invoked with empty results.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(false, &execution_service,
                                             std::move(result_providers));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->get_pending_actions_size_for_testing());
}

TEST_F(RequestDispatcherTest, TestRequestQueuingWithInitSuccess) {
  base::RunLoop loop;
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_EQ(0, request_dispatcher_->get_pending_actions_size_for_testing());

  // Request from client 1.
  ClassificationResult result1(PredictionStatus::kSucceeded);
  result1.ordered_labels.emplace_back("test_label1");
  EXPECT_CALL(*request_handler1_, GetClassificationResult(_, _, _))
      .WillRepeatedly(Invoke([&](const PredictionOptions& options,
                                 scoped_refptr<InputContext> input_context,
                                 ClassificationResultCallback callback) {
        std::move(callback).Run(result1);
      }));

  request_dispatcher_->GetClassificationResult(
      kTestClient1, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result1));
  EXPECT_EQ(1, request_dispatcher_->get_pending_actions_size_for_testing());

  // Request from client 2.
  ClassificationResult result2(PredictionStatus::kSucceeded);
  result2.ordered_labels.emplace_back("test_label2");
  EXPECT_CALL(*request_handler2_, GetClassificationResult(_, _, _))
      .WillRepeatedly(Invoke([&](const PredictionOptions& options,
                                 scoped_refptr<InputContext> input_context,
                                 ClassificationResultCallback callback) {
        std::move(callback).Run(result2);
      }));

  request_dispatcher_->GetClassificationResult(
      kTestClient2, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result2));
  EXPECT_EQ(2, request_dispatcher_->get_pending_actions_size_for_testing());

  // Finish platform initialization with success. The request queue is flushed,
  // and the request handler is invoked.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->get_pending_actions_size_for_testing());
}

TEST_F(RequestDispatcherTest, TestRequestAfterInitSuccess) {
  base::RunLoop loop;
  PredictionOptions options;
  options.on_demand_execution = true;

  // Init platform.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));

  // Request from client 1.
  ClassificationResult result1(PredictionStatus::kSucceeded);
  result1.ordered_labels.emplace_back("test_label1");
  EXPECT_CALL(*request_handler1_, GetClassificationResult(_, _, _))
      .WillRepeatedly(Invoke([&](const PredictionOptions& options,
                                 scoped_refptr<InputContext> input_context,
                                 ClassificationResultCallback callback) {
        std::move(callback).Run(result1);
      }));

  request_dispatcher_->GetClassificationResult(
      kTestClient1, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result1));
  EXPECT_EQ(0, request_dispatcher_->get_pending_actions_size_for_testing());

  // Request from client 2.
  ClassificationResult result2(PredictionStatus::kSucceeded);
  result2.ordered_labels.emplace_back("test_label2");
  EXPECT_CALL(*request_handler2_, GetClassificationResult(_, _, _))
      .WillRepeatedly(Invoke([&](const PredictionOptions& options,
                                 scoped_refptr<InputContext> input_context,
                                 ClassificationResultCallback callback) {
        std::move(callback).Run(result2);
      }));

  request_dispatcher_->GetClassificationResult(
      kTestClient2, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result2));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->get_pending_actions_size_for_testing());
}

}  // namespace
}  // namespace segmentation_platform
