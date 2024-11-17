// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {
namespace {

const base::FilePath::CharType kPolicyCache[] =
    FILE_PATH_LITERAL("Machine Level User Cloud Policy");
const base::FilePath::CharType kKeyCache[] =
    FILE_PATH_LITERAL("Machine Level User Cloud Policy Signing Key");
constexpr base::FilePath::StringPieceType kExternalPolicyCache =
    FILE_PATH_LITERAL("PolicyFetchResponse");
constexpr base::FilePath::StringPieceType kExternalPolicyInfo =
    FILE_PATH_LITERAL("CachedPolicyInfo");
}  // namespace

MachineLevelUserCloudPolicyStore::MachineLevelUserCloudPolicyStore(
    const DMToken& machine_dm_token,
    const std::string& machine_client_id,
    const base::FilePath& external_policy_path,
    const base::FilePath& external_policy_info_path,
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : DesktopCloudPolicyStore(
          policy_path,
          key_path,
          base::BindRepeating(
              &MachineLevelUserCloudPolicyStore::MaybeUseExternalCachedPolicies,
              external_policy_path,
              external_policy_info_path),
          background_task_runner,
          PolicyScope::POLICY_SCOPE_MACHINE),
      machine_dm_token_(machine_dm_token),
      machine_client_id_(machine_client_id) {}

MachineLevelUserCloudPolicyStore::~MachineLevelUserCloudPolicyStore() = default;

// static
std::unique_ptr<MachineLevelUserCloudPolicyStore>
MachineLevelUserCloudPolicyStore::Create(
    const DMToken& machine_dm_token,
    const std::string& machine_client_id,
    const base::FilePath& external_policy_dir,
    const base::FilePath& policy_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  base::FilePath policy_cache_file = policy_dir.Append(kPolicyCache);
  base::FilePath key_cache_file = policy_dir.Append(kKeyCache);
  base::FilePath external_policy_path;
  base::FilePath external_policy_info_path;
  if (!external_policy_dir.empty()) {
    external_policy_path =
        external_policy_dir
            .AppendASCII(policy::dm_protocol::
                             kChromeMachineLevelUserCloudPolicyTypeBase64)
            .Append(kExternalPolicyCache);
    external_policy_info_path = external_policy_dir.Append(kExternalPolicyInfo);
  }
  return std::make_unique<MachineLevelUserCloudPolicyStore>(
      machine_dm_token, machine_client_id, external_policy_path,
      external_policy_info_path, policy_cache_file, key_cache_file,
      background_task_runner);
}

bool IsResultKeyEqual(const PolicyLoadResult& default_result,
                      const PolicyLoadResult& external_result) {
  return default_result.key.signing_key() ==
             external_result.key.signing_key() &&
         default_result.key.signing_key_signature() ==
             external_result.key.signing_key_signature() &&
         default_result.key.verification_key() ==
             external_result.key.verification_key();
}

void MachineLevelUserCloudPolicyStore::LoadImmediately() {
  // There is no global dm token, stop loading the policy cache in order to
  // avoid an unnecessary disk access. Policies will be fetched after enrollment
  // succeeded.
  if (!machine_dm_token_.is_valid()) {
    VLOG_POLICY(1, POLICY_FETCHING)
        << "LoadImmediately ignored, no DM token present.";
#if BUILDFLAG(IS_ANDROID)
    // On Android, some dependencies (e.g. FirstRunActivity) are blocked until
    // the PolicyService is initialized, which waits on all policy providers to
    // indicate that policies are available.
    //
    // When cloud enrollment is not mandatory, machine-level cloud policies are
    // loaded asynchronously and will be applied once they are fetched from the
    // server. To avoid blocking those dependencies on Android, notify that the
    // PolicyService initialization doesn't need to wait on cloud policies by
    // sending out an empty policy set.
    //
    // The call to |OnPolicyLoaded| is exactly the same that would happen if
    // this disk access optimization was not implemented.
    PolicyLoadResult result;
    result.status = policy::LOAD_RESULT_NO_POLICY_FILE;
    OnPolicyLoaded(result);
#endif  // BUILDFLAG(IS_ANDROID)
    return;
  }
  VLOG_POLICY(1, POLICY_FETCHING) << "Load policy cache Immediately.";
  DesktopCloudPolicyStore::LoadImmediately();
}

void MachineLevelUserCloudPolicyStore::Load() {
  // There is no global dm token, stop loading the policy cache. The policy will
  // be fetched in the end of enrollment process.
  if (!machine_dm_token_.is_valid()) {
    VLOG_POLICY(1, POLICY_FETCHING) << "Load ignored, no DM token present.";
    return;
  }
  VLOG_POLICY(1, POLICY_FETCHING) << "Load policy cache.";
  DesktopCloudPolicyStore::Load();
}

// static
PolicyLoadResult
MachineLevelUserCloudPolicyStore::MaybeUseExternalCachedPolicies(
    const base::FilePath& policy_cache_path,
    const base::FilePath& policy_info_path,
    PolicyLoadResult default_cached_policy_load_result) {
  PolicyLoadResult external_policy_cache_load_result =
      LoadExternalCachedPolicies(policy_cache_path, policy_info_path);
  if (external_policy_cache_load_result.status != policy::LOAD_RESULT_SUCCESS)
    return default_cached_policy_load_result;

  // If default key is missing or not matches the external one, enable key
  // rotation mode to re-fetch public key again.
  if (!IsResultKeyEqual(default_cached_policy_load_result,
                        external_policy_cache_load_result)) {
    external_policy_cache_load_result.doing_key_rotation = true;
  }

  if (default_cached_policy_load_result.status != policy::LOAD_RESULT_SUCCESS)
    return external_policy_cache_load_result;

  enterprise_management::PolicyData default_data;
  enterprise_management::PolicyData external_data;
  if (default_data.ParseFromString(
          default_cached_policy_load_result.policy.policy_data()) &&
      external_data.ParseFromString(
          external_policy_cache_load_result.policy.policy_data()) &&
      external_data.timestamp() > default_data.timestamp()) {
    return external_policy_cache_load_result;
  }
  return default_cached_policy_load_result;
}

// static
PolicyLoadResult MachineLevelUserCloudPolicyStore::LoadExternalCachedPolicies(
    const base::FilePath& policy_cache_path,
    const base::FilePath& policy_info_path) {
  // Loads cached cloud policies by an external provider.
  PolicyLoadResult policy_cache_load_result =
      DesktopCloudPolicyStore::LoadPolicyFromDisk(policy_cache_path,
                                                  base::FilePath());
  PolicyLoadResult policy_info_load_result =
      DesktopCloudPolicyStore::LoadPolicyFromDisk(policy_info_path,
                                                  base::FilePath());

  // External policy source doesn't provide full components policies data hence
  // browser will rely on the local cache which requires public key to verify
  // them.
  // Load the key and signature of the key from Extennal policy info file and
  // use it to verify all Chrome and components policies. The browser will
  // redownload the policeis in case of validation failure.
  VLOG_POLICY(1, POLICY_PROCESSING)
      << (policy_info_load_result.policy.has_new_public_key()
              ? "External policy has public key."
              : "External policy doesn't have public key.");
  policy_cache_load_result.key.set_signing_key(
      policy_info_load_result.policy.new_public_key());
  policy_cache_load_result.key.set_signing_key_signature(
      policy_info_load_result.policy
          .new_public_key_verification_signature_deprecated());
  policy_cache_load_result.key.set_verification_key(GetPolicyVerificationKey());

  return policy_cache_load_result;
}

std::unique_ptr<UserCloudPolicyValidator>
MachineLevelUserCloudPolicyStore::CreateValidator(
    std::unique_ptr<enterprise_management::PolicyFetchResponse>
        policy_fetch_response,
    CloudPolicyValidatorBase::ValidateTimestampOption option) {
  auto validator = std::make_unique<UserCloudPolicyValidator>(
      std::move(policy_fetch_response), background_task_runner());
  validator->ValidatePolicyType(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  validator->ValidateDMToken(machine_dm_token_.value(),
                             CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  validator->ValidateDeviceId(machine_client_id_,
                              CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  if (has_policy()) {
    validator->ValidateTimestamp(
        base::Time::FromMillisecondsSinceUnixEpoch(policy()->timestamp()),
        option);
  }
  validator->ValidatePayload();
  return validator;
}

void MachineLevelUserCloudPolicyStore::SetupRegistration(
    const DMToken& machine_dm_token,
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
    UserCloudPolicyValidator::CompletionCallback callback) {
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);

  // Policies cached by the external provider do not require key and signature
  // validation since they are stored in a secure location.
  if (key)
    ValidateKeyAndSignature(validator.get(), key.get(), std::string());

  validator->RunValidation();
  std::move(callback).Run(validator.get());
}

}  // namespace policy
