// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DMSERVER_JOB_CONFIGURATIONS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DMSERVER_JOB_CONFIGURATIONS_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/cloud_policy.pb.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudPolicyClient;
class DMAuth;

// A configuration for sending enterprise_management::DeviceManagementRequest to
// the DM server.
class POLICY_EXPORT DMServerJobConfiguration : public JobConfigurationBase {
 public:
  typedef base::OnceCallback<void(
      DeviceManagementService::Job* job,
      DeviceManagementStatus code,
      int net_error,
      const enterprise_management::DeviceManagementResponse&)>
      Callback;

  DMServerJobConfiguration(
      DeviceManagementService* service,
      JobType type,
      const std::string& cliend_id,
      bool critical,
      std::unique_ptr<DMAuth> auth_data,
      base::Optional<std::string> oauth_token,
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      Callback callback);

  // This constructor is a convenience if the caller already hsa a pointer to
  // a CloudPolicyClient.
  DMServerJobConfiguration(JobType type,
                           CloudPolicyClient* client,
                           bool critical,
                           std::unique_ptr<DMAuth> auth_data,
                           base::Optional<std::string> oauth_token,
                           Callback callback);

  ~DMServerJobConfiguration() override;

  enterprise_management::DeviceManagementRequest* request() {
    return &request_;
  }

 protected:
  DeviceManagementStatus MapNetErrorAndResponseCodeToDMStatus(
      int net_error,
      int response_code);

 private:
  // JobConfiguration interface.
  std::string GetPayload() override;
  std::string GetUmaName() override;
  void OnBeforeRetry() override {}
  void OnURLLoadComplete(DeviceManagementService::Job* job,
                         int net_error,
                         int response_code,
                         const std::string& response_body) override;

  // JobConfigurationBase overrides.
  GURL GetURL(int last_error) override;

  std::string server_url_;
  enterprise_management::DeviceManagementRequest request_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DMServerJobConfiguration);
};

// A configuration for sending registration requests to the DM server.  These
// are requests of type TYPE_REGISTRATION, TYPE_TOKEN_ENROLLMENT, and
// TYPE_CERT_BASED_REGISTRATION.  This class extends DMServerJobConfiguration
// by adjusting the enterprise_management::DeviceManagementRequest message in
// registration retry requests.
class POLICY_EXPORT RegistrationJobConfiguration
    : public DMServerJobConfiguration {
 public:
  RegistrationJobConfiguration(JobType type,
                               CloudPolicyClient* client,
                               std::unique_ptr<DMAuth> auth_data,
                               base::Optional<std::string> oauth_token,
                               Callback callback);

 private:
  // JobConfiguration interface.
  void OnBeforeRetry() override;

  DISALLOW_COPY_AND_ASSIGN(RegistrationJobConfiguration);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DMSERVER_JOB_CONFIGURATIONS_H_
