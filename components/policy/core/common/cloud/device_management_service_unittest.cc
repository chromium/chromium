// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/device_management_service.h"

#include <ostream>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;

namespace em = enterprise_management;

namespace policy {

const char kServiceUrl[] = "https://example.com/management_service";

// Encoded empty response messages for testing the error code paths.
const char kResponseEmpty[] = "\x08\x00";

#define PROTO_STRING(name) (std::string(name, base::size(name) - 1))

// Some helper constants.
const char kGaiaAuthToken[] = "gaia-auth-token";
const char kOAuthToken[] = "oauth-token";
const char kDMToken[] = "device-management-token";
const char kClientID[] = "device-id";
const char kRobotAuthCode[] = "robot-oauth-auth-code";
const char kEnrollmentToken[] = "enrollment_token";

// Unit tests for the device management policy service. The tests are run
// against a TestURLLoaderFactory that is used to short-circuit the request
// without calling into the actual network stack.
class DeviceManagementServiceTestBase : public testing::Test {
 protected:
  DeviceManagementServiceTestBase() {
    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    ResetService();
    InitializeService();
  }

  ~DeviceManagementServiceTestBase() override {
    service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    // Verify the metrics when job is done.
    ON_CALL(*this, OnJobDone(_, _, _, _))
        .WillByDefault(Invoke(
            [this](DeviceManagementService::Job*, DeviceManagementStatus status,
                   int net_error,
                   const std::string&) { VerifyMetrics(status, net_error); }));
  }

  void TearDown() override {
    // Metrics data is always reset after verification so there shouldn't be any
    // data point left.
    EXPECT_EQ(
        0u, histogram_tester_.GetTotalCountsForPrefix(request_uma_name_prefix_)
                .size());
  }

  void ResetService() {
    std::unique_ptr<DeviceManagementService::Configuration> configuration(
        new MockDeviceManagementServiceConfiguration(kServiceUrl));
    service_.reset(new DeviceManagementService(std::move(configuration)));
  }

  void InitializeService() {
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index = 0) {
    if (index >= url_loader_factory_.pending_requests()->size())
      return nullptr;
    return &(*url_loader_factory_.pending_requests())[index];
  }

  std::unique_ptr<DeviceManagementService::Job> StartJob(
      DeviceManagementService::JobConfiguration::JobType type,
      bool critical,
      std::unique_ptr<DMAuth> auth_data,
      base::Optional<std::string> oauth_token,
      const std::string& payload = std::string()) {
    last_job_type_ =
        DeviceManagementService::JobConfiguration::GetJobTypeAsString(type);
    std::unique_ptr<FakeJobConfiguration> config =
        std::make_unique<FakeJobConfiguration>(
            service_.get(), type, kClientID, critical, std::move(auth_data),
            oauth_token, shared_url_loader_factory_,
            base::Bind(&DeviceManagementServiceTestBase::OnJobDone,
                       base::Unretained(this)),
            base::BindRepeating(&DeviceManagementServiceTestBase::OnJobRetry,
                                base::Unretained(this)));
    config->SetRequestPayload(payload);
    return service_->CreateJob(std::move(config));
  }

