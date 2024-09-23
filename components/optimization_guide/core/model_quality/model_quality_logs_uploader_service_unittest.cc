// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

std::unique_ptr<proto::LogAiDataRequest> BuildComposeLogAiDataReuqest() {
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
      new proto::LogAiDataRequest());

  // Inject logging metadata and make sure it gets written to final log.
  log_ai_data_request->mutable_logging_metadata()
      ->mutable_system_profile()
      ->set_build_timestamp(12345);

  proto::ComposeLoggingData compose_logging_data;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");

  proto::ComposeResponse response;
  response.set_output("compose response");

  proto::ComposeQuality quality;
  quality.set_final_status(
      optimization_guide::proto::FinalStatus::STATUS_INSERTED);

  *(compose_logging_data.mutable_request()) = request;
  *(compose_logging_data.mutable_response()) = response;
  *(compose_logging_data.mutable_quality()) = quality;

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
    model_execution::prefs::RegisterLocalStatePrefs(pref_service_.registry());
    model_quality_logs_uploader_service_ =
        std::make_unique<ModelQualityLogsUploaderService>(
            shared_url_loader_factory_, &pref_service_);
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

  void SetUp() override {
    model_execution::prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void WritePerformanceClassToPref(OnDeviceModelPerformanceClass perf_class) {
    pref_service_.SetInteger(
        model_execution::prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kVeryHigh));
  }

  void UploadModelQualityLogs(
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request) {
    model_quality_logs_uploader_service_->UploadModelQualityLogs(
        std::move(log_ai_data_request));

    RunUntilIdle();
  }

  void UploadModelQualityLogsWithLogEntry(
      std::unique_ptr<ModelQualityLogEntry> log_entry) {
    model_quality_logs_uploader_service_->UploadModelQualityLogs(
        std::move(log_entry->log_ai_data_request_));

    RunUntilIdle();
  }

  std::unique_ptr<ModelQualityLogEntry> GetModelQualityLogEntryAndSetFeedback(
      UserVisibleFeatureKey feature,
      proto::UserFeedback feedback) {
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
        new proto::LogAiDataRequest());
    std::unique_ptr<ModelQualityLogEntry> log_entry =
        std::make_unique<ModelQualityLogEntry>(
            std::move(log_ai_data_request),
            model_quality_logs_uploader_service_->GetWeakPtr());
    switch (feature) {
      case UserVisibleFeatureKey::kCompose:
        log_entry->quality_data<ComposeFeatureTypeMap>()->set_user_feedback(
            feedback);
        break;
      case UserVisibleFeatureKey::kTabOrganization:
        log_entry->quality_data<TabOrganizationFeatureTypeMap>()
            ->add_organizations()
            ->set_user_feedback(feedback);
        break;
      case UserVisibleFeatureKey::kWallpaperSearch:
        log_entry->quality_data<WallpaperSearchFeatureTypeMap>()
            ->set_user_feedback(feedback);
        break;
      case UserVisibleFeatureKey::kHistorySearch:
        // TODO(crbug.com/345308285): Add user feedback for history searches.
        break;
    }

    return log_entry;
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

  std::optional<proto::LogAiDataRequest> GetPendingLogsUploadRequest() {
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
  TestingPrefServiceSimple pref_service_;
  std::optional<proto::LogAiDataRequest> last_ai_data_request_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ModelQualityLogsUploaderServiceTest, TestSuccessfulResponse) {
  WritePerformanceClassToPref(OnDeviceModelPerformanceClass::kVeryHigh);

  auto ai_data_request = BuildComposeLogAiDataReuqest();
  UploadModelQualityLogs(std::move(ai_data_request));
  VerifyHasPendingLogsUploadRequest();

  proto::LogAiDataResponse response;
  EXPECT_TRUE(SimulateSuccessfulResponse(response));

  auto pending_request = GetPendingLogsUploadRequest();
  EXPECT_EQ(proto::LogAiDataRequest::FeatureCase::kCompose,
            pending_request->feature_case());
  // Performance class should be attached.
  EXPECT_EQ(proto::PERFORMANCE_CLASS_VERY_HIGH,
            pending_request->logging_metadata()
                .on_device_system_profile()
                .performance_class());
  EXPECT_EQ(
      12345,
      pending_request->logging_metadata().system_profile().build_timestamp());

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.NetErrorCode",
      -net::OK, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.Status", 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kUploadSuccessful, 1);
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
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kUploadSuccessful, 2);
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

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kNetError, 1);
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

