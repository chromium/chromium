// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_dispatcher.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/cached_result_writer.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/mock_ukm_data_manager.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace segmentation_platform {
namespace {

// Test clients.
const char kDeviceSwitcherClient[] = "device_switcher";
const char kAdaptiveToolbarClient[] = "adaptive_toolbar";
const char kTestLabel1[] = "test_label1";
const char kTestLabel2[] = "test_label2";

proto::PredictionResult CreatePredictionResultWithBinaryClassifier(
    const char* const label) {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinaryClassifier(0.5f, label, "unused");

  proto::PredictionResult prediction_result;
  prediction_result.add_result(0.8f);
  prediction_result.mutable_output_config()->Swap(
      model_metadata.mutable_output_config());
  return prediction_result;
}

proto::PredictionResult CreatePredictionResultWithGenericPredictor() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForGenericPredictor({"output1", "output2"});

  proto::PredictionResult prediction_result;
  prediction_result.add_result(0.8f);
  prediction_result.add_result(0.2f);
  prediction_result.mutable_output_config()->Swap(
      model_metadata.mutable_output_config());
  return prediction_result;
}

class MockRequestHandler : public RequestHandler {
 public:
  MockRequestHandler() = default;
  ~MockRequestHandler() override = default;

  MOCK_METHOD3(GetPredictionResult,
               void(const PredictionOptions& prediction_options,
                    scoped_refptr<InputContext> input_context,
                    RawResultCallback callback));
};

class RequestDispatcherTest : public testing::Test {
 public:
  RequestDispatcherTest() = default;
  ~RequestDispatcherTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    std::vector<std::unique_ptr<Config>> configs;
    configs.emplace_back(test_utils::CreateTestConfig(
        kDeviceSwitcherClient,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER));
    configs.back()->auto_execute_and_cache = false;
    configs.emplace_back(test_utils::CreateTestConfig(
        kAdaptiveToolbarClient,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR));
    configs.back()->auto_execute_and_cache = false;
    configs.emplace_back(test_utils::CreateTestConfig(
        kShoppingUserSegmentationKey,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER));
    configs.back()->auto_execute_and_cache = true;
    auto config_holder = std::make_unique<ConfigHolder>(std::move(configs));

    prefs_.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                          std::string());
    client_result_prefs_ = std::make_unique<ClientResultPrefs>(&prefs_);
    auto cached_result_writer = std::make_unique<CachedResultWriter>(
        client_result_prefs_.get(), &clock_);
    storage_service_ = std::make_unique<StorageService>(
        nullptr, nullptr, nullptr, nullptr, std::move(config_holder),
        &ukm_data_manager_);
    storage_service_->set_cached_result_writer_for_testing(
        std::move(cached_result_writer));

    request_dispatcher_ =
        std::make_unique<RequestDispatcher>(storage_service_.get());

    auto handler1 = std::make_unique<MockRequestHandler>();
    request_handler1_ = handler1.get();
    request_dispatcher_->set_request_handler_for_testing(kDeviceSwitcherClient,
                                                         std::move(handler1));

    auto handler2 = std::make_unique<MockRequestHandler>();
    request_handler2_ = handler2.get();
    request_dispatcher_->set_request_handler_for_testing(kAdaptiveToolbarClient,
                                                         std::move(handler2));
    auto handler3 = std::make_unique<MockRequestHandler>();
    request_handler3_ = handler3.get();
    request_dispatcher_->set_request_handler_for_testing(
        kShoppingUserSegmentationKey, std::move(handler3));
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 const ClassificationResult& expected,
                                 const ClassificationResult& actual) {
    EXPECT_EQ(expected.ordered_labels, actual.ordered_labels);
    EXPECT_EQ(expected.status, actual.status);
    std::move(closure).Run();
  }

  void OnGetAnnotatedNumericResult(base::RepeatingClosure closure,
                                   const AnnotatedNumericResult& expected,
                                   const AnnotatedNumericResult& actual) {
    EXPECT_EQ(expected.result.SerializeAsString(),
              actual.result.SerializeAsString());
    EXPECT_EQ(expected.status, actual.status);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::SimpleTestClock clock_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<ClientResultPrefs> client_result_prefs_;
  MockUkmDataManager ukm_data_manager_;
  std::unique_ptr<StorageService> storage_service_;
  raw_ptr<MockRequestHandler, DanglingUntriaged> request_handler1_ = nullptr;
  raw_ptr<MockRequestHandler, DanglingUntriaged> request_handler2_ = nullptr;
  raw_ptr<MockRequestHandler, DanglingUntriaged> request_handler3_ = nullptr;
  std::unique_ptr<RequestDispatcher> request_dispatcher_;
};

