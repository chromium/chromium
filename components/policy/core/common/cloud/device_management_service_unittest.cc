// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/device_management_service.h"

#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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

#define PROTO_STRING(name) (std::string(name, std::size(name) - 1))

// Some helper constants.
const char kOAuthToken[] = "oauth-token";
const char kDMToken[] = "device-management-token";
const char kClientID[] = "device-id";
const char kRobotAuthCode[] = "robot-oauth-auth-code";
const char kEnrollmentToken[] = "enrollment_token";
const char kProfileID[] = "profile-id";
const char kIdToken[] = "id-token";
#if BUILDFLAG(IS_IOS)
const char kOAuthAuthorizationHeaderPrefix[] = "OAuth ";
#endif

// Helper function which generates a DMServer response and populates the
// `error_detail` field.
std::string GenerateResponseWithErrorDetail(
    em::DeviceManagementErrorDetail error_detail) {
  em::DeviceManagementResponse response;
  response.add_error_detail(error_detail);
  return response.SerializeAsString();
}

// Unit tests for the device management policy service. The tests are run
// against a TestURLLoaderFactory that is used to short-circuit the request
// without calling into the actual network stack.
class DeviceManagementServiceTestBase : public testing::Test {
 protected:
  explicit DeviceManagementServiceTestBase(
      base::test::TaskEnvironment::TimeSource time_source)
      : task_environment_(time_source) {
    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    ResetService();
    InitializeService();
  }

