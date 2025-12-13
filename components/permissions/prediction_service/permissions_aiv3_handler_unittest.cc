// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_handler.h"

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/optimization_guide/core/inference/test_model_handler.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/permissions/prediction_service/permissions_ai_encoder_base.h"
#include "components/permissions/prediction_service/permissions_aiv3_executor.h"
#include "components/permissions/prediction_service/permissions_aiv3_model_metadata.pb.h"
#include "components/permissions/test/aivx_modelhandler_utils.h"
#include "components/permissions/test/enums_to_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace permissions {
namespace {
using ModelCallbackFuture =
    ::base::test::TestFuture<const std::optional<PermissionRequestRelevance>&>;
using ::optimization_guide::proto::OptimizationTarget;
using ::testing::SizeIs;
using ModelInput = PermissionsAiv3Handler::ModelInput;

constexpr OptimizationTarget kOptTargetGeolocation = OptimizationTarget::
    OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE;
constexpr OptimizationTarget kOptTargetNotification = OptimizationTarget::
    OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE;

constexpr std::string_view kZeroReturnModel = "aiv3_ret_0.tflite";
constexpr std::string_view kZeroDotFiveReturnModel = "aiv3_ret_0_5.tflite";
constexpr std::string_view kOneReturnModel = "aiv3_ret_1.tflite";

constexpr SkColor kDefaultColor = SkColorSetRGB(0x1E, 0x1C, 0x0F);

auto& kImageInputWidth = PermissionsAiv3Executor::kImageInputWidth;
auto& kImageInputHeight = PermissionsAiv3Executor::kImageInputHeight;

constexpr char kModelExecutionAlreadyInProgressHistogram[] =
    "Permissions.AIv3.ModelExecutionAlreadyInProgress";

constexpr char kModelExecutionTimeoutHistogram[] =
    "Permissions.AIv3.ModelExecutionTimeout";

PermissionsAiv3ModelMetadata BuildMetadataFromValues(
    const std::array<float, 4>& thresholds) {
  PermissionsAiv3ModelMetadata metadata;
  std::string serialized_metadata;
  metadata.mutable_relevance_thresholds()->set_min_low_relevance(thresholds[0]);
  metadata.mutable_relevance_thresholds()->set_min_medium_relevance(
      thresholds[1]);
  metadata.mutable_relevance_thresholds()->set_min_high_relevance(
      thresholds[2]);
  metadata.mutable_relevance_thresholds()->set_min_very_high_relevance(
      thresholds[3]);
  return metadata;
}

class PermissionsAiv3HandlerMock : public PermissionsAiv3Handler {
 public:
  PermissionsAiv3HandlerMock(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type,
      std::unique_ptr<PermissionsAiv3Executor> model_executor)
      : PermissionsAiv3Handler(model_provider,
                               optimization_target,
                               request_type,
                               std::move(model_executor)) {}

  // This is a mock implementation of ExecuteModelWithInput that does not
  // schedule the real model execution but captures the callback. This gives the
  // test control over the duration of the model execution and can be used to
  // simulate the model execution being stuck (or simply too long).
  void ExecuteModelWithInput(
      ExecutionCallback callback,
      const PermissionsAiv3Executor::ModelInput& input) override {
    callback_ = std::move(callback);
  }

  void ReleaseCallback(PermissionRequestRelevance relevance) {
    EXPECT_TRUE(callback_);
    std::move(callback_).Run(relevance);
  }

 private:
  ExecutionCallback callback_;
};

class PermissionsAiv3ExecutorFake : public PermissionsAiv3Executor {
 public:
  explicit PermissionsAiv3ExecutorFake(RequestType type)
      : PermissionsAiv3Executor(type) {}

  void set_preprocess_hook(
      base::OnceCallback<void(const std::vector<TfLiteTensor*>& input_tensors)>
          hook) {
    preprocess_hook_ = std::move(hook);
  }

  std::optional<PermissionRequestRelevance> relevance_;
  base::OnceCallback<void(const std::vector<TfLiteTensor*>& input_tensors)>
      preprocess_hook_;

