// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_PROFILE_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_PROFILE_CLOUD_POLICY_STORE_H_

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/policy/policy_export.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace enterprise_management {
class PolicyFetchResponse;
class PolicySigningKey;
}  // namespace enterprise_management

namespace policy {

// Implements a cloud policy store that stores policy for profile level user
// cloud policy. This is used on (non-chromeos) platforms that do no have a
// secure storage implementation.
class POLICY_EXPORT ProfileCloudPolicyStore : public DesktopCloudPolicyStore {
 public:
  ProfileCloudPolicyStore(
      const base::FilePath& policy_path,
      const base::FilePath& key_path,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      bool is_dasherless = false);

  ProfileCloudPolicyStore(const ProfileCloudPolicyStore&) = delete;
  ProfileCloudPolicyStore& operator=(const ProfileCloudPolicyStore&) = delete;
  ~ProfileCloudPolicyStore() override;

  // Creates a ProfileCloudPolicyStore instance.
  static std::unique_ptr<ProfileCloudPolicyStore> Create(
      const base::FilePath& profile_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      bool is_dasherless = false);

  // override UserCloudPolicyStoreBase
  std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option) override;

 private:
  // override DesktopCloudPolicyStore
  void Validate(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      std::unique_ptr<enterprise_management::PolicySigningKey> key,
      UserCloudPolicyValidator::CompletionCallback callback) override;

  bool is_dasherless_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_PROFILE_CLOUD_POLICY_STORE_H_
