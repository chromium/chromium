// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class MockDeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  MockDeviceManagementServiceConfiguration();
  explicit MockDeviceManagementServiceConfiguration(
      const std::string& server_url);
  ~MockDeviceManagementServiceConfiguration() override;

  std::string GetDMServerUrl() override;
  std::string GetAgentParameter() override;
  std::string GetPlatformParameter() override;
  std::string GetReportingServerUrl() override;

 private:
  const std::string server_url_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementServiceConfiguration);
};

class MockDeviceManagementService : public DeviceManagementService {
 public:
  using StartJobFunction = void(JobControl* job);

  MockDeviceManagementService();
  ~MockDeviceManagementService() override;

  using DeviceManagementService::GetWeakPtr;
  using DeviceManagementService::RequeueJobForTesting;
  using DeviceManagementService::StartQueuedJobs;

  MOCK_METHOD1(StartJob, StartJobFunction);

  // Can be used as an action when mocking the StartJob method. Will respond
  // with the given data to the network request during the next idle run loop.
  // This call behaves the same as calling StartJobAsync() with the first
  // arguments set to net::OK and DeviceManagement::kSuccess.
  testing::Action<StartJobFunction> StartJobOKAsync(
      const enterprise_management::DeviceManagementResponse& response);

  // Can be used as an action when mocking the StartJob method.
  // Will respond with the given data to the network request during the next
  // idle run loop.
  testing::Action<StartJobFunction> StartJobAsync(
      int net_error,
      int response_code,
      const enterprise_management::DeviceManagementResponse& response =
          enterprise_management::DeviceManagementResponse());

  // Can be used as an action when mocking the StartJob method.
  // Will respond with the given data to the network request during the next
  // idle run loop.
  testing::Action<StartJobFunction> StartJobAsync(int net_error,
                                                  int response_code,
                                                  const std::string& payload);

  // Can be used as an action when mocking the StartJob method.
  // Will not respond to the network request automatically.  The caller is
  // responsible for responding when needed with a call to DoURLCompletion().
  // Note that MockDeviceManagementService owns the job_control object and
  // callers should not delete it.  The object will be deleted once
  // DoURLCompletion() is called or MockDeviceManagementService is destroyed.
  testing::Action<StartJobFunction> StartJobFullControl(
      JobControl** job_control);

  // Can be used as an action when mocking the StartJob method.
  // Makes a copy of the job type from the JobConfiguration of the Job passed
  // to StartJob.
  testing::Action<StartJobFunction> CaptureJobType(
      DeviceManagementService::JobConfiguration::JobType* job_type);

  // Can be used as an action when mocking the StartJob method.
  // Makes a copy of the query parameters from the JobConfiguration of the Job
  //  passed to StartJob.
  testing::Action<StartJobFunction> CaptureQueryParams(
      DeviceManagementService::JobConfiguration::ParameterMap* params);

  // Can be used as an action when mocking the StartJob method.
  // Makes a copy of the device management request from the JobConfiguration
  // of the Job passed to StartJob.
  testing::Action<StartJobFunction> CaptureRequest(
      enterprise_management::DeviceManagementRequest* request);

  // Can be used as an action when mocking the StartJob method.
  // Makes a copy of the payload of the JobConfiguration
  // of the Job passed to StartJob.
  testing::Action<StartJobFunction> CapturePayload(std::string* payload);

  // Call after using StartJobFullControl() to respond to the network request.
  // If the job completed successfully, |*job| will be nulled to prevent callers
  // from using the pointer beyond its lifetime.  If the job was retried, then
  // |*job| is not nulled and the caller can continue to use the pointer.
  void DoURLCompletion(
      JobControl** job,
      int net_error,
      int response_code,
      const enterprise_management::DeviceManagementResponse& response);

  // Call after using StartJobFullControl() to respond to the network request.
  // If the job completed successfully, |*job| will be nulled to prevent callers
  // from using the pointer beyond its lifetime.  If the job was retried, then
  // |*job| is not nulled and the caller can continue to use the pointer.
  void DoURLCompletion(JobControl** job,
                       int net_error,
                       int response_code,
                       const std::string& payload);

  // Call after using StartJobFullControl() to respond to the network request.
  // Use this overload only when using base::BindXXX() to complete a network
  // request.
  void DoURLCompletionForBinding(
      JobControl* job,
      int net_error,
      int response_code,
      const enterprise_management::DeviceManagementResponse& response);

  // Call after using StartJobFullControl() to respond to the network request.
  // If the job completed successfully, |*job| will be nulled to prevent callers
  // from using the pointer beyond its lifetime.  If the job was retried, then
  // |*job| is not nulled and the caller can continue to use the pointer.
  void DoURLCompletionWithPayload(JobControl** job,
                                  int net_error,
                                  int response_code,
                                  const std::string& payload);

 private:
  JobControl::RetryMethod DoURLCompletionInternal(JobControl* job,
                                                  int net_error,
                                                  int response_code,
                                                  const std::string& payload);

  DISALLOW_COPY_AND_ASSIGN(MockDeviceManagementService);
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

  typedef base::RepeatingCallback<void()> RetryCallback;

  FakeJobConfiguration(
      DeviceManagementService* service,
      JobType type,
      const std::string& client_id,
      bool critical,
      std::unique_ptr<DMAuth> auth_data,
      base::Optional<std::string> oauth_token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FakeCallback callback,
      RetryCallback retry_callback);
  ~FakeJobConfiguration() override;

  void SetRequestPayload(const std::string& request_payload);

 private:
  void OnBeforeRetry() override;
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;

  FakeCallback callback_;
  RetryCallback retry_callback_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_DEVICE_MANAGEMENT_SERVICE_H_