TEST_F(RequestDispatcherTest, TestRequestQueuingWithInitFailure) {
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());

  // Request handler will never be invoked if init fails.
  EXPECT_CALL(*request_handler1_, GetPredictionResult(_, _, _)).Times(0);

  base::RunLoop loop;
  request_dispatcher_->GetClassificationResult(
      kDeviceSwitcherClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(),
                     ClassificationResult(PredictionStatus::kFailed)));
  EXPECT_EQ(1, request_dispatcher_->GetPendingActionCountForTesting());

  // Finish platform initialization with failure. The request queue is flushed
  // and callbacks are invoked with empty results.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(false, &execution_service,
                                             std::move(result_providers));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

TEST_F(RequestDispatcherTest,
       TestRequestQueuingWithInitSuccessAndNoModelsLoading) {
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());

  // Request from client 1.
  RawResult raw_result1(PredictionStatus::kSucceeded);
  raw_result1.result = CreatePredictionResultWithBinaryClassifier(kTestLabel1);
  EXPECT_CALL(*request_handler1_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result1](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result1);
          }));

  ClassificationResult result1(PredictionStatus::kSucceeded);
  result1.ordered_labels.emplace_back(kTestLabel1);
  request_dispatcher_->GetClassificationResult(
      kDeviceSwitcherClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), run_loop_1.QuitClosure(),
                     result1));
  EXPECT_EQ(1, request_dispatcher_->GetPendingActionCountForTesting());

  // Request from client 2.
  RawResult raw_result2(PredictionStatus::kSucceeded);
  raw_result2.result = CreatePredictionResultWithBinaryClassifier(kTestLabel2);
  EXPECT_CALL(*request_handler2_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result2](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result2);
          }));

  ClassificationResult result2(PredictionStatus::kSucceeded);
  result2.ordered_labels.emplace_back(kTestLabel2);
  request_dispatcher_->GetClassificationResult(
      kAdaptiveToolbarClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), run_loop_2.QuitClosure(),
                     result2));
  EXPECT_EQ(2, request_dispatcher_->GetPendingActionCountForTesting());

  // Finish platform initialization with success. The request queue shouldn't be
  // cleared because the models for the queued segments haven't been
  // initialized.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  EXPECT_EQ(2, request_dispatcher_->GetPendingActionCountForTesting());

  // Run all pending tasks, this triggers a timeout to clear the request queue
  // even if the models didn't load.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());

  run_loop_1.Run();
  run_loop_2.Run();
}