  DeviceManagementServiceTestBase()
      : DeviceManagementServiceTestBase(
            base::test::TaskEnvironment::TimeSource::DEFAULT) {}

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
    service_ =
        std::make_unique<DeviceManagementService>(std::move(configuration));
  }

  void InitializeService() {
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index = 0) {
    if (index >= url_loader_factory_.pending_requests()->size()) {
      return nullptr;
    }
    return &(*url_loader_factory_.pending_requests())[index];
  }

  std::unique_ptr<DeviceManagementService::Job> StartJob(
      DeviceManagementService::JobConfiguration::JobType type,
      bool critical,
      DMAuth auth_data,
      std::optional<std::string>&& oauth_token,
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY,
      base::TimeDelta timeout = base::Seconds(0)) {
    auto params = DMServerJobConfiguration::CreateParams::WithoutClient(
        type, service_.get(), kClientID, shared_url_loader_factory_);
    params.critical = critical;
    params.auth_data = std::move(auth_data);
    params.oauth_token = std::move(oauth_token);
    return StartJob(std::move(params), payload, method, timeout);
  }

  std::unique_ptr<DeviceManagementService::Job> StartJob(
      DMServerJobConfiguration::CreateParams params,
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY,
      base::TimeDelta timeout = base::Seconds(0)) {
    last_job_type_ =
        DeviceManagementService::JobConfiguration::GetJobTypeAsString(
            params.type);
    std::unique_ptr<FakeJobConfiguration> config =
        std::make_unique<FakeJobConfiguration>(
            std::move(params),
            base::BindOnce(&DeviceManagementServiceTestBase::OnJobDone,
                           base::Unretained(this)),
            base::BindRepeating(&DeviceManagementServiceTestBase::OnJobRetry,
                                base::Unretained(this)),
            base::BindRepeating(
                &DeviceManagementServiceTestBase::OnShouldJobRetry,
                base::Unretained(this)));
    config->SetRequestPayload(payload);
    config->SetShouldRetryResponse(method);
    config->SetTimeoutDuration(timeout);
    return service_->CreateJob(std::move(config));
  }

  std::unique_ptr<DeviceManagementService::Job> StartRegistrationJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
        /*critical=*/false, DMAuth::NoAuth(), kOAuthToken, payload, method);
  }

  std::unique_ptr<DeviceManagementService::Job> StartCertBasedRegistrationJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
        /*critical=*/false, DMAuth::NoAuth(), std::string(), payload, method);
  }

  std::unique_ptr<DeviceManagementService::Job> StartBrowserRegistrationJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY,
      base::TimeDelta timeout = base::Seconds(0)) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_BROWSER_REGISTRATION,
        /*critical=*/false, DMAuth::FromEnrollmentToken(kEnrollmentToken),
        std::string(), payload, method, timeout);
  }

  std::unique_ptr<DeviceManagementService::Job> StartPolicyAgentRegistrationJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY,
      base::TimeDelta timeout = base::Seconds(0)) {
    return StartJob(DeviceManagementService::JobConfiguration::
                        TYPE_POLICY_AGENT_REGISTRATION,
                    /*critical=*/false,
                    DMAuth::FromEnrollmentToken(kEnrollmentToken),
                    std::string(), payload, method, timeout);
  }

  std::unique_ptr<DeviceManagementService::Job> StartOidcEnrollmentJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY,
      base::TimeDelta timeout = base::Seconds(0)) {
    auto params = DMServerJobConfiguration::CreateParams::WithoutClient(
        DeviceManagementService::JobConfiguration::TYPE_OIDC_REGISTRATION,
        service_.get(), kClientID, shared_url_loader_factory_);
    params.critical = false;
    params.oauth_token = kOAuthToken;
    params.auth_data = DMAuth::FromOidcResponse(kIdToken);
    params.profile_id = kProfileID;
    return StartJob(std::move(params), payload, method, timeout);
  }

  std::unique_ptr<DeviceManagementService::Job> StartApiAuthCodeFetchJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
        /*critical=*/false, DMAuth::NoAuth(), kOAuthToken, payload, method);
  }

  std::unique_ptr<DeviceManagementService::Job> StartUnregistrationJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
        /*critical=*/false, DMAuth::FromDMToken(kDMToken), std::string(),
        payload, method);
  }

  std::unique_ptr<DeviceManagementService::Job> StartPolicyFetchJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
        /*critical=*/false, DMAuth::NoAuth(), kOAuthToken, payload, method);
  }

  std::unique_ptr<DeviceManagementService::Job> StartCriticalPolicyFetchJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
        /*critical=*/true, DMAuth::NoAuth(), kOAuthToken, payload, method);
  }

  std::unique_ptr<DeviceManagementService::Job> StartAutoEnrollmentJob(
      const std::string& payload = std::string(),
      DeviceManagementService::Job::RetryMethod method =
          DeviceManagementService::Job::NO_RETRY) {
    return StartJob(
        DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
        /*critical=*/false, DMAuth::NoAuth(), std::string(), payload, method);
  }

  void SendResponse(net::Error net_error,
                    int http_status,
                    const std::string& response,
                    size_t request_index = 0u,
                    const std::string& mime_type = std::string(),
                    bool was_fetched_via_proxy = false) {
    const auto* request = GetPendingRequest(request_index);
    ASSERT_TRUE(request);

    // Note: We cannot use `network::CreateURLResponseHead` because we are using
    // some unconventional HTTP status codes, which trigger NOTREACHED in
    // `net::GetHttpReasonPhrase`.
    auto head = network::mojom::URLResponseHead::New();
    std::string status_line(
        base::StringPrintf("HTTP/1.1 %d Something", http_status));
    std::string headers = status_line + "\n" +
                          net::HttpRequestHeaders::kContentType +
                          ": text/html\n\n";
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));

    if (was_fetched_via_proxy) {
      head->proxy_chain = net::ProxyChain(
          net::ProxyServer::Scheme::SCHEME_HTTPS, /*host_port_pair=*/{});
    }
    head->mime_type = mime_type;
    network::URLLoaderCompletionStatus status(net_error);

    // This wakes up pending url loaders on this url.
    url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                    response, status);

    // Unset the response to allow for potential retry attempts
    url_loader_factory_.ClearResponses();

    // Finish SimpleURLLoader::DownloadToStringOfUnboundedSizeUntilCrashAndDie
    base::RunLoop().RunUntilIdle();
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
                    int /*net_error*/,
                    const std::string&));

  MOCK_METHOD2(OnJobRetry,
               void(int response_code, const std::string& response_body));

  MOCK_METHOD2(OnShouldJobRetry,
               void(int response_code, const std::string& response_body));

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
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, CertBasedRegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartCertBasedRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, BrowserRegistrationRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartBrowserRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest,
       PolicyAgentRegistrationRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyAgentRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, OidcEnrollmentRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartOidcEnrollmentJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, ApiAuthCodeFetchRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartApiAuthCodeFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, PolicyRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

