// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_BASE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_BASE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Base class that implements common cross-platform UserCloudPolicyStore
// functionality.
class POLICY_EXPORT UserCloudPolicyStoreBase : public CloudPolicyStore {
 public:
  UserCloudPolicyStoreBase(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      PolicyScope policy_scope,
      const std::string& policy_type);
  UserCloudPolicyStoreBase(const UserCloudPolicyStoreBase&) = delete;
  UserCloudPolicyStoreBase& operator=(const UserCloudPolicyStoreBase&) = delete;
  ~UserCloudPolicyStoreBase() override;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() const {
    return background_task_runner_;
  }

 protected:
  // Creates a validator configured to validate a user policy. The caller owns
  // the resulting object until StartValidation() is invoked.
  virtual std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option);

  virtual std::unique_ptr<ExtensionInstallCloudPolicyValidator>
  CreateExtensionInstallValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option);

  // Sets |policy_fetch_response|, |policy_data| and |payload| as the active
  // policy, and sets |policy_signature_public_key| as the active public key.
  template <typename PayloadProto>
  void InstallPolicy(
      std::unique_ptr<enterprise_management::PolicyData> policy_data,
      std::unique_ptr<PayloadProto> payload,
      const std::string& policy_signature_public_key) {
    static_assert(
        std::is_same<PayloadProto,
                     enterprise_management::CloudPolicySettings>() ||
        std::is_same<PayloadProto,
                     enterprise_management::ExtensionInstallPolicies>());

    // Decode the payload.
    policy_map_.Clear();
    PolicyPerProfileFilter filter = PolicyPerProfileFilter::kAny;
    DecodeProtoFields(*payload, external_data_manager(), POLICY_SOURCE_CLOUD,
                      policy_scope_, &policy_map_, filter);

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

 private:
  template <typename PayloadProto>
  std::unique_ptr<CloudPolicyValidator<PayloadProto>> CreateValidatorImpl(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option);

  // Task runner for background file operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  PolicyScope policy_scope_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_BASE_H_
