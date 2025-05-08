// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/permissions_aiv3_handler.h"

#include "base/path_service.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/test_model_handler.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/permissions/prediction_service/permissions_aiv3_encoder.h"
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

constexpr OptimizationTarget kOptTargetGeolocation = OptimizationTarget::
    OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE;
constexpr OptimizationTarget kOptTargetNotification = OptimizationTarget::
    OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE;

constexpr std::string_view kZeroReturnModel = "aiv3_ret_0.tflite";
constexpr std::string_view kZeroDotFiveReturnModel = "aiv3_ret_0_5.tflite";
constexpr std::string_view kOneReturnModel = "aiv3_ret_1.tflite";

constexpr SkColor kDefaultColor = SkColorSetRGB(0x1E, 0x1C, 0x0F);

auto& kModelInputWidth = PermissionsAiv3Encoder::kModelInputWidth;
auto& kModelInputHeight = PermissionsAiv3Encoder::kModelInputHeight;

base::FilePath ModelFilePath(std::string_view file_name) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("permissions")
      .AppendASCII(file_name);
}

void FillDataToBitmap(SkBitmap* bmp) {}

std::unique_ptr<SkBitmap> BuildBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(kDefaultColor);
  return std::make_unique<SkBitmap>(std::move(bitmap));
}

class PermissionsAiv3EncoderFake : public PermissionsAiv3Encoder {
 public:
  explicit PermissionsAiv3EncoderFake(RequestType type)
      : PermissionsAiv3Encoder(type) {}

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
    auto ret = PermissionsAiv3Encoder::Preprocess(input_tensors, input);
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

    auto geolocation_encoder_mock =
        std::make_unique<PermissionsAiv3EncoderFake>(RequestType::kGeolocation);
    geolocation_encoder_mock_ = geolocation_encoder_mock.get();
    geolocation_model_handler_ = std::make_unique<PermissionsAiv3Handler>(
        model_provider_.get(),
        /*optimization_target=*/kOptTargetGeolocation,
        /*request_type=*/RequestType::kGeolocation,
        task_environment_.GetMainThreadTaskRunner(),
        std::move(geolocation_encoder_mock));

    auto notification_encoder_mock =
        std::make_unique<PermissionsAiv3EncoderFake>(
            RequestType::kNotifications);
    notification_encoder_mock_ = notification_encoder_mock.get();
    notification_model_handler_ = std::make_unique<PermissionsAiv3Handler>(
        model_provider_.get(),
        /*optimization_target=*/kOptTargetNotification,
        /*request_type=*/RequestType::kNotifications,
        task_environment_.GetMainThreadTaskRunner(),
        std::move(notification_encoder_mock));
  }

  void TearDown() override {
    geolocation_encoder_mock_ = nullptr;
    notification_encoder_mock_ = nullptr;
    geolocation_model_handler_.reset();
    notification_model_handler_.reset();
    model_provider_.reset();
    task_environment_.RunUntilIdle();
  }

  void PushModelFileToModelExecutor(OptimizationTarget opt_target,
                                    const base::FilePath& model_file_path) {
    std::optional<optimization_guide::proto::Any> any;

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

 protected:
  raw_ptr<PermissionsAiv3EncoderFake> geolocation_encoder_mock_;
  raw_ptr<PermissionsAiv3EncoderFake> notification_encoder_mock_;

  std::unique_ptr<PermissionsAiv3Handler> geolocation_model_handler_;
  std::unique_ptr<PermissionsAiv3Handler> notification_model_handler_;

  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  base::test::TaskEnvironment task_environment_;
};

class Aiv3HandlerTest : public Aiv3HandlerTestBase {};

struct RelevanceTestCase {
  OptimizationTarget optimization_target;
  base::FilePath model_file_path;
  PermissionRequestRelevance expected_relevance;
};

class RelevanceAiv3HandlerTest
    : public Aiv3HandlerTestBase,
      public testing::WithParamInterface<RelevanceTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ModelResults,
    RelevanceAiv3HandlerTest,
    testing::ValuesIn<RelevanceTestCase>({
        {kOptTargetGeolocation, ModelFilePath(kZeroReturnModel),
         PermissionRequestRelevance::kVeryLow},
        {kOptTargetGeolocation, ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kHigh},
        {kOptTargetGeolocation, ModelFilePath(kOneReturnModel),
         PermissionRequestRelevance::kVeryHigh},
        {kOptTargetNotification, ModelFilePath(kZeroReturnModel),
         PermissionRequestRelevance::kVeryLow},
        {kOptTargetNotification, ModelFilePath(kZeroDotFiveReturnModel),
         PermissionRequestRelevance::kMedium},
        {kOptTargetNotification, ModelFilePath(kOneReturnModel),
         PermissionRequestRelevance::kVeryHigh},
    }));

