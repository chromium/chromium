// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/enterprise/core/dependency_factory.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_management {
class PolicyData;
}  // namespace enterprise_management

namespace enterprise_attestation {

class ProfileCloudManagementDelegate : public CloudManagementDelegate {
 public:
  ProfileCloudManagementDelegate(
      std::unique_ptr<enterprise_core::DependencyFactory> dependency_factory,
      enterprise::ProfileIdService* profile_id_service,
      std::unique_ptr<DMServerClient> dmserver_client);

  ~ProfileCloudManagementDelegate() override;

  // CloudManagementDelegate:
  std::optional<std::string> GetDMToken() const override;

  void UploadBrowserPublicKey(
      const enterprise_management::DeviceManagementRequest& upload_request,
      policy::DMServerJobConfiguration::Callback callback) override;

 private:
  const enterprise_management::PolicyData* GetPolicyData() const;
  std::optional<std::string> GetClientID() const;

  std::unique_ptr<enterprise_core::DependencyFactory> dependency_factory_;
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  std::unique_ptr<DMServerClient> dmserver_client_;
};

}  // namespace enterprise_attestation

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PROFILE_CLOUD_MANAGEMENT_DELEGATE_H_
