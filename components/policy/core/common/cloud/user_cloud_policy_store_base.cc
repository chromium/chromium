// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

#include <utility>

#include "base/check_op.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
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

std::unique_ptr<CloudPolicyValidatorBase>
UserCloudPolicyStoreBase::CreateValidator(
    std::unique_ptr<em::PolicyFetchResponse> policy_fetch_response,
    CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option) {
  std::unique_ptr<CloudPolicyValidatorBase> validator;
  if (IsChromePolicyType(policy_type())) {
    validator = std::make_unique<CloudPolicyValidator<em::CloudPolicySettings>>(
        std::move(policy_fetch_response), background_task_runner_);
  } else if (IsExtensionInstallPolicyType(policy_type())) {
    validator =
        std::make_unique<CloudPolicyValidator<em::ExtensionInstallPolicies>>(
            std::move(policy_fetch_response), background_task_runner_);
  } else {
    NOTREACHED() << "Unsupported policy type: " << policy_type();
  }

  // Configure the validator.
  validator->ValidatePolicyType(policy_type());
  validator->ValidateAgainstCurrentPolicy(
      policy(), timestamp_option, CloudPolicyValidatorBase::DM_TOKEN_REQUIRED,
      CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->ValidatePayload();
  return validator;
}

void UserCloudPolicyStoreBase::InstallPolicy(
    std::unique_ptr<enterprise_management::PolicyData> policy_data,
    CloudPolicyValidatorBase* validator,
    const std::string& policy_signature_public_key) {
  // Decode the payload.
  policy_map_.Clear();
  PolicyPerProfileFilter filter = PolicyPerProfileFilter::kAny;
  CHECK_EQ(validator->policy_type(), policy_type());
  if (IsExtensionInstallPolicyType(policy_type())) {
    DecodeProtoFields(
        *static_cast<ExtensionInstallCloudPolicyValidator*>(validator)
             ->payload(),
        external_data_manager(), POLICY_SOURCE_CLOUD, policy_scope_,
        &policy_map_, filter);
  } else {
    DecodeProtoFields(
        *static_cast<UserCloudPolicyValidator*>(validator)->payload(),
        external_data_manager(), POLICY_SOURCE_CLOUD, policy_scope_,
        &policy_map_, filter);
  }

  if (policy_data->user_affiliation_ids_size() > 0) {
    policy_map_.SetUserAffiliationIds(
        {policy_data->user_affiliation_ids().begin(),
         policy_data->user_affiliation_ids().end()});
  }
  if (policy_data->device_affiliation_ids_size() > 0) {
    policy_map_.SetDeviceAffiliationIds(
        {policy_data->device_affiliation_ids().begin(),
         policy_data->device_affiliation_ids().end()});
  }
  SetPolicy(std::move(policy_data));
  policy_signature_public_key_ = policy_signature_public_key;
}

}  // namespace policy