  std::unique_ptr<DeviceManagementService::Job> StartRegistrationJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
        /*critical=*/false, DMAuth::NoAuth(), kOAuthToken, payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartCertBasedRegistrationJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
        /*critical=*/false, DMAuth::FromGaiaToken(kGaiaAuthToken),
        std::string(), payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartTokenEnrollmentJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT,
        /*critical=*/false, DMAuth::FromEnrollmentToken(kEnrollmentToken),
        std::string(), payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartApiAuthCodeFetchJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
        /*critical=*/false, DMAuth::NoAuth(), kOAuthToken, payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartUnregistrationJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
        /*critical=*/false, DMAuth::FromDMToken(kDMToken), std::string(),
        payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartPolicyFetchJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
        /*critical=*/false, DMAuth::NoAuth(), kOAuthToken, payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartCriticalPolicyFetchJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
        /*critical=*/true, DMAuth::NoAuth(), kOAuthToken, payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartAutoEnrollmentJob(
      const std::string& payload = std::string()) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
        /*critical=*/false, DMAuth::NoAuth(), std::string(), payload);
  }

  std::unique_ptr<DeviceManagementService::Job> StartAppInstallReportJob(
      const std::string& payload = std::string()) {
    return StartJob(DeviceManagementService::JobConfiguration::
                        TYPE_UPLOAD_APP_INSTALL_REPORT,
                    /*critical=*/false, DMAuth::FromDMToken(kDMToken),
                    std::string(), payload);
  }

  void SendResponse(net::Error error,
                    int http_status,
                    const std::string& response,
                    const std::string& mime_type = std::string(),
                    bool was_fetched_via_proxy = false) {
    service_->OnURLLoaderCompleteInternal(
        service_->GetSimpleURLLoaderForTesting(), response, mime_type, error,
        http_status, was_fetched_via_proxy);
  }

  void VerifyMetrics(DeviceManagementStatus status, int net_error) {
    EXPECT_LE(expected_retry_count_, 10);
    DCHECK_NE(last_job_type_, "");
    EXPECT_EQ(
        1u, histogram_tester_.GetTotalCountsForPrefix(request_uma_name_prefix_)
                .size());
    int expected_sample;
    if (net_error != net::OK) {
      expected_sample = static_cast<int>(
          DMServerRequestSuccess::kRequestFailed);  // network error sample
    } else if (status != DM_STATUS_SUCCESS &&
               status != DM_STATUS_RESPONSE_DECODING_ERROR) {
      expected_sample = static_cast<int>(
          DMServerRequestSuccess::kRequestError);  // server error sample
    } else {
      expected_sample = expected_retry_count_;  // Success without retry sample
    }
    histogram_tester_.ExpectUniqueSample(
        request_uma_name_prefix_ + last_job_type_, expected_sample, 1);

    // Reset metrics data for next request.
    statistics_recorder_.reset();
    statistics_recorder_ =
        base::StatisticsRecorder::CreateTemporaryForTesting();
  }

  MOCK_METHOD4(OnJobDone,
               void(DeviceManagementService::Job*,
                    DeviceManagementStatus,
                    int,
                    const std::string&));

  MOCK_METHOD0(OnJobRetry, void());

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;
  std::unique_ptr<DeviceManagementService> service_;

  std::string last_job_type_;
  int expected_retry_count_ = 0;
  const std::string request_uma_name_prefix_ =
      "Enterprise.DMServerRequestSuccess.";
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_ =
      base::StatisticsRecorder::CreateTemporaryForTesting();
  base::HistogramTester histogram_tester_;
};

struct FailedRequestParams {
  FailedRequestParams(DeviceManagementStatus expected_status,
                      net::Error error,
                      int http_status,
                      const std::string& response)
      : expected_status_(expected_status),
        error_(error),
        http_status_(http_status),
        response_(response) {}

  DeviceManagementStatus expected_status_;
  net::Error error_;
  int http_status_;
  std::string response_;
};

void PrintTo(const FailedRequestParams& params, std::ostream* os) {
  *os << "FailedRequestParams " << params.expected_status_ << " "
      << params.error_ << " " << params.http_status_;
}

// A parameterized test case for erroneous response situations, they're mostly
// the same for all kinds of requests.
class DeviceManagementServiceFailedRequestTest
    : public DeviceManagementServiceTestBase,
      public testing::WithParamInterface<FailedRequestParams> {};

