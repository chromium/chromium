// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_device_management_service.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using testing::Action;

namespace em = enterprise_management;

namespace policy {
namespace {

const char kServerUrl[] = "https://example.com/management_service";
const char kUserAgent[] = "Chrome 1.2.3(456)";
const char kPlatform[] = "Test|Unit|1.2.3";

// Common mock request job functionality.
class MockRequestJobBase : public DeviceManagementRequestJob {
 public:
  MockRequestJobBase(JobType type,
                     MockDeviceManagementService* service)
      : DeviceManagementRequestJob(type, std::string(), std::string()),
        service_(service) {}
  ~MockRequestJobBase() override {}

 protected:
  void Run() override {
    service_->StartJob(ExtractParameter(dm_protocol::kParamRequest),
                       auth_data_->gaia_token(),
                       ExtractParameter(dm_protocol::kParamOAuthToken),
                       auth_data_->dm_token(), auth_data_->enrollment_token(),
                       ExtractParameter(dm_protocol::kParamDeviceID), request_);
  }

 private:
  // Searches for a query parameter and returns the associated value.
  const std::string& ExtractParameter(const std::string& name) const {
    for (auto entry(query_params_.begin()); entry != query_params_.end();
         ++entry) {
      if (name == entry->first)
        return entry->second;
    }

    return base::EmptyString();
  }

  MockDeviceManagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockRequestJobBase);
};

// Synchronous mock request job that immediately completes on calling Run().
class SyncRequestJob : public MockRequestJobBase {
 public:
  SyncRequestJob(JobType type,
                 MockDeviceManagementService* service,
                 DeviceManagementStatus status,
                 const em::DeviceManagementResponse& response)
      : MockRequestJobBase(type, service),
        status_(status),
        response_(response) {}
  ~SyncRequestJob() override {}

 protected:
  void Run() override {
    MockRequestJobBase::Run();
    callback_.Run(status_, net::OK, response_);
  }

 private:
  DeviceManagementStatus status_;
  em::DeviceManagementResponse response_;

  DISALLOW_COPY_AND_ASSIGN(SyncRequestJob);
};

// Asynchronous job that allows the test to delay job completion.
class AsyncRequestJob : public MockRequestJobBase,
                        public MockDeviceManagementJob {
 public:
  AsyncRequestJob(JobType type, MockDeviceManagementService* service)
      : MockRequestJobBase(type, service) {}
  ~AsyncRequestJob() override {}

 protected:
  void RetryJob() override {
    if (!retry_callback_.is_null())
      retry_callback_.Run(this);
    Run();
  }

  void SendResponse(DeviceManagementStatus status,
                    const em::DeviceManagementResponse& response) override {
    callback_.Run(status, net::OK, response);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncRequestJob);
};

}  // namespace

ACTION_P3(CreateSyncMockDeviceManagementJob, service, status, response) {
  return new SyncRequestJob(arg0, service, status, response);
}

ACTION_P2(CreateAsyncMockDeviceManagementJob, service, mock_job) {
  AsyncRequestJob* job = new AsyncRequestJob(arg0, service);
  *mock_job = job;
  return job;
}

MockDeviceManagementJob::~MockDeviceManagementJob() {}

MockDeviceManagementServiceConfiguration::
    MockDeviceManagementServiceConfiguration()
    : server_url_(kServerUrl) {}

MockDeviceManagementServiceConfiguration::
    MockDeviceManagementServiceConfiguration(const std::string& server_url)
    : server_url_(server_url) {}

MockDeviceManagementServiceConfiguration::
    ~MockDeviceManagementServiceConfiguration() {}

std::string MockDeviceManagementServiceConfiguration::GetServerUrl() {
  return server_url_;
}

std::string MockDeviceManagementServiceConfiguration::GetAgentParameter() {
  return kUserAgent;
}

std::string MockDeviceManagementServiceConfiguration::GetPlatformParameter() {
  return kPlatform;
}

MockDeviceManagementService::MockDeviceManagementService()
    : DeviceManagementService(std::unique_ptr<Configuration>(
          new MockDeviceManagementServiceConfiguration)) {}

MockDeviceManagementService::~MockDeviceManagementService() {}

Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::SucceedJob(
        const em::DeviceManagementResponse& response) {
  return CreateSyncMockDeviceManagementJob(this, DM_STATUS_SUCCESS, response);
}

Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::FailJob(DeviceManagementStatus status) {
  const em::DeviceManagementResponse dummy_response;
  return CreateSyncMockDeviceManagementJob(this, status, dummy_response);
}

Action<MockDeviceManagementService::CreateJobFunction>
    MockDeviceManagementService::CreateAsyncJob(MockDeviceManagementJob** job) {
  return CreateAsyncMockDeviceManagementJob(this, job);
}

}  // namespace policy
