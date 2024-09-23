// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockDeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  MockDeviceManagementServiceConfiguration();
  explicit MockDeviceManagementServiceConfiguration(
      const std::string& server_url);
  MockDeviceManagementServiceConfiguration(
      const MockDeviceManagementServiceConfiguration&) = delete;
  MockDeviceManagementServiceConfiguration& operator=(
      const MockDeviceManagementServiceConfiguration&) = delete;
  ~MockDeviceManagementServiceConfiguration() override;

  std::string GetDMServerUrl() const override;
  std::string GetAgentParameter() const override;
  std::string GetPlatformParameter() const override;
  std::string GetRealtimeReportingServerUrl() const override;
  std::string GetEncryptedReportingServerUrl() const override;

 private:
  const std::string server_url_;
};

class MockJobCreationHandler {
 public:
  MockJobCreationHandler();
  MockJobCreationHandler(const MockJobCreationHandler&) = delete;
  MockJobCreationHandler& operator=(const MockJobCreationHandler&) = delete;
  ~MockJobCreationHandler();

  MOCK_METHOD(void,
              OnJobCreation,
              (const DeviceManagementService::JobForTesting&));
};

class FakeDeviceManagementService : public DeviceManagementService {
 public:
  using JobAction = testing::Action<void(const JobForTesting&)>;

  explicit FakeDeviceManagementService(
      MockJobCreationHandler* creation_handler);
  FakeDeviceManagementService(std::unique_ptr<Configuration> config,
                              MockJobCreationHandler* creation_handler);
  FakeDeviceManagementService(const FakeDeviceManagementService&) = delete;
  FakeDeviceManagementService& operator=(const FakeDeviceManagementService&) =
      delete;
  ~FakeDeviceManagementService() override;

  // Convenience actions to obtain the respective data from the job's
  // configuration.
  JobAction CaptureAuthData(DMAuth* auth_data);
  JobAction CaptureJobType(
      DeviceManagementService::JobConfiguration::JobType* job_type);
  JobAction CapturePayload(std::string* payload);
  JobAction CaptureQueryParams(
      std::map<std::string, std::string>* query_params);
  JobAction CaptureRequest(
      enterprise_management::DeviceManagementRequest* request);
  JobAction CaptureTimeout(base::TimeDelta* timeout);

  // Convenience actions to post a task which will call |SetResponseForTesting|
  // on the job.
  JobAction SendJobResponseAsync(
      int net_error,
      int response_code,
      const std::string& response = "",
      const std::string& mime_type = "application/x-protobuffer",
      bool was_fetched_via_proxy = false);

  JobAction SendJobResponseAsync(
      int net_error,
      int response_code,
      const enterprise_management::DeviceManagementResponse& response,
      const std::string& mime_type = "application/x-protobuffer",
      bool was_fetched_via_proxy = false);

  JobAction SendJobOKAsync(const std::string& response);

  JobAction SendJobOKAsync(
      const enterprise_management::DeviceManagementResponse& response);

  // Convenience wrappers around |job->SetResponseForTesting()|
  void SendJobResponseNow(
      DeviceManagementService::JobForTesting* job,
      int net_error,
      int response_code,
      const std::string& response = "",
      const std::string& mime_type = "application/x-protobuffer",
      bool was_fetched_via_proxy = false);

  void SendJobResponseNow(
      DeviceManagementService::JobForTesting* job,
      int net_error,
      int response_code,
      const enterprise_management::DeviceManagementResponse& response,
      const std::string& mime_type = "application/x-protobuffer",
      bool was_fetched_via_proxy = false);

  void SendJobOKNow(DeviceManagementService::JobForTesting* job,
                    const std::string& response);

  void SendJobOKNow(
      DeviceManagementService::JobForTesting* job,
      const enterprise_management::DeviceManagementResponse& response);

 private:
  std::unique_ptr<Job> CreateJob(
      std::unique_ptr<JobConfiguration> config) override;

  raw_ptr<MockJobCreationHandler> creation_handler_;
};

// A fake implementation of DMServerJobConfiguration that can be used in tests
// to set arbitrary request payloads for network requests.
class FakeJobConfiguration : public DMServerJobConfiguration {
 public:
  typedef base::OnceCallback<void(DeviceManagementService::Job* job,
                                  DeviceManagementStatus code,
                                  int net_error,
                                  const std::string&)>
      FakeCallback;

  typedef base::RepeatingCallback<void(int response_code,
                                       const std::string& response_body)>
      RetryCallback;

  FakeJobConfiguration(DMServerJobConfiguration::CreateParams params,
                       FakeCallback callback,
                       RetryCallback retry_callback,
                       RetryCallback should_retry_callback);
  ~FakeJobConfiguration() override;

  void SetRequestPayload(const std::string& request_payload);
  void SetShouldRetryResponse(DeviceManagementService::Job::RetryMethod method);
  void SetTimeoutDuration(base::TimeDelta timeout);

 private:
  DeviceManagementService::Job::RetryMethod ShouldRetry(
      int response_code,
      const std::string& response_body) override;
  void OnBeforeRetry(int response_code,
                     const std::string& response_body) override;
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;

  DeviceManagementService::Job::RetryMethod should_retry_response_;
  FakeCallback callback_;
  RetryCallback retry_callback_;
  RetryCallback should_retry_callback_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
