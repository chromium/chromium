// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/profile_cloud_management_delegate.h"

#include "base/check.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_attestation {

ProfileCloudManagementDelegate::ProfileCloudManagementDelegate(
    std::unique_ptr<enterprise_core::DependencyFactory> dependency_factory,
    enterprise::ProfileIdService* profile_id_service,
    std::unique_ptr<DMServerClient> dmserver_client)
    : dependency_factory_(std::move(dependency_factory)),
      profile_id_service_(profile_id_service),
      dmserver_client_(std::move(dmserver_client)) {
  CHECK(dependency_factory_);
  CHECK(profile_id_service_);
  CHECK(dmserver_client_);
}

ProfileCloudManagementDelegate::~ProfileCloudManagementDelegate() = default;

std::optional<std::string> ProfileCloudManagementDelegate::GetDMToken() const {
  const auto* policy_data = GetPolicyData();
  if (!policy_data || !policy_data->has_request_token()) {
    return std::nullopt;
  }
  return policy_data->request_token();
}

void ProfileCloudManagementDelegate::UploadBrowserPublicKey(
    const enterprise_management::DeviceManagementRequest& upload_request,
    policy::DMServerJobConfiguration::Callback callback) {
  dmserver_client_->UploadBrowserPublicKey(
      GetClientID().value_or(""), GetDMToken().value_or(""),
      profile_id_service_->GetProfileId(), std::move(upload_request),
      std::move(callback));
}

const enterprise_management::PolicyData*
ProfileCloudManagementDelegate::GetPolicyData() const {
  policy::CloudPolicyManager* policy_manager =
      dependency_factory_->GetUserCloudPolicyManager();
  if (policy_manager && policy_manager->core() &&
      policy_manager->core()->store() &&
      policy_manager->core()->store()->has_policy()) {
    return policy_manager->core()->store()->policy();
  }
  return nullptr;
}

std::optional<std::string> ProfileCloudManagementDelegate::GetClientID() const {
  const auto* policy_data = GetPolicyData();
  if (!policy_data || !policy_data->has_device_id()) {
    return std::nullopt;
  }
  return policy_data->device_id();
}

}  // namespace enterprise_attestation