TEST_P(DeviceManagementServiceFailedRequestTest, RegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, CertBasedRegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartCertBasedRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, TokenEnrollmentRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartTokenEnrollmentJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, ApiAuthCodeFetchRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartApiAuthCodeFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, UnregisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartUnregistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, PolicyRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, AutoEnrollmentRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartAutoEnrollmentJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, AppInstallReportRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartAppInstallReportJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

INSTANTIATE_TEST_SUITE_P(
    DeviceManagementServiceFailedRequestTestInstance,
    DeviceManagementServiceFailedRequestTest,
    testing::Values(
        FailedRequestParams(DM_STATUS_REQUEST_FAILED,
                            net::ERR_FAILED,
                            200,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_HTTP_STATUS_ERROR,
                            net::OK,
                            666,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED,
                            net::OK,
                            403,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
                            net::OK,
                            405,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_DEVICE_ID_CONFLICT,
                            net::OK,
                            409,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
                            net::OK,
                            410,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
                            net::OK,
                            401,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_REQUEST_INVALID,
                            net::OK,
                            400,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_TEMPORARY_UNAVAILABLE,
                            net::OK,
                            404,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_ACTIVATION_PENDING,
                            net::OK,
                            412,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_MISSING_LICENSES,
                            net::OK,
                            402,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(
            DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
            net::OK,
            417,
            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_REQUEST_TOO_LARGE,
                            net::OK,
                            413,
                            PROTO_STRING(kResponseEmpty))));

// Simple query parameter parser for testing.
class QueryParams {
 public:
  explicit QueryParams(const std::string& query) {
    base::SplitStringIntoKeyValuePairs(query, '=', '&', &params_);
  }

  // Returns true if there is exactly one query parameter with |name| and its
  // value is equal to |expected_value|.
  bool Check(const std::string& name, const std::string& expected_value) {
    std::vector<std::string> params = GetParams(name);
    return params.size() == 1 && params[0] == expected_value;
  }

  // Returns vector containing all values for the query param with |name|.
  std::vector<std::string> GetParams(const std::string& name) {
    std::vector<std::string> results;
    for (const auto& param : params_) {
      std::string unescaped_name = net::UnescapeBinaryURLComponent(
          param.first, net::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
      if (unescaped_name == name) {
        std::string value = net::UnescapeBinaryURLComponent(
            param.second, net::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
        results.push_back(value);
      }
    }
    return results;
  }

 private:
  typedef base::StringPairs ParamMap;
  ParamMap params_;
};

class DeviceManagementServiceTest
    : public DeviceManagementServiceTestBase {
 protected:
  void CheckURLAndQueryParams(
      const network::TestURLLoaderFactory::PendingRequest* request,
      const std::string& request_type,
      const std::string& device_id,
      const std::string& last_error,
      bool critical = false) {
    const GURL service_url(kServiceUrl);
    const auto& request_url = request->request.url;
    EXPECT_EQ(service_url.scheme(), request_url.scheme());
    EXPECT_EQ(service_url.host(), request_url.host());
    EXPECT_EQ(service_url.port(), request_url.port());
    EXPECT_EQ(service_url.path(), request_url.path());

    QueryParams query_params(request_url.query());
    EXPECT_TRUE(query_params.Check(dm_protocol::kParamRequest, request_type));
    EXPECT_TRUE(query_params.Check(dm_protocol::kParamDeviceID, device_id));
    EXPECT_TRUE(query_params.Check(dm_protocol::kParamDeviceType,
                                   dm_protocol::kValueDeviceType));
    EXPECT_TRUE(query_params.Check(dm_protocol::kParamAppType,
                                   dm_protocol::kValueAppType));
    EXPECT_EQ(critical,
              query_params.Check(dm_protocol::kParamCritical, "true"));
    if (last_error == "") {
      EXPECT_TRUE(query_params.Check(dm_protocol::kParamRetry, "false"));
    } else {
      EXPECT_TRUE(query_params.Check(dm_protocol::kParamRetry, "true"));
      EXPECT_TRUE(query_params.Check(dm_protocol::kParamLastError, last_error));
    }
  }
};

TEST_F(DeviceManagementServiceTest, RegisterRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->
      set_device_management_token(kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         "");

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, CriticalRequest) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartCriticalPolicyFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestPolicy, kClientID,
                         "", true);
}

TEST_F(DeviceManagementServiceTest, CertBasedRegisterRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->
      set_device_management_token(kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartCertBasedRegistrationJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestCertBasedRegister,
                         kClientID, "");

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, TokenEnrollmentRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartTokenEnrollmentJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestTokenEnrollment,
                         kClientID, "");

  // Make sure request is properly authorized.
  std::string header;
  ASSERT_TRUE(request->request.headers.GetHeader("Authorization", &header));
  EXPECT_EQ("GoogleEnrollmentToken token=enrollment_token", header);

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, ApiAuthCodeFetchRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_service_api_access_response()->set_auth_code(
      kRobotAuthCode);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartApiAuthCodeFetchJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestApiAuthorization,
                         kClientID, "");

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, UnregisterRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_unregister_response();
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartUnregistrationJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // Check the data the fetcher received.
  const GURL& request_url(request->request.url);
  const GURL service_url(kServiceUrl);
  EXPECT_EQ(service_url.scheme(), request_url.scheme());
  EXPECT_EQ(service_url.host(), request_url.host());
  EXPECT_EQ(service_url.port(), request_url.port());
  EXPECT_EQ(service_url.path(), request_url.path());

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestUnregister,
                         kClientID, "");

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, AppInstallReportRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_app_install_report_response();
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartAppInstallReportJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestAppInstallReport,
                         kClientID, "");

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, CancelRegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelCertBasedRegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartCertBasedRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelTokenEnrollmentRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartTokenEnrollmentJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelApiAuthCodeFetch) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartApiAuthCodeFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelUnregisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartUnregistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelPolicyRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelAppInstallReportRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartAppInstallReportJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, JobQueueing) {
  // Start with a non-initialized service.
  ResetService();

  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->
      set_device_management_token(kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);

  // Make a request. We should not see any fetchers being created.
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_FALSE(request);

  // Now initialize the service. That should start the job.
  InitializeService();
  request = GetPendingRequest();
  ASSERT_TRUE(request);

  // Check that the request is processed as expected.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, CancelRequestAfterShutdown) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // Shutdown the service and cancel the job afterwards.
  service_->Shutdown();
  request_job.reset();
}

