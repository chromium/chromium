// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv4_handler.h"

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/inference/test_model_handler.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/permissions/prediction_service/permissions_ai_encoder_base.h"
#include "components/permissions/prediction_service/permissions_aiv4_encoder.h"
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
using ::testing::ValuesIn;
using ModelInput = PermissionsAiv4Handler::ModelInput;

constexpr OptimizationTarget kOptTargetNotifications = OptimizationTarget::
    OPTIMIZATION_TARGET_PERMISSIONS_AIV4_NOTIFICATIONS_DESKTOP;

constexpr std::string_view kZeroReturnModel = "aiv4_ret_0.tflite";
constexpr std::string_view k0_023ReturnModel = "aiv4_ret_0_023.tflite";
constexpr std::string_view kOneReturnModel = "aiv4_ret_1.tflite";

constexpr SkColor kDefaultColor = SkColorSetRGB(0x1E, 0x1C, 0x0F);

auto kImageInputWidth = PermissionsAiv4Encoder::kImageInputWidth;
auto kImageInputHeight = PermissionsAiv4Encoder::kImageInputHeight;
auto kTextInputSize = PermissionsAiv4Encoder::kTextInputSize;

constexpr char kModelExecutionTimeoutHistogram[] =
    "Permissions.AIv4.ModelExecutionTimeout";

passage_embeddings::Embedding GetDummyEmbeddings(
    int input_size = kTextInputSize) {
  return passage_embeddings::Embedding(
      /*data=*/std::vector<float>(input_size, 42.f),
      /*passage_word_count=*/42);
}

class PermissionsAiv4EncoderFake : public PermissionsAiv4Encoder {
 public:
  explicit PermissionsAiv4EncoderFake(RequestType type)
      : PermissionsAiv4Encoder(type) {}

  void set_preprocess_hook(
      base::OnceCallback<void(const std::vector<TfLiteTensor*>& input_tensors)>
          hook) {
    preprocess_hook_ = std::move(hook);
  }

  void set_postprocess_hook(
      base::OnceCallback<
          void(const std::vector<const TfLiteTensor*>& output_tensors)> hook) {
    postprocess_hook_ = std::move(hook);
  }

 protected:
  std::optional<PermissionRequestRelevance> relevance_;
  base::OnceCallback<void(const std::vector<TfLiteTensor*>& input_tensors)>
      preprocess_hook_;
  base::OnceCallback<void(
      const std::vector<const TfLiteTensor*>& output_tensors)>
      postprocess_hook_;

  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override {
    auto ret = PermissionsAiv4Encoder::Preprocess(input_tensors, input);
    if (preprocess_hook_) {
      std::move(preprocess_hook_).Run(input_tensors);
    }
    return ret;
  }

  std::optional<PermissionsAiv4Encoder::ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override {
    if (postprocess_hook_) {
      std::move(postprocess_hook_).Run(output_tensors);
    }

    return PermissionsAiv4Encoder::Postprocess(output_tensors);
  }
};

class PermissionsAiv4HandlerMock : public PermissionsAiv4Handler {
 public:
  PermissionsAiv4HandlerMock(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type,
      std::unique_ptr<PermissionsAiv4Encoder> model_executor)
      : PermissionsAiv4Handler(model_provider,
                               optimization_target,
                               request_type,
                               std::move(model_executor)) {}

  // This is a mock implementation of ExecuteModelWithInput that does not
  // schedule the real model execution but captures the callback. This gives the
  // test control over the duration of the model execution and can be used to
  // simulate the model execution being stuck (or simply too long).
  void ExecuteModelWithInput(
      ExecutionCallback callback,
      const PermissionsAiv4Encoder::ModelInput& input) override {
    callback_ = std::move(callback);
  }

  void ReleaseCallback(PermissionRequestRelevance relevance) {
    EXPECT_TRUE(callback_);
    std::move(callback_).Run(relevance);
  }

 private:
  ExecutionCallback callback_;
};

class Aiv4HandlerTestBase : public testing::Test {
 public:
  Aiv4HandlerTestBase() = default;
  ~Aiv4HandlerTestBase() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();

