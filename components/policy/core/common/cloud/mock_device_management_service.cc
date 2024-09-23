// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_device_management_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "net/base/net_errors.h"

namespace policy {
namespace {

const char kServerUrl[] = "https://example.com/management_service";
const char kUserAgent[] = "Chrome 1.2.3(456)";
const char kPlatform[] = "Test|Unit|1.2.3";

std::string Serialize(
    const enterprise_management::DeviceManagementResponse& response) {
  // SerializeToString() may fail, that's OK.  Some tests explicitly use
  // malformed responses.
  std::string payload;
  if (response.IsInitialized())
    response.SerializeToString(&payload);
  return payload;
}

}  // namespace

MockDeviceManagementServiceConfiguration::
    MockDeviceManagementServiceConfiguration()
    : server_url_(kServerUrl) {}

MockDeviceManagementServiceConfiguration::
    MockDeviceManagementServiceConfiguration(const std::string& server_url)
    : server_url_(server_url) {}

MockDeviceManagementServiceConfiguration::
    ~MockDeviceManagementServiceConfiguration() = default;

std::string MockDeviceManagementServiceConfiguration::GetDMServerUrl() const {
  return server_url_;
}

std::string MockDeviceManagementServiceConfiguration::GetAgentParameter()
    const {
  return kUserAgent;
}

std::string MockDeviceManagementServiceConfiguration::GetPlatformParameter()
    const {
  return kPlatform;
}

std::string
MockDeviceManagementServiceConfiguration::GetRealtimeReportingServerUrl()
    const {
  return server_url_;
}

std::string
MockDeviceManagementServiceConfiguration::GetEncryptedReportingServerUrl()
    const {
  return server_url_;
}

MockJobCreationHandler::MockJobCreationHandler() = default;
MockJobCreationHandler::~MockJobCreationHandler() = default;

FakeDeviceManagementService::FakeDeviceManagementService(
    MockJobCreationHandler* creation_handler)
    : FakeDeviceManagementService(
          std::make_unique<MockDeviceManagementServiceConfiguration>(),
          creation_handler) {}

FakeDeviceManagementService::FakeDeviceManagementService(
    std::unique_ptr<Configuration> config,
    MockJobCreationHandler* creation_handler)
    : DeviceManagementService(std::move(config)),
      creation_handler_(creation_handler) {
  CHECK(creation_handler_);
}

FakeDeviceManagementService::~FakeDeviceManagementService() = default;

std::unique_ptr<DeviceManagementService::Job>
FakeDeviceManagementService::CreateJob(
    std::unique_ptr<JobConfiguration> config) {
  auto job_pair = CreateJobForTesting(std::move(config));
  creation_handler_->OnJobCreation(job_pair.second);
  return std::move(job_pair.first);
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::CaptureAuthData(DMAuth* auth_data) {
  return [auth_data](DeviceManagementService::JobForTesting job) mutable {
    if (job.IsActive())
      *auth_data = job.GetConfigurationForTesting()->GetAuth().Clone();
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::CaptureJobType(
    DeviceManagementService::JobConfiguration::JobType* job_type) {
  return [job_type](DeviceManagementService::JobForTesting job) mutable {
    if (job.IsActive())
      *job_type = job.GetConfigurationForTesting()->GetType();
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::CapturePayload(std::string* payload) {
  return [payload](DeviceManagementService::JobForTesting job) mutable {
    if (job.IsActive())
      *payload = job.GetConfigurationForTesting()->GetPayload();
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::CaptureQueryParams(
    std::map<std::string, std::string>* query_params) {
  return [query_params](DeviceManagementService::JobForTesting job) mutable {
    if (job.IsActive())
      *query_params = job.GetConfigurationForTesting()->GetQueryParams();
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::CaptureRequest(
    enterprise_management::DeviceManagementRequest* request) {
  return [request](DeviceManagementService::JobForTesting job) mutable {
    if (job.IsActive()) {
      const std::string payload =
          job.GetConfigurationForTesting()->GetPayload();
      CHECK(request->ParseFromString(payload));
    }
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::CaptureTimeout(base::TimeDelta* timeout) {
  return [timeout](DeviceManagementService::JobForTesting job) mutable {
    if (job.IsActive()) {
      auto to = job.GetConfigurationForTesting()->GetTimeoutDuration();
      if (to)
        *timeout = to.value();
    }
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::SendJobResponseAsync(int net_error,
                                                  int response_code,
                                                  const std::string& response,
                                                  const std::string& mime_type,
                                                  bool was_fetched_via_proxy) {
  // Note: We need to use a WeakPtr<Job> here, because some tests might destroy
  // pending jobs, e.g. CloudPolicyClientTest, CancelUploadAppInstallReport.
  // And base::WeakPtr cannot bind to non-void functions.
  // Thus, we need the redirect to SendWeakJobResponseNow.
  return [=, this](DeviceManagementService::JobForTesting job) {
    this->GetTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([=]() mutable {
          if (job.IsActive()) {
            job.SetResponseForTesting(net_error, response_code, response,
                                      mime_type, was_fetched_via_proxy);
          } else
            LOG(WARNING) << "job inactive";
        }));
  };
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::SendJobResponseAsync(
    int net_error,
    int response_code,
    const enterprise_management::DeviceManagementResponse& response,
    const std::string& mime_type,
    bool was_fetched_via_proxy) {
  return SendJobResponseAsync(net_error, response_code, Serialize(response),
                              mime_type, was_fetched_via_proxy);
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::SendJobOKAsync(const std::string& response) {
  return SendJobResponseAsync(net::OK, DeviceManagementService::kSuccess,
                              response);
}

FakeDeviceManagementService::JobAction
FakeDeviceManagementService::SendJobOKAsync(
    const enterprise_management::DeviceManagementResponse& response) {
  return SendJobOKAsync(Serialize(response));
}

void FakeDeviceManagementService::SendJobResponseNow(
    DeviceManagementService::JobForTesting* job,
    int net_error,
    int response_code,
    const std::string& response,
    const std::string& mime_type,
    bool was_fetched_via_proxy) {
  CHECK(job);
  if (job->SetResponseForTesting(net_error, response_code, response, mime_type,
                                 was_fetched_via_proxy) ==
      Job::RetryMethod::NO_RETRY) {
    job->Deactivate();
  }
}

void FakeDeviceManagementService::SendJobResponseNow(
    DeviceManagementService::JobForTesting* job,
    int net_error,
    int response_code,
    const enterprise_management::DeviceManagementResponse& response,
    const std::string& mime_type,
    bool was_fetched_via_proxy) {
  SendJobResponseNow(job, net_error, response_code, Serialize(response),
                     mime_type, was_fetched_via_proxy);
}

void FakeDeviceManagementService::SendJobOKNow(
    DeviceManagementService::JobForTesting* job,
    const std::string& response) {
  SendJobResponseNow(job, net::OK, kSuccess, response);
}

void FakeDeviceManagementService::SendJobOKNow(
    DeviceManagementService::JobForTesting* job,
    const enterprise_management::DeviceManagementResponse& response) {
  SendJobOKNow(job, Serialize(response));
}

FakeJobConfiguration::FakeJobConfiguration(
    DMServerJobConfiguration::CreateParams params,
    FakeCallback callback,
    RetryCallback retry_callback,
    RetryCallback should_retry_callback)
    : DMServerJobConfiguration(std::move(params)),
      should_retry_response_(DeviceManagementService::Job::NO_RETRY),
      callback_(std::move(callback)),
      retry_callback_(retry_callback),
      should_retry_callback_(should_retry_callback) {
  DCHECK(!callback_.is_null());
  DCHECK(!retry_callback_.is_null());
}

FakeJobConfiguration::~FakeJobConfiguration() = default;

void FakeJobConfiguration::SetRequestPayload(
    const std::string& request_payload) {
  request()->ParseFromString(request_payload);
}

void FakeJobConfiguration::SetShouldRetryResponse(
    DeviceManagementService::Job::RetryMethod method) {
  should_retry_response_ = method;
}

void FakeJobConfiguration::SetTimeoutDuration(base::TimeDelta timeout) {
  timeout_ = timeout;
}

DeviceManagementService::Job::RetryMethod FakeJobConfiguration::ShouldRetry(
    int response_code,
    const std::string& response_body) {
  should_retry_callback_.Run(response_code, response_body);
  return should_retry_response_;
}

void FakeJobConfiguration::OnBeforeRetry(int response_code,
                                         const std::string& response_body) {
  retry_callback_.Run(response_code, response_body);
}

void FakeJobConfiguration::OnURLLoadComplete(DeviceManagementService::Job* job,
                                             int net_error,
                                             int response_code,
                                             const std::string& response_body) {
  DeviceManagementStatus status =
      MapNetErrorAndResponseToDMStatus(net_error, response_code, response_body);
  std::move(callback_).Run(job, status, net_error, response_body);
}

}  // namespace policy
