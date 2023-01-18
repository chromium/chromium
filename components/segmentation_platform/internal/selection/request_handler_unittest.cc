// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace segmentation_platform {
namespace {

// Test Ids.
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

class MockResultProvider : public SegmentResultProvider {
 public:
  MOCK_METHOD1(GetSegmentResult,
               void(std::unique_ptr<GetResultOptions> options));
};

std::unique_ptr<Config> CreateTestConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = "client_key";
  config->segmentation_uma_name = "TestUmaKey";
  config->AddSegmentId(kSegmentId);
  return config;
}

proto::PredictionResult CreatePredictionResultWithBinaryClassifier() {
  proto::PredictionResult prediction_result;
  proto::Predictor_BinaryClassifier* binary_classifier =
      prediction_result.mutable_output_config()
          ->mutable_predictor()
          ->mutable_binary_classifier();

  binary_classifier->set_threshold(0.5f);
  binary_classifier->set_positive_label("positive_label");
  binary_classifier->set_negative_label("negative_label");

  prediction_result.add_result(0.8f);
  return prediction_result;
}

class RequestHandlerTest : public testing::Test {
 public:
  RequestHandlerTest() = default;
  ~RequestHandlerTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    config_ = CreateTestConfig();
    auto provider = std::make_unique<MockResultProvider>();
    result_provider_ = provider.get();
    request_handler_ =
        RequestHandler::Create(*config_, std::move(provider), nullptr);
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
  std::unique_ptr<Config> config_;
  raw_ptr<MockResultProvider> result_provider_ = nullptr;
  std::unique_ptr<RequestHandler> request_handler_;
};

TEST_F(RequestHandlerTest, TestGetClassificationResult) {
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_CALL(*result_provider_, GetSegmentResult(_))
      .Times(1)
      .WillRepeatedly(Invoke(
          [](std::unique_ptr<SegmentResultProvider::GetResultOptions> options) {
            EXPECT_TRUE(options->ignore_db_scores);
            EXPECT_EQ(options->segment_id, kSegmentId);
            auto result =
                std::make_unique<SegmentResultProvider::SegmentResult>(
                    SegmentResultProvider::ResultState::kTfliteModelScoreUsed,
                    CreatePredictionResultWithBinaryClassifier(), /*rank=*/2);
            std::move(options->callback).Run(std::move(result));
          }));

  base::RunLoop loop;
  ClassificationResult expected(PredictionStatus::kSucceeded);
  expected.ordered_labels.emplace_back("positive_label");
  request_handler_->GetClassificationResult(
      options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestHandlerTest::OnGetClassificationResult,
                     base::Unretained(this), loop.QuitClosure(), expected));
  loop.Run();
}

}  // namespace
}  // namespace segmentation_platform
