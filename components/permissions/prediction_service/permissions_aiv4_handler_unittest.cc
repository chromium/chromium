// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv4_handler.h"

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
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

// TODO(crbug.com/382447738) It does not matter for this test to work, but lets
// add the correct target as soon as we have it.
constexpr OptimizationTarget kOptTargetNotification = OptimizationTarget::
    OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE;

constexpr std::string_view kZeroReturnModel = "aiv4_ret_0.tflite";
constexpr std::string_view k0_023ReturnModel = "aiv4_ret_0_023.tflite";
constexpr std::string_view kOneReturnModel = "aiv4_ret_1.tflite";

constexpr SkColor kDefaultColor = SkColorSetRGB(0x1E, 0x1C, 0x0F);

auto& kImageInputWidth = PermissionsAiv4Encoder::kImageInputWidth;
auto& kImageInputHeight = PermissionsAiv4Encoder::kImageInputHeight;

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
        /*optimization_target=*/kOptTargetNotification,
        /*request_type=*/RequestType::kNotifications,
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
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

 protected:
  raw_ptr<PermissionsAiv4EncoderFake> notification_encoder_mock_;
  std::unique_ptr<PermissionsAiv4Handler> notification_model_handler_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  base::test::TaskEnvironment task_environment_;
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
        {kOptTargetNotification, test::ModelFilePath(kZeroReturnModel),
         /*expected_model_return_value=*/0.0f,
         PermissionRequestRelevance::kVeryLow},
        {kOptTargetNotification, test::ModelFilePath(k0_023ReturnModel),
         /*expected_model_return_value=*/0.023f,
         PermissionRequestRelevance::kLow},
        {kOptTargetNotification, test::ModelFilePath(kOneReturnModel),
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
      /*snapshot=*/
      test::BuildBitmap(kImageInputWidth, kImageInputHeight, kDefaultColor),
      /*rendered_text=*/"dummy");
  EXPECT_EQ(future.Take(), GetParam().expected_relevance);
  EXPECT_TRUE(flag);
}

TEST_F(Aiv4HandlerTest, BitmapGetsCopiedToTensor) {
  PushModelFileToModelExecutor(kOptTargetNotification,
                               test::ModelFilePath(kZeroReturnModel));

  auto snapshot =
      test::BuildBitmap(kImageInputWidth, kImageInputHeight, kDefaultColor);

  bool flag = false;
  notification_encoder_mock_->set_preprocess_hook(base::BindLambdaForTesting(
      [&flag](const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        if (tflite::task::core::PopulateVector<float>(input_tensors[1], &data)
                .ok()) {
          EXPECT_THAT(data, SizeIs(kImageInputWidth * kImageInputHeight * 3));
          for (int i = 0; i < kImageInputWidth * kImageInputHeight; i += 3) {
            EXPECT_FLOAT_EQ(data[i], SkColorGetR(kDefaultColor) / 255.0f);
            EXPECT_FLOAT_EQ(data[i + 1], SkColorGetG(kDefaultColor) / 255.0f);
            EXPECT_FLOAT_EQ(data[i + 2], SkColorGetB(kDefaultColor) / 255.0f);
          }
        }
        flag = true;
      }));

  ModelCallbackFuture future;
  auto* aiv4_handler = model_handler();
  aiv4_handler->ExecuteModel(future.GetCallback(), std::move(snapshot),
                             "dummy");
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

}  // namespace
}  // namespace permissions