TEST_F(RequestDispatcherTest,
       TestRequestQueuingWithInitSuccessAndAfterModelsLoaded) {
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());

  // Request from client 1.
  RawResult raw_result1(PredictionStatus::kSucceeded);
  raw_result1.result = CreatePredictionResultWithBinaryClassifier(kTestLabel1);
  EXPECT_CALL(*request_handler1_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result1](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result1);
          }));

  ClassificationResult result1(PredictionStatus::kSucceeded);
  result1.ordered_labels.emplace_back(kTestLabel1);
  request_dispatcher_->GetClassificationResult(
      kDeviceSwitcherClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), run_loop_1.QuitClosure(),
                     result1));
  EXPECT_EQ(1, request_dispatcher_->GetPendingActionCountForTesting());

  // Request from client 2.
  RawResult raw_result2(PredictionStatus::kSucceeded);
  raw_result2.result = CreatePredictionResultWithBinaryClassifier(kTestLabel2);
  EXPECT_CALL(*request_handler2_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result2](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result2);
          }));

  ClassificationResult result2(PredictionStatus::kSucceeded);
  result2.ordered_labels.emplace_back("test_label2");
  request_dispatcher_->GetClassificationResult(
      kAdaptiveToolbarClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), run_loop_2.QuitClosure(),
                     result2));
  EXPECT_EQ(2, request_dispatcher_->GetPendingActionCountForTesting());

  // Finish platform initialization with success. The request queue is posted,
  // but no requests are dispatched because their models are still not yet
  // loaded.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  // Initialize platform, no requests should be executed.
  EXPECT_EQ(2, request_dispatcher_->GetPendingActionCountForTesting());

  // Set the device switcher model as initialized. Its request should be
  // executed.
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER);
  // The device switcher request should be dispatched and
  // the other one gets enqueued again.
  run_loop_1.Run();
  EXPECT_EQ(1, request_dispatcher_->GetPendingActionCountForTesting());

  // Set the new tab model as initialized.
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR);
  // The last request should be dispatched.
  run_loop_2.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

TEST_F(RequestDispatcherTest, TestRequestAfterInitSuccessAndModelsLoaded) {
  base::RunLoop loop;
  PredictionOptions options;
  options.on_demand_execution = true;

  // Init platform.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  // Set platform as initialized.
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  // Set both models as initialized, now requests should be dispatched
  // immediately without queueing.
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER);
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR);

  // Request from client 1.
  RawResult raw_result1(PredictionStatus::kSucceeded);
  raw_result1.result = CreatePredictionResultWithBinaryClassifier(kTestLabel1);
  EXPECT_CALL(*request_handler1_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result1](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result1);
          }));

  ClassificationResult result1(PredictionStatus::kSucceeded);
  result1.ordered_labels.emplace_back(kTestLabel1);
  request_dispatcher_->GetClassificationResult(
      kDeviceSwitcherClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result1));
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());

  // Request from client 2.
  RawResult raw_result2(PredictionStatus::kSucceeded);
  raw_result2.result = CreatePredictionResultWithBinaryClassifier(kTestLabel2);
  EXPECT_CALL(*request_handler2_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result2](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result2);
          }));

  ClassificationResult result2(PredictionStatus::kSucceeded);
  result2.ordered_labels.emplace_back(kTestLabel2);
  request_dispatcher_->GetClassificationResult(
      kAdaptiveToolbarClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result2));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

TEST_F(RequestDispatcherTest, TestAnnotatedNumericResultRequestWithWaiting) {
  base::RunLoop loop;
  PredictionOptions options;
  options.on_demand_execution = true;

  // Request from client 1.
  RawResult raw_result1(PredictionStatus::kSucceeded);
  raw_result1.result = CreatePredictionResultWithGenericPredictor();
  EXPECT_CALL(*request_handler1_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result1](const PredictionOptions& options,
                                scoped_refptr<InputContext> input_context,
                                RawResultCallback callback) {
            std::move(callback).Run(raw_result1);
          }));

  request_dispatcher_->GetAnnotatedNumericResult(
      kDeviceSwitcherClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetAnnotatedNumericResult,
                     base::Unretained(this), loop.QuitClosure(), raw_result1));
  EXPECT_EQ(1, request_dispatcher_->GetPendingActionCountForTesting());

  // Init platform.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));

  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