TEST_P(RelevanceAiv3HandlerTest,
       RelevanceIsMatchedToTheCorrectModelThresholds) {
  PushModelFileToModelExecutor(GetParam().optimization_target,
                               GetParam().model_file_path);
  auto* aiv3_handler = model_handler(GetParam().optimization_target);
  EXPECT_TRUE(aiv3_handler->ModelAvailable());

  ModelCallbackFuture future;
  aiv3_handler->ExecuteModel(
      future.GetCallback(),
      /*snapshot=*/BuildBitmap(kModelInputWidth, kModelInputHeight));
  EXPECT_EQ(future.Take(), GetParam().expected_relevance);
}

TEST_F(Aiv3HandlerTest, BitmapGetsCopiedToTensor) {
  PushModelFileToModelExecutor(kOptTargetGeolocation,
                               ModelFilePath(kZeroReturnModel));

  auto snapshot = BuildBitmap(kModelInputWidth, kModelInputHeight);

  FillDataToBitmap(snapshot.get());

  bool flag = false;
  geolocation_encoder_mock_->set_preprocess_hook(base::BindOnce(
      [](bool* flag, const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        if (tflite::task::core::PopulateVector<float>(input_tensors[0], &data)
                .ok()) {
          EXPECT_THAT(data, SizeIs(kModelInputWidth * kModelInputHeight * 3));
          for (int i = 0; i < kModelInputWidth * kModelInputHeight; i += 3) {
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
  aiv3_handler->ExecuteModel(future.GetCallback(), std::move(snapshot));
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

TEST_F(Aiv3HandlerTest, HandlesEmptyInputSnapshot) {
  PushModelFileToModelExecutor(kOptTargetGeolocation,
                               ModelFilePath(kZeroReturnModel));

  auto snapshot = BuildBitmap(/*width=*/0, /*height=*/0);

  FillDataToBitmap(snapshot.get());

  ModelCallbackFuture future;
  auto* aiv3_handler = model_handler(kOptTargetGeolocation);
  aiv3_handler->ExecuteModel(future.GetCallback(), std::move(snapshot));
  EXPECT_EQ(future.Take(), std::nullopt);
}

struct ResizeTestCase {
  int input_width;
  int input_height;
};

class ResizeAiv3HandlerTest
    : public Aiv3HandlerTestBase,
      public testing::WithParamInterface<ResizeTestCase> {};

INSTANTIATE_TEST_SUITE_P(ResizeBitmapInternally,
                         ResizeAiv3HandlerTest,
                         testing::ValuesIn<ResizeTestCase>({
                             {/*input_width=*/32, /*input_height=*/32},
                             {/*input_width=*/32, /*input_height=*/64},
                             {/*input_width=*/64, /*input_height=*/32},
                             {/*input_width=*/128, /*input_height=*/128},
                             {/*input_width=*/64, /*input_height=*/128},
                             {/*input_width=*/128, /*input_height=*/64},
                         }));

TEST_P(ResizeAiv3HandlerTest, ResizesBitmapsForModelInput) {
  PushModelFileToModelExecutor(kOptTargetGeolocation,
                               ModelFilePath(kZeroReturnModel));

  auto snapshot = BuildBitmap(GetParam().input_width, GetParam().input_height);

  FillDataToBitmap(snapshot.get());

  bool flag = false;
  geolocation_encoder_mock_->set_preprocess_hook(base::BindOnce(
      [](bool* flag, const std::vector<TfLiteTensor*>& input_tensors) {
        std::vector<float> data;
        if (tflite::task::core::PopulateVector<float>(input_tensors[0], &data)
                .ok()) {
          EXPECT_THAT(data, SizeIs(kModelInputWidth * kModelInputHeight * 3));
          for (int i = 0; i < kModelInputWidth * kModelInputHeight * 3; ++i) {
            EXPECT_FALSE(std::isnan(data[i]));
          }
        }
        *flag = true;
      },
      &flag));

  ModelCallbackFuture future;
  auto* aiv3_handler = model_handler(kOptTargetGeolocation);
  aiv3_handler->ExecuteModel(future.GetCallback(), std::move(snapshot));
  EXPECT_EQ(future.Take(), PermissionRequestRelevance::kVeryLow);
  EXPECT_TRUE(flag);
}

}  // namespace
}  // namespace permissions