TEST_P(DeviceManagementServiceFailedRequestTest, AutoEnrollmentRequest) {
  EXPECT_CALL(*this, OnJobDone(_, GetParam().expected_status_, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnShouldJobRetry(GetParam().http_status_, GetParam().response_));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartAutoEnrollmentJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  SendResponse(GetParam().error_, GetParam().http_status_,
               GetParam().response_);
}

INSTANTIATE_TEST_SUITE_P(
    DeviceManagementServiceFailedRequestTestInstance,
    DeviceManagementServiceFailedRequestTest,
    testing::Values(
        FailedRequestParams(DM_STATUS_REQUEST_FAILED, net::ERR_FAILED, 0, ""),
        FailedRequestParams(DM_STATUS_REQUEST_FAILED,
                            net::ERR_TIMED_OUT,
                            0,
                            ""),
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
        FailedRequestParams(
            DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
            net::OK,
            410,
            GenerateResponseWithErrorDetail(
                em::CBCM_DELETION_POLICY_PREFERENCE_INVALIDATE_TOKEN)),
        FailedRequestParams(
#if BUILDFLAG(IS_CHROMEOS)
            DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
#else   // BUILDFLAG(IS_CHROMEOS)
            DM_STATUS_SERVICE_DEVICE_NEEDS_RESET,
#endif  // BUILDFLAG(IS_CHROMEOS)
            net::OK,
            410,
            GenerateResponseWithErrorDetail(
                em::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN)),
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
        FailedRequestParams(
            DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL,
            net::OK,
            905,
            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(
            DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED,
            net::OK,
            906,
            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(
            DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE,
            net::OK,
            907,
            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_REQUEST_TOO_LARGE,
                            net::OK,
                            413,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK,
                            net::OK,
                            418,
                            PROTO_STRING(kResponseEmpty)),
        FailedRequestParams(DM_STATUS_SERVICE_TOO_MANY_REQUESTS,
                            net::OK,
                            429,
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
      std::string unescaped_name = base::UnescapeBinaryURLComponent(
          param.first, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
      if (unescaped_name == name) {
        std::string value = base::UnescapeBinaryURLComponent(
            param.second, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
        results.push_back(value);
      }
    }
    return results;
  }

 private:
  typedef base::StringPairs ParamMap;
  ParamMap params_;
};

class DeviceManagementServiceTest : public DeviceManagementServiceTestBase {
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
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));
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
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));
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

TEST_F(DeviceManagementServiceTest, BrowserRegistrationRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartBrowserRegistrationJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegisterBrowser,
                         kClientID, "");

  // Make sure request is properly authorized.
  EXPECT_EQ("GoogleEnrollmentToken token=enrollment_token",
            request->request.headers.GetHeader("Authorization"));

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, PolicyAgentRegistrationRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyAgentRegistrationJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegisterPolicyAgent,
                         kClientID, "");

  // Make sure request is properly authorized.
  EXPECT_EQ("GoogleEnrollmentToken token=enrollment_token",
            request->request.headers.GetHeader("Authorization"));

  EXPECT_EQ(expected_data, network::GetUploadData(request->request));

  // Generate the response.
  SendResponse(net::OK, 200, expected_data);
}

TEST_F(DeviceManagementServiceTest, OidcEnrollmentRequest) {
  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartOidcEnrollmentJob(expected_data));
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegisterProfile,
                         kClientID, "");

  // Make sure request is properly authorized.
  EXPECT_EQ("GoogleDM3PAuth oauth_token=oauth-token, id_token=id-token",
            request->request.headers.GetHeader("Authorization"));

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
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));
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