 protected:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override {
    auto ret = PermissionsAiv3Executor::Preprocess(input_tensors, input);
    if (preprocess_hook_) {
      std::move(preprocess_hook_).Run(input_tensors);
    }
    return ret;
  }
};

class Aiv3HandlerTestBase : public testing::Test {
 public:
  Aiv3HandlerTestBase() = default;
  ~Aiv3HandlerTestBase() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();

    auto geolocation_executor_mock =
        std::make_unique<PermissionsAiv3ExecutorFake>(
            RequestType::kGeolocation);
    geolocation_executor_mock_ = geolocation_executor_mock.get();
    geolocation_model_handler_ = std::make_unique<PermissionsAiv3Handler>(
        model_provider_.get(),
        /*optimization_target=*/kOptTargetGeolocation,
        /*request_type=*/RequestType::kGeolocation,
        std::move(geolocation_executor_mock));

    auto notification_executor_mock =
        std::make_unique<PermissionsAiv3ExecutorFake>(
            RequestType::kNotifications);
    notification_executor_mock_ = notification_executor_mock.get();
    notification_model_handler_ = std::make_unique<PermissionsAiv3Handler>(
        model_provider_.get(),
        /*optimization_target=*/kOptTargetNotification,
        /*request_type=*/RequestType::kNotifications,
        std::move(notification_executor_mock));
  }

  void TearDown() override {
    geolocation_executor_mock_ = nullptr;
    notification_executor_mock_ = nullptr;
    geolocation_model_handler_.reset();
    notification_model_handler_.reset();
    model_provider_.reset();
    task_environment_.RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      OptimizationTarget opt_target,
      const base::FilePath& model_file_path,
      std::optional<PermissionsAiv3ModelMetadata> metadata = std::nullopt) {
    std::optional<optimization_guide::proto::Any> any;

    if (metadata.has_value()) {
      any = std::make_optional<optimization_guide::proto::Any>();
      std::string serialized_metadata;
      (metadata.value()).SerializeToString(&serialized_metadata);
      any->set_value(serialized_metadata);
      any->set_type_url(
          "type.googleapis.com/"
          "google.privacy.webpermissionpredictions.aiv3.v1."
          "PermissionsAiv3ModelMetadata");
    }

    auto model_metadata = optimization_guide::TestModelInfoBuilder()
                              .SetModelMetadata(any)
                              .SetModelFilePath(model_file_path)
                              .SetVersion(123)
                              .Build();

    model_handler(opt_target)->OnModelUpdated(opt_target, *model_metadata);

    task_environment_.RunUntilIdle();
  }

  PermissionsAiv3Handler* model_handler(OptimizationTarget target) {
    return target == kOptTargetGeolocation ? geolocation_model_handler_.get()
                                           : notification_model_handler_.get();
  }

