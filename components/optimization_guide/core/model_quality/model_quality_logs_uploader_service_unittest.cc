// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

namespace {

std::unique_ptr<proto::LogAiDataRequest> BuildComposeLogAiDataReuqest() {
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
      new proto::LogAiDataRequest());

  proto::ComposeLoggingData compose_logging_data;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");

  proto::ComposeResponse response;
  response.set_output("compose response");

  proto::ComposeQuality quality;
  quality.set_final_status(
      optimization_guide::proto::FinalStatus::STATUS_INSERTED);

  *(compose_logging_data.mutable_request_data()) = request;
  *(compose_logging_data.mutable_response_data()) = response;
  *(compose_logging_data.mutable_quality_data()) = quality;

  *(log_ai_data_request->mutable_compose()) = compose_logging_data;
  return log_ai_data_request;
}

}  // namespace

class ModelQualityLogsUploaderServiceTest : public testing::Test {
 public:
  ModelQualityLogsUploaderServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    model_quality_logs_uploader_service_ =
        std::make_unique<ModelQualityLogsUploaderService>(
            shared_url_loader_factory_);
    // Enable compose logging.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kModelQualityLogging,
        {{"model_execution_feature_compose", "true"}});
  }
  ModelQualityLogsUploaderServiceTest(
      const ModelQualityLogsUploaderServiceTest&) = delete;
  ModelQualityLogsUploaderServiceTest& operator=(
      const ModelQualityLogsUploaderServiceTest&) = delete;

  ~ModelQualityLogsUploaderServiceTest() override = default;

  void UploadModelQualityLogs(
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request) {
    model_quality_logs_uploader_service_->UploadModelQualityLogs(
        std::move(log_ai_data_request));

    RunUntilIdle();
  }

  void VerifyNumberOfPendingRequests(int no_requests) {
    EXPECT_GE(test_url_loader_factory_.NumPending(), no_requests);
  }

  void VerifyHasPendingLogsUploadRequest() {
    EXPECT_GE(test_url_loader_factory_.NumPending(), 1);
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ(pending_request->request.method, "POST");
    std::string key_value;
    EXPECT_TRUE(net::GetValueForKeyInQuery(pending_request->request.url, "key",
                                           &key_value));
    EXPECT_EQ(pending_request->request.request_body->elements()->size(), 1u);
    auto& element =
        pending_request->request.request_body->elements_mutable()->front();
    CHECK(element.type() == network::DataElement::Tag::kBytes);
    auto request_body = element.As<network::DataElementBytes>().AsStringPiece();
    last_ai_data_request_.reset();
    proto::LogAiDataRequest ai_data_request;
    if (ai_data_request.ParseFromString(
            static_cast<std::string>(request_body))) {
      last_ai_data_request_ = ai_data_request;
    }
  }

  absl::optional<proto::LogAiDataRequest> GetPendingLogsUploadRequest() {
    return last_ai_data_request_;
  }

  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        kOptimizationGuideServiceModelQualtiyDefaultURL, content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulResponse(
      const proto::LogAiDataResponse& ai_data_response) {
    std::string serialized_response;
    ai_data_response.SerializeToString(&serialized_response);
    return SimulateResponse(serialized_response, net::HTTP_OK);
  }

 protected:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ModelQualityLogsUploaderService>
      model_quality_logs_uploader_service_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;

  absl::optional<proto::LogAiDataRequest> last_ai_data_request_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ModelQualityLogsUploaderServiceTest, TestSuccessfulResponse) {
  auto ai_data_request = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request));
  VerifyHasPendingLogsUploadRequest();

  proto::LogAiDataResponse response;
  EXPECT_TRUE(SimulateSuccessfulResponse(response));

  EXPECT_EQ(proto::LogAiDataRequest::FeatureCase::kCompose,
            GetPendingLogsUploadRequest()->feature_case());

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode",
      -net::OK, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.Status", 1);
}

TEST_F(ModelQualityLogsUploaderServiceTest, TestMultipleUploads) {
  auto ai_data_request_1 = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request_1));

  auto ai_data_request_2 = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request_2));

  // Number of pending requests should be two.
  VerifyNumberOfPendingRequests(2);

  proto::LogAiDataResponse response1;
  EXPECT_TRUE(SimulateSuccessfulResponse(response1));

  proto::LogAiDataResponse response2;
  EXPECT_TRUE(SimulateSuccessfulResponse(response2));

  // Both uploads should succeed.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode",
      -net::OK, 2);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.Status", 2);
}

TEST_F(ModelQualityLogsUploaderServiceTest, TestUploadWhenLoggingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kModelQualityLogging,
      {{"model_execution_feature_compose", "false"}});

  auto ai_data_request = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request));

  // When logging is disabled there should be no pending requests.
  VerifyNumberOfPendingRequests(0);
}

TEST_F(ModelQualityLogsUploaderServiceTest, TestUploadWhenRequestIsEmpty) {
  UploadModelQualityLogs(nullptr);

  // When the request is null there should be no pending requests.
  VerifyNumberOfPendingRequests(0);
}

TEST_F(ModelQualityLogsUploaderServiceTest, TestNetErrorResponse) {
  auto ai_data_request = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request));

  VerifyHasPendingLogsUploadRequest();

  SimulateResponse("foo response", net::HTTP_NOT_FOUND);

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.Status",
      net::HTTP_NOT_FOUND, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode", 1);
}

TEST_F(ModelQualityLogsUploaderServiceTest, TestBadResponse) {
  auto ai_data_request = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request));

  VerifyHasPendingLogsUploadRequest();

  SimulateResponse("bad response", net::HTTP_OK);

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.Status", net::HTTP_OK,
      1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode", 1);
}

// TODO(b/301301447): Add more tests to cover all cases.

}  // namespace optimization_guide
