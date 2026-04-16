// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

// A fake BinaryUploadRequest for testing.
class FakeBinaryUploadRequest : public BinaryUploadRequest {
 public:
  explicit FakeBinaryUploadRequest(ContentAnalysisCallback callback)
      : BinaryUploadRequest(
            std::move(callback),
            CloudOrLocalAnalysisSettings(CloudAnalysisSettings()),
            base::NullCallback()) {}
  void GetRequestData(DataCallback callback) override {
    std::move(callback).Run(ScanRequestUploadResult::kSuccess, Data());
  }
};

// A test class that exposes protected members of CloudBinaryUploadServiceBase.
class TestCloudBinaryUploadServiceBase : public CloudBinaryUploadServiceBase {
 public:
  using CloudBinaryUploadServiceBase::active_requests_;
  using CloudBinaryUploadServiceBase::received_connector_results_;
  using CloudBinaryUploadServiceBase::RecordRequestMetrics;
  using CloudBinaryUploadServiceBase::start_times_;

  // CloudBinaryUploadServiceBase:
  void MaybeGetAccessToken(BinaryUploadRequest::Id request_id) override {}
  enterprise_connectors::BinaryUploadRequest::BrowserPolicyConnectorGetter
  BrowserPolicyConnectorGetter() override {
    return base::BindRepeating(
        []() -> policy::BrowserPolicyConnector* { return nullptr; });
  }
  bool IsAdvancedProtection() override { return false; }
  bool IsEnhancedProtection() override { return false; }
#if BUILDFLAG(IS_CHROMEOS)
  bool IsManagedGuestSession() override { return false; }
#endif
};

}  // namespace

class CloudBinaryUploadServiceBaseTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that GetParallelActiveRequestsMax returns the correct value based on
// features and parameters.
TEST_F(CloudBinaryUploadServiceBaseTest, GetParallelActiveRequestsMax) {
  // Default value.
  EXPECT_EQ(CloudBinaryUploadServiceBase::GetParallelActiveRequestsMax(),
            static_cast<size_t>(kDefaultMaxParallelActiveRequests));

  // Experiment value.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnableNewUploadCountLimit,
      {{kParallelContentAnalysisRequestCountMax.name, "10"}});
  EXPECT_EQ(CloudBinaryUploadServiceBase::GetParallelActiveRequestsMax(), 10u);
}

// Tests that GetUploadUrl returns the correct URL for enterprise and consumer
// scans.
TEST_F(CloudBinaryUploadServiceBaseTest, GetUploadUrl) {
  EXPECT_EQ(CloudBinaryUploadServiceBase::GetUploadUrl(
                /*is_consumer_scan_eligible=*/false)
                .spec(),
            "https://safebrowsing.google.com/safebrowsing/uploads/scan");
  EXPECT_EQ(CloudBinaryUploadServiceBase::GetUploadUrl(
                /*is_consumer_scan_eligible=*/true)
                .spec(),
            "https://safebrowsing.google.com/safebrowsing/uploads/consumer");
}

// Tests that ResponseIsComplete correctly identifies when all expected
// connector results have been received.
TEST_F(CloudBinaryUploadServiceBaseTest, ResponseIsComplete) {
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  // Request doesn't exist.
  EXPECT_FALSE(service.ResponseIsComplete(id));

  auto request = std::make_unique<FakeBinaryUploadRequest>(base::DoNothing());
  request->add_tag("dlp");
  request->add_tag("malware");
  service.active_requests_[id] = std::move(request);

  // No results yet.
  EXPECT_FALSE(service.ResponseIsComplete(id));

  // Only DLP result.
  service.received_connector_results_[id]["dlp"] =
      ContentAnalysisResponse::Result();
  EXPECT_FALSE(service.ResponseIsComplete(id));

  // Both results.
  service.received_connector_results_[id]["malware"] =
      ContentAnalysisResponse::Result();
  EXPECT_TRUE(service.ResponseIsComplete(id));
}

// Tests that ResponseIsComplete correctly handles skipped malware scans.
TEST_F(CloudBinaryUploadServiceBaseTest, ResponseIsComplete_SkipMalware) {
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  auto request = std::make_unique<FakeBinaryUploadRequest>(base::DoNothing());
  request->add_tag("dlp");
  request->add_tag("malware");
  request->set_should_skip_malware_scan(true);
  service.active_requests_[id] = std::move(request);

  // No results yet.
  EXPECT_FALSE(service.ResponseIsComplete(id));

  // Only DLP result, malware is skipped.
  service.received_connector_results_[id]["dlp"] =
      ContentAnalysisResponse::Result();
  EXPECT_TRUE(service.ResponseIsComplete(id));
}

