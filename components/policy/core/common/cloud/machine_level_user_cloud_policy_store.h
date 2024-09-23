// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_STORE_H_

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

namespace policy {

// Implements a cloud policy store that stores policy for machine level user
// cloud policy. This is used on (non-chromeos) platforms that do no have a
// secure storage implementation.
class POLICY_EXPORT MachineLevelUserCloudPolicyStore
    : public DesktopCloudPolicyStore {
 public:
  MachineLevelUserCloudPolicyStore(
      const DMToken& machine_dm_token,
      const std::string& machine_client_id,
      const base::FilePath& external_policy_path,
      const base::FilePath& external_policy_info_path,
      const base::FilePath& policy_path,
      const base::FilePath& key_path,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  MachineLevelUserCloudPolicyStore(const MachineLevelUserCloudPolicyStore&) =
      delete;
  MachineLevelUserCloudPolicyStore& operator=(
      const MachineLevelUserCloudPolicyStore&) = delete;
  ~MachineLevelUserCloudPolicyStore() override;

  // Creates a MachineLevelUserCloudPolicyStore instance. |external_policy_path|
  // must be a secure location because no signature validations are made on it.
  static std::unique_ptr<MachineLevelUserCloudPolicyStore> Create(
      const DMToken& machine_dm_token,
      const std::string& machine_client_id,
      const base::FilePath& external_policy_dir,
      const base::FilePath& policy_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // override DesktopCloudPolicyStore
  void LoadImmediately() override;
  void Load() override;

  // override UserCloudPolicyStoreBase
  std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option) override;

  // Setup global |dm_token| and |client_id| in store for the validation purpose
  // before policy refresh.
  void SetupRegistration(const DMToken& machine_dm_token,
                         const std::string& machine_client_id);

  // No DM token can be fetched from server or read from disk. Finish
  // initialization with empty policy data.
  void InitWithoutToken();

 private:
  // Function used as a PolicyLoadFilter to use external policies if they are
  // newer than the ones previously written by the browser.
  static PolicyLoadResult MaybeUseExternalCachedPolicies(
      const base::FilePath& policy_cache_path,
      const base::FilePath& policy_info_path,
      PolicyLoadResult default_cached_policy_load_result);

  static PolicyLoadResult LoadExternalCachedPolicies(
      const base::FilePath& policy_cache_path,
      const base::FilePath& policy_info_path);

  // override DesktopCloudPolicyStore
  void Validate(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      std::unique_ptr<enterprise_management::PolicySigningKey> key,
      UserCloudPolicyValidator::CompletionCallback callback) override;

  DMToken machine_dm_token_;
  std::string machine_client_id_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_STORE_H_