TEST_F(ModelQualityLogsUploaderServiceTest, WallpaperSearchUserFeedbackUMA) {
  std::unique_ptr<ModelQualityLogEntry> log_entry_1 =
      GetModelQualityLogEntryAndSetFeedback(
          UserVisibleFeatureKey::kWallpaperSearch,
          proto::USER_FEEDBACK_THUMBS_UP);
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_1));

  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.WallpaperSearch",
      proto::USER_FEEDBACK_THUMBS_UP, 1);

  std::unique_ptr<ModelQualityLogEntry> log_entry_2 =
      GetModelQualityLogEntryAndSetFeedback(
          UserVisibleFeatureKey::kWallpaperSearch,
          proto::USER_FEEDBACK_THUMBS_DOWN);
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_2));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.WallpaperSearch",
      proto::USER_FEEDBACK_THUMBS_DOWN, 1);
}

TEST_F(ModelQualityLogsUploaderServiceTest, TabOrganizationUserFeedbackUMA) {
  std::unique_ptr<ModelQualityLogEntry> log_entry_1 =
      GetModelQualityLogEntryAndSetFeedback(
          UserVisibleFeatureKey::kTabOrganization,
          proto::USER_FEEDBACK_THUMBS_UP);
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_1));

  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.TabOrganization",
      proto::USER_FEEDBACK_THUMBS_UP, 1);

  std::unique_ptr<ModelQualityLogEntry> log_entry_2 =
      GetModelQualityLogEntryAndSetFeedback(
          UserVisibleFeatureKey::kTabOrganization,
          proto::USER_FEEDBACK_THUMBS_DOWN);
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_2));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.TabOrganization",
      proto::USER_FEEDBACK_THUMBS_DOWN, 1);
}

TEST_F(ModelQualityLogsUploaderServiceTest,
       TabOrganizationUserFeedbackNullCheck) {
  // Set TabOrganization ModelQualityLogEntry without any quality data tab
  // organization.
  std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request_1(
      new proto::LogAiDataRequest());

  proto::TabOrganizationLoggingData tab_organization_logging_data;

  proto::TabOrganizationRequest tab_request;

  *(tab_organization_logging_data.mutable_request()) = tab_request;
  *(log_ai_data_request_1->mutable_tab_organization()) =
      tab_organization_logging_data;
  std::unique_ptr<ModelQualityLogEntry> log_entry_1 =
      std::make_unique<ModelQualityLogEntry>(std::move(log_ai_data_request_1),
                                             nullptr);

  // Upload logs without quality data set this should mark user_feedback as
  // unspecified.
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_1));

  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.TabOrganization",
      proto::USER_FEEDBACK_UNSPECIFIED, 1);
}

TEST_F(ModelQualityLogsUploaderServiceTest,
       TabOrganizationMultipleOrganizationUserFeedbackUMA) {
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      GetModelQualityLogEntryAndSetFeedback(
          UserVisibleFeatureKey::kTabOrganization,
          proto::USER_FEEDBACK_THUMBS_UP);
  // Add one more tab organization to existing log_entry with user feedback.
  log_entry->quality_data<TabOrganizationFeatureTypeMap>()
      ->add_organizations()
      ->set_user_feedback(proto::USER_FEEDBACK_THUMBS_DOWN);

  UploadModelQualityLogsWithLogEntry(std::move(log_entry));

  // We only record the first user feedback value.
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.TabOrganization",
      proto::USER_FEEDBACK_THUMBS_UP, 1);
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.TabOrganization",
      proto::USER_FEEDBACK_THUMBS_DOWN, 0);
}

TEST_F(ModelQualityLogsUploaderServiceTest, ComposeUserFeedbackUMA) {
  std::unique_ptr<ModelQualityLogEntry> log_entry_1 =
      GetModelQualityLogEntryAndSetFeedback(UserVisibleFeatureKey::kCompose,
                                            proto::USER_FEEDBACK_THUMBS_UP);
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_1));

  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.Compose",
      proto::USER_FEEDBACK_THUMBS_UP, 1);

  std::unique_ptr<ModelQualityLogEntry> log_entry_2 =
      GetModelQualityLogEntryAndSetFeedback(UserVisibleFeatureKey::kCompose,
                                            proto::USER_FEEDBACK_THUMBS_DOWN);
  UploadModelQualityLogsWithLogEntry(std::move(log_entry_2));
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelQuality.UserFeedback.Compose",
      proto::USER_FEEDBACK_THUMBS_DOWN, 1);
}

TEST_F(ModelQualityLogsUploaderServiceTest, CheckUploadOnDestruction) {
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      GetModelQualityLogEntryAndSetFeedback(UserVisibleFeatureKey::kCompose,
                                            proto::USER_FEEDBACK_THUMBS_UP);
  // Instead of calling UploadModelQualityLogs, resetting the log entry this
  // shouldn't upload the logs as
  // ModelQualityLogsUploaderService::CanUploadLogs will return false.
  log_entry.reset();

  RunUntilIdle();
  VerifyNumberOfPendingRequests(0);
}

// TODO(b/301301447): Add more tests to cover all cases.

}  // namespace optimization_guide