TEST_F(DeviceManagementServiceTest, RequestWithProfileId) {
  auto params = DMServerJobConfiguration::CreateParams::WithoutClient(
      DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
      service_.get(), kClientID, shared_url_loader_factory_);
  params.oauth_token = kOAuthToken;
  params.profile_id = kProfileID;
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, std::string()));
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJob(std::move(params)));

  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  QueryParams query_params(request->request.url.query());
  EXPECT_TRUE(query_params.Check(dm_protocol::kParamProfileID, kProfileID));

  // Generate the response.
  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementServiceTest, CancelRegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelCertBasedRegisterRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartCertBasedRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelBrowserRegistrationRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartBrowserRegistrationJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelOidcEnrollmentRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartOidcEnrollmentJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelApiAuthCodeFetch) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartApiAuthCodeFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, CancelPolicyRequest) {
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartPolicyFetchJob());
  auto* request = GetPendingRequest();
  ASSERT_TRUE(request);

  // There shouldn't be any callbacks.
  request_job.reset();
}

TEST_F(DeviceManagementServiceTest, JobQueueing) {
  // Start with a non-initialized service.
  ResetService();

  em::DeviceManagementResponse expected_response;
  expected_response.mutable_register_response()->set_device_management_token(
      kDMToken);
  std::string expected_data;
  ASSERT_TRUE(expected_response.SerializeToString(&expected_data));

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, expected_data));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, expected_data));

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
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
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
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(500, std::string()));

  // Generate a callback.
  SendResponse(net::OK, 500, std::string());

  // Job should have been reset.
  EXPECT_FALSE(request_job);
}

TEST_F(DeviceManagementServiceTest, RetryOnProxyError) {
  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(0, std::string()));
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  EXPECT_EQ(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  // Not a retry.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         "");
  const std::string upload_data(network::GetUploadData(request->request));

  // Generate a callback with a proxy failure.
  SendResponse(net::ERR_PROXY_CONNECTION_FAILED, 0, std::string());

  // Verify that a new fetch was started that bypasses the proxy.
  request = GetPendingRequest();
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->request.load_flags & net::LOAD_BYPASS_PROXY);
  EXPECT_EQ(upload_data, network::GetUploadData(request->request));
  // Retry with last error net::ERR_PROXY_CONNECTION_FAILED.
  CheckURLAndQueryParams(
      request, dm_protocol::kValueRequestRegister, kClientID,
      base::NumberToString(net::ERR_PROXY_CONNECTION_FAILED));
}

TEST_F(DeviceManagementServiceTest, RetryOnBadResponseFromProxy) {
  // Make a request and expect that it will not succeed.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(200, std::string()));
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  EXPECT_EQ(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  const GURL original_url(request->request.url);
  const std::string upload_data(network::GetUploadData(request->request));

  // Generate a callback with a valid http response, that was generated by
  // a bad/wrong proxy.
  SendResponse(net::OK, 200, std::string(), 0u, "bad/type",
               true /* was_fetched_via_proxy */);

  // Verify that a new fetch was started that bypasses the proxy.
  request = GetPendingRequest();
  ASSERT_TRUE(request);
  EXPECT_NE(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  EXPECT_EQ(original_url, request->request.url);
  EXPECT_EQ(upload_data, network::GetUploadData(request->request));
}

TEST_F(DeviceManagementServiceTest, AcceptMimeTypeFromProxy) {
  // Make a request and expect that it will succeed.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(1);
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());
  auto* request = GetPendingRequest();
  EXPECT_EQ(0, request->request.load_flags & net::LOAD_BYPASS_PROXY);
  const GURL original_url(request->request.url);
  const std::string upload_data(network::GetUploadData(request->request));

  // Generate a callback with a valid http response, containing a charset in the
  // Content-type header.
  SendResponse(net::OK, 200, std::string(), 0u, "application/x-protobuffer",
               true /* was_fetched_via_proxy */);
}

TEST_F(DeviceManagementServiceTest, RetryOnNetworkChanges) {
  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(0, std::string()));
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);

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

  // Verify that a new fetch was started that retries this job, after
  // having called OnJobRetry.
  Mock::VerifyAndClearExpectations(this);
  request = GetPendingRequest();
  ASSERT_TRUE(request);
  EXPECT_EQ(original_upload_data, network::GetUploadData(request->request));
  // Retry with last error net::ERR_NETWORK_CHANGED.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister, kClientID,
                         base::NumberToString(net::ERR_NETWORK_CHANGED));
}