ACTION_P(ResetPointer, pointer) {
  pointer->reset();
}

TEST_F(DeviceManagementServiceTest, CancelDuringCallback) {
  // Make a request.
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  EXPECT_CALL(*this, OnJobDone(_, _, _, _))
      .WillOnce(DoAll(ResetPointer(&request_job),
                      Invoke([this](DeviceManagementService::Job*,
                                    DeviceManagementStatus status,
                                    int net_error, const std::string&) {
                        VerifyMetrics(status, net_error);
                      })));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);

  // Generate a callback.
  SendResponse(net::OK, 500, std::string());

  // Job should have been reset.
  EXPECT_FALSE(request_job);
}

TEST_F(DeviceManagementServiceTest, RetryOnProxyError) {
  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry());

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  EXPECT_EQ(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  // Not a retry.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         "");
  const std::string upload_data(network::GetUploadData(request->request));

  // Generate a callback with a proxy failure.
  SendResponse(net::ERR_PROXY_CONNECTION_FAILED, 200, std::string());
  base::RunLoop().RunUntilIdle();

  // Verify that a new fetch was started that bypasses the proxy.
  request = GetPendingRequest(1);
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->request.load_flags & net::LOAD_BYPASS_PROXY);
  EXPECT_EQ(upload_data, network::GetUploadData(request->request));
  // Retry with last error net::ERR_PROXY_CONNECTION_FAILED.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         std::to_string(net::ERR_PROXY_CONNECTION_FAILED));
}