  optimization_guide::TestOptimizationGuideModelProvider* GetModelProvider() {
    return model_provider_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 protected:
  raw_ptr<PermissionsAiv3ExecutorFake> geolocation_executor_mock_;
  raw_ptr<PermissionsAiv3ExecutorFake> notification_executor_mock_;

  std::unique_ptr<PermissionsAiv3Handler> geolocation_model_handler_;
  std::unique_ptr<PermissionsAiv3Handler> notification_model_handler_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

class Aiv3HandlerTest : public Aiv3HandlerTestBase {};

struct RelevanceTestCase {
  OptimizationTarget optimization_target;
  base::FilePath model_file_path;
  PermissionRequestRelevance expected_relevance;
  std::optional<PermissionsAiv3ModelMetadata> metadata;
};

class RelevanceAiv3HandlerTest
    : public Aiv3HandlerTestBase,
      public testing::WithParamInterface<RelevanceTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ModelResults,
    RelevanceAiv3HandlerTest,
    testing::ValuesIn<RelevanceTestCase>({
        {kOptTargetGeolocation, test::ModelFilePath(kZeroReturnModel),
         PermissionRequestRelevance::kVeryLow, /*metadata=*/std::nullopt},
        {kOptTargetGeolocation, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kHigh, /*metadata=*/std::nullopt},
        {kOptTargetGeolocation, test::ModelFilePath(kOneReturnModel),
         PermissionRequestRelevance::kVeryHigh, /*metadata=*/std::nullopt},
        {kOptTargetNotification, test::ModelFilePath(kZeroReturnModel),
         PermissionRequestRelevance::kVeryLow, /*metadata=*/std::nullopt},
        {kOptTargetNotification, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kMedium, /*metadata=*/std::nullopt},
        {kOptTargetNotification, test::ModelFilePath(kOneReturnModel),
         PermissionRequestRelevance::kVeryHigh, /*metadata=*/std::nullopt},
        {kOptTargetGeolocation, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kVeryLow,
         BuildMetadataFromValues({0.6, 0.7, 0.8, 0.9})},
        {kOptTargetNotification, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kLow,
         BuildMetadataFromValues({0.5, 0.6, 0.7, 0.8})},
        {kOptTargetNotification, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kMedium,
         BuildMetadataFromValues({0.4, 0.5, 0.6, 0.7})},
        {kOptTargetGeolocation, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kHigh,
         BuildMetadataFromValues({0.3, 0.4, 0.5, 0.6})},
        {kOptTargetNotification, test::ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kVeryHigh,
         BuildMetadataFromValues({0.2, 0.3, 0.4, 0.5})},
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<RelevanceAiv3HandlerTest::ParamType>&
           info) {
      return base::StrCat(
          {"With",
           info.param.metadata.has_value() ? "Metadata" : "DefaultThresholds",
           info.param.optimization_target == kOptTargetGeolocation
               ? "Geolocation"
               : "Notifications",
           "ModelReturns", test::ToString(info.param.expected_relevance)});
    });

TEST_P(RelevanceAiv3HandlerTest,
       RelevanceIsMatchedToTheCorrectModelThresholds) {
  PushModelFileToModelExecutor(GetParam().optimization_target,
                               GetParam().model_file_path, GetParam().metadata);
  auto* aiv3_handler = model_handler(GetParam().optimization_target);
  EXPECT_TRUE(aiv3_handler->ModelAvailable());

  ModelCallbackFuture future;
  aiv3_handler->ExecuteModel(
      future.GetCallback(),
      ModelInput{/*snapshot=*/test::BuildBitmap(
          kImageInputWidth, kImageInputHeight, kDefaultColor)});
  EXPECT_EQ(future.Take(), GetParam().expected_relevance);
}

TEST_F(Aiv3HandlerTest, BitmapGetsCopiedToTensor) {
  PushModelFileToModelExecutor(kOptTargetGeolocation,
                               test::ModelFilePath(kZeroReturnModel));

  auto snapshot =
      test::BuildBitmap(kImageInputWidth, kImageInputHeight, kDefaultColor);

  bool flag = false;
  geolocation_executor_mock_->set_preprocess_hook(base::BindOnce(
      [](bool* flag, const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        if (tflite::task::core::PopulateVector<float>(input_tensors[0], &data)
                .ok()) {
          EXPECT_THAT(data, SizeIs(kImageInputWidth * kImageInputHeight * 3));
          for (int i = 0; i < kImageInputWidth * kImageInputHeight; i += 3) {
            EXPECT_FLOAT_EQ(data[i], SkColorGetR(kDefaultColor) / 255.0f);
            EXPECT_FLOAT_EQ(data[i + 1], SkColorGetG(kDefaultColor) / 255.0f);
            EXPECT_FLOAT_EQ(data[i + 2], SkColorGetB(kDefaultColor) / 255.0f);
          }
        }
        *flag = true;
      },
      &flag));

  ModelCallbackFuture future;
  auto* aiv3_handler = model_handler(kOptTargetGeolocation);
  aiv3_handler->ExecuteModel(future.GetCallback(),
                             ModelInput{std::move(snapshot)});
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

struct ResizeTestCase {
  int input_width;
  int input_height;
};

class ResizeAiv3HandlerTest
    : public Aiv3HandlerTestBase,
      public testing::WithParamInterface<ResizeTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ResizeBitmapInternally,
    ResizeAiv3HandlerTest,
    testing::ValuesIn<ResizeTestCase>({
        {/*input_width=*/32, /*input_height=*/32},
        {/*input_width=*/32, /*input_height=*/64},
        {/*input_width=*/64, /*input_height=*/32},
        {/*input_width=*/128, /*input_height=*/128},
        {/*input_width=*/64, /*input_height=*/128},
        {/*input_width=*/128, /*input_height=*/64},
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<ResizeAiv3HandlerTest::ParamType>& info) {
      return base::StrCat({base::NumberToString(info.param.input_width), "x",
                           base::NumberToString(info.param.input_height),
                           "To64x64"});
    });

TEST_P(ResizeAiv3HandlerTest, ResizesBitmapsForModelInput) {
  PushModelFileToModelExecutor(kOptTargetGeolocation,
                               test::ModelFilePath(kZeroReturnModel));

  auto snapshot = test::BuildBitmap(GetParam().input_width,
                                    GetParam().input_height, kDefaultColor);

  bool flag = false;
  geolocation_executor_mock_->set_preprocess_hook(base::BindOnce(
      [](bool* flag, const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        ASSERT_TRUE(
            tflite::task::core::PopulateVector<float>(input_tensors[0], &data)
                .ok());
        EXPECT_THAT(data, SizeIs(kImageInputWidth * kImageInputHeight * 3));
        for (int i = 0; i < kImageInputWidth * kImageInputHeight * 3; ++i) {
          EXPECT_FALSE(std::isnan(data[i]));
        }
        *flag = true;
      },
      &flag));

  ModelCallbackFuture future;
  auto* aiv3_handler = model_handler(kOptTargetGeolocation);
  aiv3_handler->ExecuteModel(future.GetCallback(),
                             ModelInput{std::move(snapshot)});
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

// This test verifies an edge case when the permission model handler receives
// multiple overlapping request for the on-device model execution. Multiple
// requests means that the UI was updated faster than the model produces content
// evaluation. In other words the callback that is stored in the model execution
// class refers to a stailed UI and the result of the execution should be
// ignored.
TEST_F(Aiv3HandlerTest, ModelHandlerPreventsConcurrentExecutions) {
  base::HistogramTester histograms;

  auto geolocation_encoder_mock =
      std::make_unique<PermissionsAiv3ExecutorFake>(RequestType::kGeolocation);
  std::unique_ptr<PermissionsAiv3HandlerMock> model_handler_mock =
      std::make_unique<PermissionsAiv3HandlerMock>(
          GetModelProvider(),
          /*optimization_target=*/kOptTargetGeolocation,
          /*request_type=*/RequestType::kGeolocation,
          std::move(geolocation_encoder_mock));

  // Because of `PermissionsAiv3ExecutorFake` the first execution will be hold
  // until manually released to simulate a long execution so that we can test
  // the concurrent execution prevention logic.
  ModelCallbackFuture future1;
  // The image size is arbitrary and does not affect the test.
  auto snapshot1 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(future1.GetCallback(),
                                   ModelInput{std::move(snapshot1)});

  // Request the second model execution while the first one is still in
  // progress. The second execution should be cancelled with `std::nullopt`
  // result.
  ModelCallbackFuture future2;
  // The image size is arbitrary and does not affect the test.
  auto snapshot2 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(future2.GetCallback(),
                                   ModelInput{std::move(snapshot2)});
  EXPECT_EQ(future2.Take(), std::nullopt);

  // Any return value is OK as it should be ignored and replaced with
  // `std::nullopt`.
  model_handler_mock->ReleaseCallback(PermissionRequestRelevance::kUnspecified);
  // Because the callback was released after the second execution was requested,
  // the first execution should return `std::nullopt` result.
  EXPECT_EQ(future1.Take(), std::nullopt);

  histograms.ExpectBucketCount(
      "Permissions.AIv3.ModelExecutionAlreadyInProgress", true, 1u);

  histograms.ExpectBucketCount(kModelExecutionAlreadyInProgressHistogram, false,
                               1u);
}

// This test verifies the default behavior of the permission model handler
// without any concurrent executions. This is needed to make sure there is no
// regression in the default behavior and a correct value is properly delivered
// from the on-device model to the callback.
TEST_F(Aiv3HandlerTest, ModelHandlerSingleExecutions) {
  base::HistogramTester histograms;

  auto geolocation_encoder_mock =
      std::make_unique<PermissionsAiv3ExecutorFake>(RequestType::kGeolocation);
  std::unique_ptr<PermissionsAiv3HandlerMock> model_handler_mock =
      std::make_unique<PermissionsAiv3HandlerMock>(
          GetModelProvider(),
          /*optimization_target=*/kOptTargetGeolocation,
          /*request_type=*/RequestType::kGeolocation,
          std::move(geolocation_encoder_mock));

  // Because of `PermissionsAiv3ExecutorFake` the first execution will be hold
  // until manually released. In this case we release the callback before we
  // try to execute the model again.
  ModelCallbackFuture future1;
  // The image size is arbitrary and does not affect the test.
  auto snapshot1 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(future1.GetCallback(),
                                   ModelInput{std::move(snapshot1)});

  // The manual release without a concurrent request should return the
  // correct relevance.
  model_handler_mock->ReleaseCallback(PermissionRequestRelevance::kVeryLow);
  // Because the callback was released uninterrupted, the execution should
  // return the correct result.
  EXPECT_EQ(future1.Take(), PermissionRequestRelevance::kVeryLow);

  histograms.ExpectBucketCount(kModelExecutionAlreadyInProgressHistogram, true,
                               0u);

  histograms.ExpectBucketCount(kModelExecutionAlreadyInProgressHistogram, false,
                               1u);
}

// This test verifies the timeout behavior of the permission model handler.
// The timeout is triggered when the model execution takes longer than the
// timeout threshold. Additionally, this test verifies that the model handler
// prevents concurrent executions after the timeout is triggered and before the
// first execution is completed.
TEST_F(Aiv3HandlerTest, ModelHandlerTimeoutExecutions) {
  base::HistogramTester histograms;

  auto geolocation_encoder_mock =
      std::make_unique<PermissionsAiv3ExecutorFake>(RequestType::kGeolocation);
  std::unique_ptr<PermissionsAiv3HandlerMock> model_handler_mock =
      std::make_unique<PermissionsAiv3HandlerMock>(
          GetModelProvider(),
          /*optimization_target=*/kOptTargetGeolocation,
          /*request_type=*/RequestType::kGeolocation,
          std::move(geolocation_encoder_mock));

  // Because of `PermissionsAiv3ExecutorFake` the first execution will be hold
  // until manually released. In this case we release the callback before we
  // try to execute the model again.
  ModelCallbackFuture future1;
  // The image size is arbitrary and does not affect the test.
  auto snapshot1 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(future1.GetCallback(),
                                   ModelInput{std::move(snapshot1)});

  task_environment().FastForwardBy(
      base::Seconds(PermissionsAiv3Handler::kModelExecutionTimeout + 1));

  // Because the execution took longer than the timeout, the execution should
  // return `std::nullopt` result even without manually releasing the callback.
  EXPECT_EQ(future1.Take(), std::nullopt);

  // The second execution should return an empty response because the model is
  // still busy with the first execution.
  ModelCallbackFuture future2;
  // The image size is arbitrary and does not affect the test.
  auto snapshot2 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(future2.GetCallback(),
                                   ModelInput{std::move(snapshot2)});

  EXPECT_EQ(future2.Take(), std::nullopt);

  // This will resets the flags that prevent concurrent executions. `kVeryLow`
  // will not be returned because the callback was released after the timeout.
  model_handler_mock->ReleaseCallback(PermissionRequestRelevance::kVeryLow);

  ModelCallbackFuture future3;
  // The image size is arbitrary and does not affect the test.
  auto snapshot3 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(future3.GetCallback(),
                                   ModelInput{std::move(snapshot3)});

  // Because all flags are reset, the execution will not timeout and the
  // correct relevance will be returned.
  model_handler_mock->ReleaseCallback(PermissionRequestRelevance::kVeryLow);

  EXPECT_EQ(future3.Take(), PermissionRequestRelevance::kVeryLow);

  histograms.ExpectBucketCount(kModelExecutionTimeoutHistogram, true, 1u);
}

}  // namespace
}  // namespace permissions