TEST_F(RequestDispatcherTest, TestOnDemandWithFallback) {
  // Result available in client prefs.
  client_result_prefs_->SaveClientResultToPrefs(
      kDeviceSwitcherKey,
      metadata_utils::CreateClientResultFromPredResult(
          CreatePredictionResultWithBinaryClassifier(kTestLabel1),
          /*timestamp=*/base::Time::Now()));
  auto cached_result_provider = std::make_unique<CachedResultProvider>(
      client_result_prefs_.get(), storage_service_->config_holder()->configs());
  storage_service_->set_cached_result_provider_for_testing(
      std::move(cached_result_provider));

  base::RunLoop loop;
  PredictionOptions options = PredictionOptions::ForOnDemand(true);
  options.can_update_cache_for_future_requests = true;

  // Init platform.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  // Set platform as initialized.
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  // Set both models as initialized, now requests should be dispatched
  // immediately without queueing.
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER);

  // Request from client.
  RawResult raw_result(PredictionStatus::kFailed);
  EXPECT_CALL(*request_handler1_, GetPredictionResult(_, _, _))
      .WillOnce(Invoke([&raw_result](const PredictionOptions& options,
                                     scoped_refptr<InputContext> input_context,
                                     RawResultCallback callback) {
        std::move(callback).Run(raw_result);
      }));

  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(kTestLabel1);
  request_dispatcher_->GetClassificationResult(
      kDeviceSwitcherClient, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

TEST_F(RequestDispatcherTest, TestCachedExecutionWithoutFallback) {
  // Result available in client prefs.
  client_result_prefs_->SaveClientResultToPrefs(
      kShoppingUserSegmentationKey,
      metadata_utils::CreateClientResultFromPredResult(
          CreatePredictionResultWithBinaryClassifier(kTestLabel1),
          /*timestamp=*/base::Time::Now()));
  auto cached_result_provider = std::make_unique<CachedResultProvider>(
      client_result_prefs_.get(), storage_service_->config_holder()->configs());
  storage_service_->set_cached_result_provider_for_testing(
      std::move(cached_result_provider));

  base::RunLoop loop;
  PredictionOptions options = PredictionOptions::ForCached(true);
  options.can_update_cache_for_future_requests = true;

  // Init platform.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  // Set platform as initialized.
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  // Set both models as initialized, now requests should be dispatched
  // immediately without queueing.
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER);

  // Request from client.
  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(kTestLabel1);
  request_dispatcher_->GetClassificationResult(
      kShoppingUserSegmentationKey, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

TEST_F(RequestDispatcherTest, TestCachedExecutionWithFallback) {
  // Result not available in client prefs.
  auto cached_result_provider = std::make_unique<CachedResultProvider>(
      client_result_prefs_.get(), storage_service_->config_holder()->configs());
  storage_service_->set_cached_result_provider_for_testing(
      std::move(cached_result_provider));

  base::RunLoop loop;
  PredictionOptions options = PredictionOptions::ForCached(true);
  options.can_update_cache_for_future_requests = true;

  // Init platform.
  std::map<std::string, std::unique_ptr<SegmentResultProvider>>
      result_providers;
  ExecutionService execution_service;
  // Set platform as initialized.
  request_dispatcher_->OnPlatformInitialized(true, &execution_service,
                                             std::move(result_providers));
  // Set both models as initialized, now requests should be dispatched
  // immediately without queueing.
  request_dispatcher_->OnModelUpdated(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER);

  // Request from client.
  RawResult raw_result(PredictionStatus::kSucceeded);
  raw_result.result = CreatePredictionResultWithBinaryClassifier(kTestLabel1);
  EXPECT_CALL(*request_handler3_, GetPredictionResult(_, _, _))
      .WillRepeatedly(
          Invoke([&raw_result](const PredictionOptions& options,
                               scoped_refptr<InputContext> input_context,
                               RawResultCallback callback) {
            std::move(callback).Run(raw_result);
          }));

  ClassificationResult result(PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(kTestLabel1);
  request_dispatcher_->GetClassificationResult(
      kShoppingUserSegmentationKey, options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestDispatcherTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), result));
  loop.Run();
  EXPECT_EQ(0, request_dispatcher_->GetPendingActionCountForTesting());
}

}  // namespace
}  // namespace segmentation_platform