TEST_F(DeviceManagementServiceTest, RetryOnBadResponseFromProxy) {
  // Make a request and expect that it will not succeed.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry());

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  EXPECT_EQ(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  const GURL original_url(request->request.url);
  const std::string upload_data(network::GetUploadData(request->request));

  // Generate a callback with a valid http response, that was generated by
  // a bad/wrong proxy.
  SendResponse(net::OK, 200, std::string(), "bad/type",
               true /* was_fetched_via_proxy */);
  base::RunLoop().RunUntilIdle();

  // Verify that a new fetch was started that bypasses the proxy.
  request = GetPendingRequest(1);
  ASSERT_TRUE(request);
  EXPECT_NE(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  EXPECT_EQ(original_url, request->request.url);
  EXPECT_EQ(upload_data, network::GetUploadData(request->request));
}

TEST_F(DeviceManagementServiceTest, AcceptMimeTypeFromProxy) {
  // Make a request and expect that it will succeed.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(1);

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  EXPECT_EQ(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  const GURL original_url(request->request.url);
  const std::string upload_data(network::GetUploadData(request->request));

  // Generate a callback with a valid http response, containing a charset in the
  // Content-type header.
  SendResponse(net::OK, 200, std::string(), "application/x-protobuffer",
               true /* was_fetched_via_proxy */);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DeviceManagementServiceTest, RetryOnNetworkChanges) {
  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry());

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  // Not a retry.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         "");
  const std::string original_upload_data(
      network::GetUploadData(request->request));

  // Make it fail with ERR_NETWORK_CHANGED.
  SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());
  base::RunLoop().RunUntilIdle();

  // Verify that a new fetch was started that retries this job, after
  // having called OnJobRetry.
  Mock::VerifyAndClearExpectations(this);
  request = GetPendingRequest(1);
  ASSERT_TRUE(request);
  EXPECT_EQ(original_upload_data, network::GetUploadData(request->request));
  // Retry with last error net::ERR_NETWORK_CHANGED.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         std::to_string(net::ERR_NETWORK_CHANGED));
}

TEST_F(DeviceManagementServiceTest, PolicyFetchRetryImmediately) {
  // We must not wait before a policy fetch retry, so this must not time out.
  policy::DeviceManagementService::SetRetryDelayForTesting(60000);

  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry());

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyFetchJob());
  auto* request = GetPendingRequest();
  // Not a retry.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestPolicy, kClientID,
                         "");
  const std::string original_upload_data(
      network::GetUploadData(request->request));

  // Make it fail with ERR_NETWORK_CHANGED.
  SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());
  base::RunLoop().RunUntilIdle();

  // Verify that a new fetch was started that retries this job, after
  // having called OnJobRetry.
  Mock::VerifyAndClearExpectations(this);
  request = GetPendingRequest(1);
  ASSERT_TRUE(request);
  EXPECT_EQ(original_upload_data, network::GetUploadData(request->request));
  // Retry with last error net::ERR_NETWORK_CHANGED.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestPolicy, kClientID,
                         std::to_string(net::ERR_NETWORK_CHANGED));

  // Request is succeeded with retry.
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  expected_retry_count_ = 1;
  SendResponse(net::OK, 200, std::string());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceManagementServiceTest, RetryLimit) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());

  // Simulate 3 failed network requests.
  for (int i = 0; i < 3; ++i) {
    // Make the current fetcher fail with ERR_NETWORK_CHANGED.
    auto* request = GetPendingRequest(i);
    EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
    EXPECT_CALL(*this, OnJobRetry());
    if (i == 0) {
      // Not a retry.
      CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister,
                             kClientID, "");
    } else {
      // Retry with last error net::ERR_NETWORK_CHANGED.
      CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister,
                             kClientID,
                             std::to_string(net::ERR_NETWORK_CHANGED));
    }
    SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);
  }

  // At the next failure the DeviceManagementService should give up retrying and
  // pass the error code to the job's owner.
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_REQUEST_FAILED, _, _));
  EXPECT_CALL(*this, OnJobRetry()).Times(0);
  SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceManagementServiceTest, CancelDuringRetry) {
  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry());

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());

  // Make it fail with ERR_NETWORK_CHANGED.
  SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());

  // Before we retry, cancel the job
  request_job.reset();

  // We must not crash
  base::RunLoop().RunUntilIdle();
}

// Tests that authorization data is correctly added to the request.
class DeviceManagementRequestAuthTest : public DeviceManagementServiceTestBase {
 protected:
  DeviceManagementRequestAuthTest() = default;
  ~DeviceManagementRequestAuthTest() override = default;