TEST_F(DeviceManagementServiceTest, PolicyFetchRetryImmediately) {
  // We must not wait before a policy fetch retry, so this must not time out.
  policy::DeviceManagementService::SetRetryDelayForTesting(60000);

  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(0, std::string()));
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);

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

  // Verify that a new fetch was started that retries this job, after
  // having called OnJobRetry.
  Mock::VerifyAndClearExpectations(this);
  request = GetPendingRequest();
  ASSERT_TRUE(request);
  EXPECT_EQ(original_upload_data, network::GetUploadData(request->request));
  // Retry with last error net::ERR_NETWORK_CHANGED.
  CheckURLAndQueryParams(request, dm_protocol::kValueRequestPolicy, kClientID,
                         base::NumberToString(net::ERR_NETWORK_CHANGED));

  // Request is succeeded with retry.
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  EXPECT_CALL(*this, OnShouldJobRetry(_, _));
  expected_retry_count_ = 1;
  SendResponse(net::OK, 200, std::string());
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceManagementServiceTest, RetryLimit) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartRegistrationJob());

  // Simulate `DeviceManagementService::kMaxRetries` failed network requests.
  for (int i = 0; i < DeviceManagementService::kMaxRetries; ++i) {
    // Make the current fetcher fail with ERR_NETWORK_CHANGED.
    auto* request = GetPendingRequest();
    ASSERT_TRUE(request);
    EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
    EXPECT_CALL(*this, OnJobRetry(0, std::string()));
    EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);
    if (i == 0) {
      // Not a retry.
      CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister,
                             kClientID, "");
    } else {
      // Retry with last error net::ERR_NETWORK_CHANGED.
      CheckURLAndQueryParams(request, dm_protocol::kValueRequestRegister,
                             kClientID,
                             base::NumberToString(net::ERR_NETWORK_CHANGED));
    }
    SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());
    Mock::VerifyAndClearExpectations(this);
  }

  // At the next failure the DeviceManagementService should give up retrying and
  // pass the error code to the job's owner.
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_REQUEST_FAILED, _, _));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);
  SendResponse(net::ERR_NETWORK_CHANGED, 0, std::string());
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceManagementServiceTest, CancelDuringRetry) {
  // Make a request.
  EXPECT_CALL(*this, OnJobDone(_, _, _, _)).Times(0);
  EXPECT_CALL(*this, OnJobRetry(0, std::string()));
  EXPECT_CALL(*this, OnShouldJobRetry(_, _)).Times(0);

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
 public:
  DeviceManagementRequestAuthTest(const DeviceManagementRequestAuthTest&) =
      delete;
  DeviceManagementRequestAuthTest& operator=(
      const DeviceManagementRequestAuthTest&) = delete;

 protected:
  DeviceManagementRequestAuthTest() = default;
  ~DeviceManagementRequestAuthTest() override = default;

  std::unique_ptr<DeviceManagementService::Job> StartJobWithAuthData(
      DMAuth auth,
      std::optional<std::string> oauth_token) {
    EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _));
    EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);

    // Job type is not really relevant for the test.
    std::unique_ptr<DeviceManagementService::Job> job =
        StartJob(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
                 /*critical=*/false, std::move(auth),
                 oauth_token ? *oauth_token : std::optional<std::string>());
    return job;
  }

  // Returns vector containing all values for the OAuth query param.
  std::vector<std::string> GetOAuthParams(
      const network::TestURLLoaderFactory::PendingRequest& request) {
    QueryParams query_params(request.request.url.query());
    return query_params.GetParams(dm_protocol::kParamOAuthToken);
  }

  // Returns the value of 'Authorization' header if found.
  std::optional<std::string> GetAuthHeader(
      const network::TestURLLoaderFactory::PendingRequest& request) {
    return request.request.headers.GetHeader(dm_protocol::kAuthHeader);
  }
};

TEST_F(DeviceManagementRequestAuthTest, OnlyOAuthToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::NoAuth(), kOAuthToken));
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(base::StrCat({kOAuthAuthorizationHeaderPrefix, kOAuthToken}),
            GetAuthHeader(*request));
  EXPECT_TRUE(GetOAuthParams(*request).empty());
