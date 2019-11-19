// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {
namespace {

const base::FilePath::CharType kPolicyCache[] =
    FILE_PATH_LITERAL("Machine Level User Cloud Policy");
const base::FilePath::CharType kKeyCache[] =
    FILE_PATH_LITERAL("Machine Level User Cloud Policy Signing Key");

}  // namespace

MachineLevelUserCloudPolicyStore::MachineLevelUserCloudPolicyStore(
    const std::string& machine_dm_token,
    const std::string& machine_client_id,
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    bool cloud_policy_has_priority,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : DesktopCloudPolicyStore(policy_path,
                              key_path,
                              background_task_runner,
                              PolicyScope::POLICY_SCOPE_MACHINE,
                              cloud_policy_has_priority
                                  ? PolicySource::POLICY_SOURCE_PRIORITY_CLOUD
                                  : PolicySource::POLICY_SOURCE_CLOUD),
      machine_dm_token_(machine_dm_token),
      machine_client_id_(machine_client_id) {}

MachineLevelUserCloudPolicyStore::~MachineLevelUserCloudPolicyStore() {}

// static
std::unique_ptr<MachineLevelUserCloudPolicyStore>
MachineLevelUserCloudPolicyStore::Create(
    const std::string& machine_dm_token,
    const std::string& machine_client_id,
    const base::FilePath& policy_dir,
    bool cloud_policy_has_priority,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  base::FilePath policy_cache_file = policy_dir.Append(kPolicyCache);
  base::FilePath key_cache_file = policy_dir.Append(kKeyCache);
  return std::make_unique<MachineLevelUserCloudPolicyStore>(
      machine_dm_token, machine_client_id, policy_cache_file, key_cache_file,
      cloud_policy_has_priority, background_task_runner);
}

void MachineLevelUserCloudPolicyStore::LoadImmediately() {
  // There is no global dm token, stop loading the policy cache. The policy will
  // be fetched in the end of enrollment process.
  if (machine_dm_token_.empty()) {
    VLOG(1) << "LoadImmediately ignored, no DM token present.";
    return;
  }
  VLOG(1) << "Load policy cache Immediately.";
  DesktopCloudPolicyStore::LoadImmediately();
}

void MachineLevelUserCloudPolicyStore::Load() {
  // There is no global dm token, stop loading the policy cache. The policy will
  // be fetched in the end of enrollment process.
  if (machine_dm_token_.empty()) {
    VLOG(1) << "Load ignored, no DM token present.";
    return;
  }
  VLOG(1) << "Load policy cache.";
  DesktopCloudPolicyStore::Load();
}

std::unique_ptr<UserCloudPolicyValidator>
MachineLevelUserCloudPolicyStore::CreateValidator(
    std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
    CloudPolicyValidatorBase::ValidateTimestampOption option) {
  auto validator = std::make_unique<UserCloudPolicyValidator>(
      std::move(policy), background_task_runner());
  validator->ValidatePolicyType(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  validator->ValidateDMToken(machine_dm_token_,
                             CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  validator->ValidateDeviceId(machine_client_id_,
                              CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  if (policy_) {
    validator->ValidateTimestamp(base::Time::FromJavaTime(policy_->timestamp()),
                                 option);
  }
  validator->ValidatePayload();
  return validator;
}

void MachineLevelUserCloudPolicyStore::SetupRegistration(
    const std::string& machine_dm_token,
    const std::string& machine_client_id) {
  machine_dm_token_ = machine_dm_token;
  machine_client_id_ = machine_client_id;
}

void MachineLevelUserCloudPolicyStore::InitWithoutToken() {
  NotifyStoreError();
}

void MachineLevelUserCloudPolicyStore::Validate(
    std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
    std::unique_ptr<enterprise_management::PolicySigningKey> key,
    bool validate_in_background,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);

  ValidateKeyAndSignature(validator.get(), key.get(), std::string());

  if (validate_in_background) {
    UserCloudPolicyValidator::StartValidation(std::move(validator), callback);
  } else {
    validator->RunValidation();
    callback.Run(validator.get());
  }
}

}  // namespace policy