  std::unique_ptr<DeviceManagementService::Job> StartJobWithAuthData(
      std::unique_ptr<DMAuth> auth,
      base::Optional<std::string> oauth_token) {
    EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _));
    EXPECT_CALL(*this, OnJobRetry()).Times(0);

    // Job type is not really relevant for the test.
    std::unique_ptr<DeviceManagementService::Job> job =
        StartJob(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
                 /*critical=*/false, auth ? std::move(auth) : DMAuth::NoAuth(),
                 oauth_token ? *oauth_token : base::Optional<std::string>());
    return job;
  }

  // Returns vector containing all values for the OAuth query param.
  std::vector<std::string> GetOAuthParams(
      const network::TestURLLoaderFactory::PendingRequest& request) {
    QueryParams query_params(request.request.url.query());
    return query_params.GetParams(dm_protocol::kParamOAuthToken);
  }

  // Returns the value of 'Authorization' header if found.
  base::Optional<std::string> GetAuthHeader(
      const network::TestURLLoaderFactory::PendingRequest& request) {
    std::string header;
    bool result =
        request.request.headers.GetHeader(dm_protocol::kAuthHeader, &header);
    return result ? base::Optional<std::string>(header) : base::nullopt;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceManagementRequestAuthTest);
};

TEST_F(DeviceManagementRequestAuthTest, OnlyOAuthToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(nullptr /* auth */, kOAuthToken));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  const std::vector<std::string> params = GetOAuthParams(*request);
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ(kOAuthToken, params[0]);
  EXPECT_FALSE(GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OnlyDMToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromDMToken(kDMToken),
                           base::nullopt /* oauth_token */));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  const std::vector<std::string> params = GetOAuthParams(*request);
  EXPECT_EQ(0u, params.size());
  EXPECT_EQ(base::StrCat({dm_protocol::kDMTokenAuthHeaderPrefix, kDMToken}),
            GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OnlyEnrollmentToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromEnrollmentToken(kEnrollmentToken),
                           base::nullopt /* oauth_token */));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  const std::vector<std::string> params = GetOAuthParams(*request);
  EXPECT_EQ(0u, params.size());
  EXPECT_EQ(base::StrCat({dm_protocol::kEnrollmentTokenAuthHeaderPrefix,
                          kEnrollmentToken}),
            GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OnlyGaiaToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromGaiaToken(kGaiaAuthToken),
                           base::nullopt /* oauth_token */));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  const std::vector<std::string> params = GetOAuthParams(*request);
  EXPECT_EQ(0u, params.size());
  EXPECT_EQ(base::StrCat(
                {dm_protocol::kServiceTokenAuthHeaderPrefix, kGaiaAuthToken}),
            GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OAuthAndDMToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromDMToken(kDMToken), kOAuthToken));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  std::vector<std::string> params = GetOAuthParams(*request);
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ(kOAuthToken, params[0]);
  EXPECT_EQ(base::StrCat({dm_protocol::kDMTokenAuthHeaderPrefix, kDMToken}),
            GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OAuthAndEnrollmentToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromEnrollmentToken(kEnrollmentToken),
                           kOAuthToken));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  std::vector<std::string> params = GetOAuthParams(*request);
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ(kOAuthToken, params[0]);
  EXPECT_EQ(base::StrCat({dm_protocol::kEnrollmentTokenAuthHeaderPrefix,
                          kEnrollmentToken}),
            GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OAuthAndGaiaToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromGaiaToken(kGaiaAuthToken), kOAuthToken));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  std::vector<std::string> params = GetOAuthParams(*request);
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ(kOAuthToken, params[0]);
  EXPECT_EQ(base::StrCat(
                {dm_protocol::kServiceTokenAuthHeaderPrefix, kGaiaAuthToken}),
            GetAuthHeader(*request));

  SendResponse(net::OK, 200, std::string());
}

#if defined(GTEST_HAS_DEATH_TEST)
TEST_F(DeviceManagementRequestAuthTest, CannotUseOAuthTokenAsAuthData) {
  // Job type is not really relevant for the test.
  ASSERT_DEATH(StartJobWithAuthData(DMAuth::FromOAuthToken(kOAuthToken), ""),
               "");
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace policy