// Tests that GetRequest correctly retrieves an active request.
TEST_F(CloudBinaryUploadServiceBaseTest, GetRequest) {
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  EXPECT_EQ(service.GetRequest(id), nullptr);

  auto request = std::make_unique<FakeBinaryUploadRequest>(base::DoNothing());
  BinaryUploadRequest* request_ptr = request.get();
  service.active_requests_[id] = std::move(request);

  EXPECT_EQ(service.GetRequest(id), request_ptr);
}

// Tests that RecordRequestMetrics correctly logs basic upload result and
// duration histograms.
TEST_F(CloudBinaryUploadServiceBaseTest, RecordRequestMetrics) {
  base::HistogramTester histograms;
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  service.start_times_[id] = base::TimeTicks::Now();
  task_environment_.FastForwardBy(base::Seconds(1));

  service.RecordRequestMetrics(id, ScanRequestUploadResult::kSuccess);

  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.Result",
                                ScanRequestUploadResult::kSuccess, 1);
  histograms.ExpectUniqueTimeSample("SafeBrowsingBinaryUploadRequest.Duration",
                                    base::Seconds(1), 1);
}

// Tests that RecordRequestMetrics correctly logs detailed result status from
// the ContentAnalysisResponse.
TEST_F(CloudBinaryUploadServiceBaseTest, RecordRequestMetricsWithResponse) {
  base::HistogramTester histograms;
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  service.start_times_[id] = base::TimeTicks::Now();
  task_environment_.FastForwardBy(base::Seconds(2));

  ContentAnalysisResponse response;
  auto* malware_result = response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(ContentAnalysisResponse::Result::SUCCESS);

  auto* dlp_result = response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(ContentAnalysisResponse::Result::FAILURE);

  service.RecordRequestMetrics(id, ScanRequestUploadResult::kSuccess, response);

  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.Result",
                                ScanRequestUploadResult::kSuccess, 1);
  histograms.ExpectUniqueTimeSample("SafeBrowsingBinaryUploadRequest.Duration",
                                    base::Seconds(2), 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.MalwareResult",
                                true, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.DlpResult",
                                false, 1);
}

// Tests that RecordRequestMetrics logs the correct enterprise-specific
// histograms for a resumable file upload.
TEST_F(CloudBinaryUploadServiceBaseTest,
       RecordRequestMetrics_EnterpriseFileResumable) {
  base::HistogramTester histograms;
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  auto request = std::make_unique<FakeBinaryUploadRequest>(base::DoNothing());
  request->set_device_token("dm_token");
  request->set_analysis_connector(AnalysisConnector::FILE_DOWNLOADED);

  service.active_requests_[id] = std::move(request);
  service.start_times_[id] = base::TimeTicks::Now();
  task_environment_.FastForwardBy(base::Seconds(3));

  service.RecordRequestMetrics(id, ScanRequestUploadResult::kSuccess);

  histograms.ExpectUniqueSample("Enterprise.ResumableRequest.File.Result",
                                ScanRequestUploadResult::kSuccess, 1);
  histograms.ExpectUniqueTimeSample("Enterprise.ResumableRequest.File.Duration",
                                    base::Seconds(3), 1);
}

// Tests that RecordRequestMetrics logs the correct enterprise-specific
// histograms for a multipart text upload.
TEST_F(CloudBinaryUploadServiceBaseTest,
       RecordRequestMetrics_EnterpriseTextMultipart) {
  base::HistogramTester histograms;
  TestCloudBinaryUploadServiceBase service;
  BinaryUploadRequest::Id id(1);

  auto request = std::make_unique<FakeBinaryUploadRequest>(base::DoNothing());
  request->set_device_token("dm_token");
  request->set_analysis_connector(AnalysisConnector::BULK_DATA_ENTRY);

  service.active_requests_[id] = std::move(request);
  service.start_times_[id] = base::TimeTicks::Now();
  task_environment_.FastForwardBy(base::Seconds(4));

  service.RecordRequestMetrics(id, ScanRequestUploadResult::kSuccess);

  histograms.ExpectUniqueSample("Enterprise.MultipartRequest.Text.Result",
                                ScanRequestUploadResult::kSuccess, 1);
  histograms.ExpectUniqueTimeSample("Enterprise.MultipartRequest.Text.Duration",
                                    base::Seconds(4), 1);
}

}  // namespace enterprise_connectors
