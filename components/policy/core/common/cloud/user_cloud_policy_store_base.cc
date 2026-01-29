// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

UserCloudPolicyStoreBase::UserCloudPolicyStoreBase(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    PolicyScope policy_scope,
    const std::string& policy_type)
    : CloudPolicyStore(policy_type),
      background_task_runner_(background_task_runner),
      policy_scope_(policy_scope) {}

UserCloudPolicyStoreBase::~UserCloudPolicyStoreBase() = default;

std::unique_ptr<UserCloudPolicyValidator>
UserCloudPolicyStoreBase::CreateValidator(
    std::unique_ptr<em::PolicyFetchResponse> policy_fetch_response,
    CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option) {
  CHECK_EQ(policy_type(), dm_protocol::GetChromeUserPolicyType());
  return CreateValidatorImpl<em::CloudPolicySettings>(
      std::move(policy_fetch_response), timestamp_option);
}

std::unique_ptr<ExtensionInstallCloudPolicyValidator>
UserCloudPolicyStoreBase::CreateExtensionInstallValidator(
    std::unique_ptr<em::PolicyFetchResponse> policy_fetch_response,
    CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option) {
  CHECK_EQ(policy_type(),
           dm_protocol::kChromeExtensionInstallUserCloudPolicyType);
  return CreateValidatorImpl<em::ExtensionInstallPolicies>(
      std::move(policy_fetch_response),
      timestamp_option);
}

template <typename PayloadProto>
std::unique_ptr<CloudPolicyValidator<PayloadProto>>
UserCloudPolicyStoreBase::CreateValidatorImpl(
    std::unique_ptr<em::PolicyFetchResponse> policy_fetch_response,
    CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option) {
  static_assert(std::is_same<PayloadProto, em::CloudPolicySettings>() ||
                std::is_same<PayloadProto, em::ExtensionInstallPolicies>());

  // Configure the validator.
  auto validator = std::make_unique<CloudPolicyValidator<PayloadProto>>(
      std::move(policy_fetch_response), background_task_runner_);
  validator->ValidatePolicyType(policy_type());
  validator->ValidateAgainstCurrentPolicy(
      policy(), timestamp_option, CloudPolicyValidatorBase::DM_TOKEN_REQUIRED,
      CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->ValidatePayload();
  return validator;
}

}  // namespace policy