    auto notification_encoder_mock =
        std::make_unique<PermissionsAiv4EncoderFake>(
            RequestType::kNotifications);
    notification_encoder_mock_ = notification_encoder_mock.get();
    notification_model_handler_ = std::make_unique<PermissionsAiv4Handler>(
        model_provider_.get(),
        /*optimization_target=*/kOptTargetNotifications,
        /*request_type=*/RequestType::kNotifications,
        std::move(notification_encoder_mock));
  }

  void TearDown() override {
    notification_encoder_mock_ = nullptr;
    notification_model_handler_.reset();
    model_provider_.reset();
    task_environment_.RunUntilIdle();
  }

  void PushModelFileToModelExecutor(OptimizationTarget opt_target,
                                    const base::FilePath& model_file_path) {
    auto model_metadata = optimization_guide::TestModelInfoBuilder()
                              .SetModelFilePath(model_file_path)
                              .SetVersion(123)
                              .Build();

    model_handler()->OnModelUpdated(opt_target, *model_metadata);

    task_environment_.RunUntilIdle();
  }

  PermissionsAiv4Handler* model_handler() {
    return notification_model_handler_.get();
  }

  optimization_guide::TestOptimizationGuideModelProvider* GetModelProvider() {
    return model_provider_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 protected:
  raw_ptr<PermissionsAiv4EncoderFake> notification_encoder_mock_;
  std::unique_ptr<PermissionsAiv4Handler> notification_model_handler_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

class Aiv4HandlerTest : public Aiv4HandlerTestBase {};

struct RelevanceTestCase {
  OptimizationTarget optimization_target;
  base::FilePath model_file_path;
  float expected_model_return_value;
  PermissionRequestRelevance expected_relevance;
};

class RelevanceAiv4HandlerTest
    : public Aiv4HandlerTestBase,
      public testing::WithParamInterface<RelevanceTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ModelResults,
    RelevanceAiv4HandlerTest,
    ValuesIn<RelevanceTestCase>({
        {kOptTargetNotifications, test::ModelFilePath(kZeroReturnModel),
         /*expected_model_return_value=*/0.0f,
         PermissionRequestRelevance::kVeryLow},
        {kOptTargetNotifications, test::ModelFilePath(k0_023ReturnModel),
         /*expected_model_return_value=*/0.023f,
         PermissionRequestRelevance::kLow},
        {kOptTargetNotifications, test::ModelFilePath(kOneReturnModel),
         /*expected_model_return_value=*/1.0f,
         PermissionRequestRelevance::kVeryHigh},
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<RelevanceAiv4HandlerTest::ParamType>&
           info) {
      return base::StrCat({"NotificationsModelReturns",
                           test::ToString(info.param.expected_relevance)});
    });

TEST_P(RelevanceAiv4HandlerTest,
       RelevanceIsMatchedToTheCorrectModelThresholds) {
  PushModelFileToModelExecutor(GetParam().optimization_target,
                               GetParam().model_file_path);
  auto* aiv4_handler = model_handler();
  EXPECT_TRUE(aiv4_handler->ModelAvailable());

  bool flag = false;
  notification_encoder_mock_->set_postprocess_hook(base::BindLambdaForTesting(
      [&flag](const std::vector<const TfLiteTensor*>& output_tensors) {
        std::vector<float> data;
        EXPECT_TRUE(
            tflite::task::core::PopulateVector<float>(output_tensors[0], &data)
                .ok());
        EXPECT_FLOAT_EQ(data[0], GetParam().expected_model_return_value);
        flag = true;
      }));

  ModelCallbackFuture future;
  aiv4_handler->ExecuteModel(
      future.GetCallback(),
      /*model_input=*/
      ModelInput{/*snapshot=*/test::BuildBitmap(
                     kImageInputWidth, kImageInputHeight, kDefaultColor),
                 /*rendered_text_embedding=*/GetDummyEmbeddings()});
  EXPECT_EQ(future.Take(), GetParam().expected_relevance);
  EXPECT_TRUE(flag);
}

TEST_F(Aiv4HandlerTest, BitmapGetsCopiedToTensor) {
  PushModelFileToModelExecutor(kOptTargetNotifications,
                               test::ModelFilePath(kZeroReturnModel));

  auto snapshot =
      test::BuildBitmap(kImageInputWidth, kImageInputHeight, kDefaultColor);

  bool flag = false;
  notification_encoder_mock_->set_preprocess_hook(base::BindLambdaForTesting(
      [&flag](const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        ASSERT_TRUE(
            tflite::task::core::PopulateVector<float>(input_tensors[1], &data)
                .ok());
        EXPECT_THAT(data, SizeIs(kImageInputWidth * kImageInputHeight * 3));
        for (int i = 0; i < kImageInputWidth * kImageInputHeight; i += 3) {
          EXPECT_FLOAT_EQ(data[i], SkColorGetR(kDefaultColor) / 255.0f);
          EXPECT_FLOAT_EQ(data[i + 1], SkColorGetG(kDefaultColor) / 255.0f);
          EXPECT_FLOAT_EQ(data[i + 2], SkColorGetB(kDefaultColor) / 255.0f);
        }
        flag = true;
      }));

  ModelCallbackFuture future;
  auto* aiv4_handler = model_handler();
  aiv4_handler->ExecuteModel(
      future.GetCallback(),
      ModelInput{std::move(snapshot), GetDummyEmbeddings()});
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

// This test verifies the timeout behavior of the permission model handler.
// The timeout is triggered when the model execution takes longer than the
// timeout threshold. Additionally, this test verifies that the model handler
// prevents concurrent executions after the timeout is triggered and before the
// first execution is completed.
TEST_F(Aiv4HandlerTest, ModelHandlerTimeoutExecutions) {
  base::HistogramTester histograms;

  auto geolocation_encoder_mock =
      std::make_unique<PermissionsAiv4EncoderFake>(RequestType::kGeolocation);
  std::unique_ptr<PermissionsAiv4HandlerMock> model_handler_mock =
      std::make_unique<PermissionsAiv4HandlerMock>(
          GetModelProvider(),
          /*optimization_target=*/kOptTargetNotifications,
          /*request_type=*/RequestType::kNotifications,
          std::move(geolocation_encoder_mock));

  // Because of `PermissionsAiv3EncoderFake` the first execution will be hold
  // until manually released. In this case we release the callback before we
  // try to execute the model again.
  ModelCallbackFuture future1;
  // The image size is arbitrary and does not affect the test.
  auto snapshot1 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(
      future1.GetCallback(),
      ModelInput{std::move(snapshot1), GetDummyEmbeddings()});

  task_environment().FastForwardBy(
      base::Seconds(PermissionsAiv4Handler::kModelExecutionTimeout + 1));

  // Because the execution took longer than the timeout, the execution should
  // return `std::nullopt` result even without manually releasing the callback.
  EXPECT_EQ(future1.Take(), std::nullopt);

  // The second execution should return an empty response because the model is
  // still busy with the first execution.
  ModelCallbackFuture future2;
  // The image size is arbitrary and does not affect the test.
  auto snapshot2 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(
      future2.GetCallback(),
      ModelInput{std::move(snapshot2), GetDummyEmbeddings()});

  EXPECT_EQ(future2.Take(), std::nullopt);

  // This will resets the flags that prevent concurrent executions. `kVeryLow`
  // will not be returned because the callback was released after the timeout.
  model_handler_mock->ReleaseCallback(PermissionRequestRelevance::kVeryLow);

  ModelCallbackFuture future3;
  // The image size is arbitrary and does not affect the test.
  auto snapshot3 =
      test::BuildBitmap(/*width=*/32, /*height=*/32, kDefaultColor);
  model_handler_mock->ExecuteModel(
      future3.GetCallback(),
      ModelInput{std::move(snapshot3), GetDummyEmbeddings()});

  // Because all flags are reset, the execution will not timeout and the
  // correct relevance will be returned.
  model_handler_mock->ReleaseCallback(PermissionRequestRelevance::kVeryLow);

  EXPECT_EQ(future3.Take(), PermissionRequestRelevance::kVeryLow);

  histograms.ExpectBucketCount(kModelExecutionTimeoutHistogram, true, 1u);
}

TEST_F(Aiv4HandlerTest, TextEmbeddingGetsCopiedToTensor) {
  PushModelFileToModelExecutor(kOptTargetNotifications,
                               test::ModelFilePath(kZeroReturnModel));

  auto snapshot =
      test::BuildBitmap(kImageInputWidth, kImageInputHeight, kDefaultColor);

  bool flag = false;
  notification_encoder_mock_->set_preprocess_hook(base::BindLambdaForTesting(
      [&flag](const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        ASSERT_TRUE(
            tflite::task::core::PopulateVector<float>(input_tensors[0], &data)
                .ok());
        EXPECT_THAT(data, SizeIs(kTextInputSize));
        for (int i = 0; i < kTextInputSize; i++) {
          EXPECT_FLOAT_EQ(data[i], 42.f);
        }
        flag = true;
      }));

  ModelCallbackFuture future;
  auto* aiv4_handler = model_handler();
  aiv4_handler->ExecuteModel(
      future.GetCallback(),
      ModelInput{std::move(snapshot), GetDummyEmbeddings()});
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

TEST_F(Aiv4HandlerTest, TextEmbeddingSizeDoesNotMatchAiv4InputSize) {
  PushModelFileToModelExecutor(kOptTargetNotifications,
                               test::ModelFilePath(kZeroReturnModel));

  auto snapshot =
      test::BuildBitmap(kImageInputWidth, kImageInputHeight, kDefaultColor);

  ModelCallbackFuture future;
  auto* aiv4_handler = model_handler();
  aiv4_handler->ExecuteModel(
      future.GetCallback(),
      ModelInput{std::move(snapshot), GetDummyEmbeddings(/*input_size=*/42)});

  // We do not execute the model and call the callback with nullopt if input
  // size does not match expectations.
  EXPECT_EQ(future.Take(), std::nullopt);
}

}  // namespace
}  // namespace permissions