#else
  const std::vector<std::string> params = GetOAuthParams(*request);
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ(kOAuthToken, params[0]);
  EXPECT_FALSE(GetAuthHeader(*request));
#endif

  SendResponse(net::OK, 200, std::string());
}

TEST_F(DeviceManagementRequestAuthTest, OnlyDMToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromDMToken(kDMToken),
                           std::nullopt /* oauth_token */));
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

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
                           std::nullopt /* oauth_token */));
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

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

#if !BUILDFLAG(IS_IOS)
// Cannot test requests with an oauth token and another authorization token on
// iOS because they both use the "Authorization" header.

TEST_F(DeviceManagementRequestAuthTest, OAuthAndDMToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromDMToken(kDMToken), kOAuthToken));
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

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
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

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

TEST_F(DeviceManagementRequestAuthTest, OidcAuthAndIdToken) {
  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartJobWithAuthData(DMAuth::FromOidcResponse(kIdToken), kOAuthToken));
  EXPECT_CALL(*this, OnShouldJobRetry(200, std::string()));

  const network::TestURLLoaderFactory::PendingRequest* request =
      GetPendingRequest();
  ASSERT_TRUE(request);

  std::vector<std::string> params = GetOAuthParams(*request);
  ASSERT_EQ(0u, params.size());
  EXPECT_EQ(
      base::StrCat({dm_protocol::kOidcAuthHeaderPrefix,
                    dm_protocol::kOidcAuthTokenHeaderPrefix, kOAuthToken, ",",
                    dm_protocol::kOidcIdTokenHeaderPrefix, kIdToken}),
      GetAuthHeader(*request));
  QueryParams query_params(request->request.url.query());

  SendResponse(net::OK, 200, std::string());
}
#endif

#if defined(GTEST_HAS_DEATH_TEST)
TEST_F(DeviceManagementRequestAuthTest, CannotUseOAuthTokenAsAuthData) {
  // Job type is not really relevant for the test.
  ASSERT_DEATH(StartJobWithAuthData(DMAuth::FromOAuthToken(kOAuthToken), ""),
               "");
}
#endif  // GTEST_HAS_DEATH_TEST

class DeviceManagementServiceTestWithTimeManipulation
    : public DeviceManagementServiceTestBase {
 protected:
  DeviceManagementServiceTestWithTimeManipulation()
      : DeviceManagementServiceTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  base::TimeDelta GetTimeoutDuration() const { return timeout_test_duration_; }
  static constexpr base::TimeDelta timeout_test_duration_ = base::Seconds(30);
};

TEST_F(DeviceManagementServiceTestWithTimeManipulation,
       BrowserRegistrationRequestWithTimeout) {
  // In enrollment timeout cases, expected status is DM_STATUS_REQUEST_FAILED,
  // and expected net error is NET_ERROR(TIMED_OUT, -7)
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_REQUEST_FAILED, _, ""));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartBrowserRegistrationJob("", DeviceManagementService::Job::NO_RETRY,
                                  GetTimeoutDuration()));
  ASSERT_TRUE(GetPendingRequest());

  // fast forward 30+ seconds
  task_environment_.FastForwardBy(GetTimeoutDuration() + base::Seconds(1));
}

TEST_F(DeviceManagementServiceTestWithTimeManipulation,
       OidcEnrollmentRequestWithTimeout) {
  // In enrollment timeout cases, expected status is DM_STATUS_REQUEST_FAILED,
  // and expected net error is NET_ERROR(TIMED_OUT, -7)
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_REQUEST_FAILED, _, ""));
  EXPECT_CALL(*this, OnJobRetry(_, _)).Times(0);

  std::unique_ptr<DeviceManagementService::Job> request_job(
      StartOidcEnrollmentJob("", DeviceManagementService::Job::NO_RETRY,
                             GetTimeoutDuration()));
  ASSERT_TRUE(GetPendingRequest());

  // fast forward 30+ seconds
  task_environment_.FastForwardBy(GetTimeoutDuration() + base::Seconds(1));
}

}  // namespace policy
