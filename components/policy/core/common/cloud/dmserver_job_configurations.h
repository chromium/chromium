// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DMSERVER_JOB_CONFIGURATIONS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DMSERVER_JOB_CONFIGURATIONS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudPolicyClient;

// Struct containing the result data for a given job.
struct DMServerJobResult {
  // Unowned pointer the return value of `DeviceManagementService::CreateJob`.
  raw_ptr<const DeviceManagementService::Job, DanglingUntriaged> job = nullptr;

  // net::Error value cast to int.
  int net_error = 0;

  // Status code combining
  //   - `net_error`
  //   - HTTP response code received from DMServer
  //   - potential error from parsing `response`
  DeviceManagementStatus dm_status =
      DeviceManagementStatus::DM_STATUS_REQUEST_INVALID;

  // HTTP response codes of the DMServer.
  int response_code = 0;

  // The parsed response proto received from DMServer. This could be empty
  // in case of errors.
  enterprise_management::DeviceManagementResponse response;
};

// A configuration for sending enterprise_management::DeviceManagementRequest to
// the DM server.
class POLICY_EXPORT DMServerJobConfiguration : public JobConfigurationBase {
 public:
  typedef base::OnceCallback<void(DMServerJobResult)> Callback;

  struct POLICY_EXPORT CreateParams {
   public:
    static CreateParams WithClient(JobType type, CloudPolicyClient* client);
    static CreateParams WithoutClient(
        JobType type,
        DeviceManagementService* service,
        const std::string& client_id,
        scoped_refptr<network::SharedURLLoaderFactory> factory);

    // Used by old `DMServerJobConfiguration` constructor. Please avoid adding
    // new parameter to it or using it outside DMServerJobConfiguration
    // constructor.
    static CreateParams WithParams(
        DeviceManagementService* service,
        JobType type,
        const std::string& client_id,
        bool critical,
        DMAuth auth_data,
        std::optional<std::string> oauth_token,
        scoped_refptr<network::SharedURLLoaderFactory> factory,
        Callback callback);

    CreateParams();

    CreateParams(const CreateParams&) = delete;
    CreateParams& operator=(const CreateParams&) = delete;

    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    ~CreateParams();

    raw_ptr<DeviceManagementService> service = nullptr;
    JobType type = JobType::TYPE_INVALID;
    std::string client_id;
    bool critical = false;
    DMAuth auth_data = DMAuth::NoAuth();
    std::optional<std::string> profile_id = std::nullopt;
    std::optional<std::string> oauth_token = std::nullopt;
    scoped_refptr<network::SharedURLLoaderFactory> factory;
    DMServerJobConfiguration::Callback callback;
  };

  explicit DMServerJobConfiguration(CreateParams params);

  // Deprecated. Please use the `CreateParams` instead.
  DMServerJobConfiguration(
      DeviceManagementService* service,
      JobType type,
      const std::string& client_id,
      bool critical,
      DMAuth auth_data,
      std::optional<std::string>&& oauth_token,
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      Callback callback);

  // Deprecated. Please use the `CreateParams` instead.
  DMServerJobConfiguration(JobType type,
                           CloudPolicyClient* client,
                           bool critical,
                           DMAuth auth_data,
                           std::optional<std::string>&& oauth_token,
                           Callback callback);

  DMServerJobConfiguration(const DMServerJobConfiguration&) = delete;
  DMServerJobConfiguration& operator=(const DMServerJobConfiguration&) = delete;

  ~DMServerJobConfiguration() override;

  enterprise_management::DeviceManagementRequest* request() {
    return &request_;
  }

 protected:
  DeviceManagementStatus MapNetErrorAndResponseToDMStatus(
      int net_error,
      int response_code,
      const std::string& response_body);

 private:
  // JobConfiguration interface.
  std::string GetPayload() override;
  std::string GetUmaName() override;
  void OnBeforeRetry(int response_code,
                     const std::string& response_body) override {}
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;

  // JobConfigurationBase overrides.
  GURL GetURL(int last_error) const override;

  std::string server_url_;
  enterprise_management::DeviceManagementRequest request_;
  Callback callback_;
};

// A configuration for sending registration requests to the DM server.  These
// are requests of type TYPE_REGISTRATION, TYPE_TOKEN_ENROLLMENT, and
// TYPE_CERT_BASED_REGISTRATION.  This class extends DMServerJobConfiguration
// by adjusting the enterprise_management::DeviceManagementRequest message in
// registration retry requests.
class POLICY_EXPORT RegistrationJobConfiguration
    : public DMServerJobConfiguration {
 public:
  explicit RegistrationJobConfiguration(CreateParams params);
  RegistrationJobConfiguration(const RegistrationJobConfiguration&) = delete;
  RegistrationJobConfiguration& operator=(const RegistrationJobConfiguration&) =
      delete;

  void SetTimeoutDuration(base::TimeDelta timeout);

 private:
  // JobConfiguration interface.
  void OnBeforeRetry(int response_code,
                     const std::string& response_body) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DMSERVER_JOB_CONFIGURATIONS_H_
