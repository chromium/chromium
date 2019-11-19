// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_device_management_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {
namespace {

const char kServerUrl[] = "https://example.com/management_service";
const char kUserAgent[] = "Chrome 1.2.3(456)";
const char kPlatform[] = "Test|Unit|1.2.3";

void DoURLCompletion(base::WeakPtr<DeviceManagementService> service,
                     base::WeakPtr<DeviceManagementService::JobControl> job,
                     int net_error,
                     int response_code,
                     const std::string& payload) {
  if (!job || !service)
    return;

  MockDeviceManagementService* mock_service =
      static_cast<MockDeviceManagementService*>(service.get());
  DeviceManagementService::JobControl* job_local = job.get();
  mock_service->DoURLCompletionWithPayload(&job_local, net_error, response_code,
                                           payload);
}

}  // namespace

ACTION_P5(CreateAsyncAction,
          service,
          task_runner,
          net_error,
          response_code,
          payload) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DoURLCompletion, service->GetWeakPtr(),
                     arg0->GetWeakPtr(), net_error, response_code, payload));
}

ACTION_P(CreateFullControlAction, job_control) {
  // Using StartJobFullControl() with a WillRepeatedly() expectation should
  // be invalid if the action happens more than once.
  CHECK_EQ(nullptr, *job_control);

  *job_control = arg0;
}

ACTION_P(CreateCaptureJobTypeAction, job_type) {
  *job_type = arg0->GetConfiguration()->GetType();
}

ACTION_P(CreateCaptureQuertyParamsAction, params) {
  *params = arg0->GetConfiguration()->GetQueryParams();
}

ACTION_P(CreateCaptureRequestAction, request) {
  std::string payload = arg0->GetConfiguration()->GetPayload();
  CHECK(request->ParseFromString(payload));
}

ACTION_P(CreateCapturePayloadAction, payload) {
  *payload = arg0->GetConfiguration()->GetPayload();
}

MockDeviceManagementServiceConfiguration::
    MockDeviceManagementServiceConfiguration()
    : server_url_(kServerUrl) {}

MockDeviceManagementServiceConfiguration::
    MockDeviceManagementServiceConfiguration(const std::string& server_url)
    : server_url_(server_url) {}

MockDeviceManagementServiceConfiguration::
    ~MockDeviceManagementServiceConfiguration() {}

std::string MockDeviceManagementServiceConfiguration::GetDMServerUrl() {
  return server_url_;
}

std::string MockDeviceManagementServiceConfiguration::GetAgentParameter() {
  return kUserAgent;
}

std::string MockDeviceManagementServiceConfiguration::GetPlatformParameter() {
  return kPlatform;
}

std::string MockDeviceManagementServiceConfiguration::GetReportingServerUrl() {
  return server_url_;
}

MockDeviceManagementService::MockDeviceManagementService()
    : DeviceManagementService(std::unique_ptr<Configuration>(
          new MockDeviceManagementServiceConfiguration)) {}

MockDeviceManagementService::~MockDeviceManagementService() {}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::StartJobOKAsync(
    const enterprise_management::DeviceManagementResponse& response) {
  // SerializeToString() may fail, that's OK.  Some tests explicitly use
  // malformed responses.
  std::string payload;
  if (response.IsInitialized())
    response.SerializeToString(&payload);

  return StartJobAsync(net::OK, kSuccess, payload);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::StartJobAsync(
    int net_error,
    int response_code,
    const enterprise_management::DeviceManagementResponse& response) {
  // SerializeToString() may fail, that's OK.  Some tests explicitly use
  // malformed responses.
  std::string payload;
  if (response.IsInitialized())
    response.SerializeToString(&payload);

  return StartJobAsync(net_error, response_code, payload);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::StartJobAsync(int net_error,
                                           int response_code,
                                           const std::string& payload) {
  return CreateAsyncAction(this, task_runner(), net_error, response_code,
                           payload);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::StartJobFullControl(JobControl** job_control) {
  return CreateFullControlAction(job_control);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::CaptureJobType(
    DeviceManagementService::JobConfiguration::JobType* job_type) {
  return CreateCaptureJobTypeAction(job_type);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::CaptureQueryParams(
    DeviceManagementService::JobConfiguration::ParameterMap* params) {
  return CreateCaptureQuertyParamsAction(params);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::CaptureRequest(
    enterprise_management::DeviceManagementRequest* request) {
  return CreateCaptureRequestAction(request);
}

testing::Action<MockDeviceManagementService::StartJobFunction>
MockDeviceManagementService::CapturePayload(std::string* payload) {
  return CreateCapturePayloadAction(payload);
}

// Call after using StartJobFullControl() to respond to the network request.
void MockDeviceManagementService::DoURLCompletion(
    JobControl** job,
    int net_error,
    int response_code,
    const enterprise_management::DeviceManagementResponse& response) {
  if (!job || !*job)
    return;

  // SerializeToString() may fail, that's OK.  Some tests explicitly use
  // malformed responses.
  std::string payload;
  if (response.IsInitialized())
    response.SerializeToString(&payload);

  DoURLCompletionWithPayload(job, net_error, response_code, payload);
}

void MockDeviceManagementService::DoURLCompletionForBinding(
    JobControl* job,
    int net_error,
    int response_code,
    const enterprise_management::DeviceManagementResponse& response) {
  DoURLCompletion(&job, net_error, response_code, response);
}

void MockDeviceManagementService::DoURLCompletionWithPayload(
    JobControl** job,
    int net_error,
    int response_code,
    const std::string& payload) {
  JobControl::RetryMethod retry_method =
      DoURLCompletionInternal(*job, net_error, response_code, payload);
  if (retry_method == JobControl::NO_RETRY)
    *job = nullptr;
}

MockDeviceManagementService::JobControl::RetryMethod
MockDeviceManagementService::DoURLCompletionInternal(
    JobControl* job,
    int net_error,
    int response_code,
    const std::string& payload) {
  if (!job)
    return JobControl::NO_RETRY;

  int retry_delay;
  JobControl::RetryMethod retry_method =
      job->OnURLLoadComplete(payload, "application/x-protobuffer", net_error,
                             response_code, false, &retry_delay);
  if (retry_method != JobControl::NO_RETRY)
    RequeueJobForTesting(job);

  return retry_method;
}

FakeJobConfiguration::FakeJobConfiguration(
    DeviceManagementService* service,
    JobType type,
    const std::string& client_id,
    bool critical,
    std::unique_ptr<DMAuth> auth_data,
    base::Optional<std::string> oauth_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FakeCallback callback,
    RetryCallback retry_callback)
    : DMServerJobConfiguration(service,
                               type,
                               client_id,
                               critical,
                               std::move(auth_data),
                               oauth_token,
                               url_loader_factory,
                               base::DoNothing()),
      callback_(std::move(callback)),
      retry_callback_(retry_callback) {
  DCHECK(!callback_.is_null());
  DCHECK(!retry_callback_.is_null());
}

FakeJobConfiguration::~FakeJobConfiguration() {}

void FakeJobConfiguration::SetRequestPayload(
    const std::string& request_payload) {
  request()->ParseFromString(request_payload);
}

void FakeJobConfiguration::OnBeforeRetry() {
  retry_callback_.Run();
}

void FakeJobConfiguration::OnURLLoadComplete(DeviceManagementService::Job* job,
                                             int net_error,
                                             int response_code,
                                             const std::string& response_body) {
  DeviceManagementStatus code =
      MapNetErrorAndResponseCodeToDMStatus(net_error, response_code);
  std::move(callback_).Run(job, code, net_error, response_body);
}

}  // namespace policy
