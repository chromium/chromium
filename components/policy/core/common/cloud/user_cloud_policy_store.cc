// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/proto/policy_signing_key.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
//
// Keep this in sync with EnterprisePolicyLoadStatus in histograms.xml.
enum PolicyLoadStatusForUma {
  // Policy blob was successfully loaded and parsed.
  LOAD_RESULT_SUCCESS,

  // No previously stored policy was found.
  LOAD_RESULT_NO_POLICY_FILE,

  // Could not load the previously stored policy due to either a parse or
  // file read error.
  LOAD_RESULT_LOAD_ERROR,

  // LOAD_RESULT_SIZE is the number of items in this enum and is used when
  // logging histograms to set the bucket size, so should always be the last
  // item.
  LOAD_RESULT_SIZE,
};

// Struct containing the result of a policy load - if |status| ==
// LOAD_RESULT_SUCCESS, |policy| is initialized from the policy file on disk.
struct PolicyLoadResult {
  PolicyLoadStatusForUma status;
  em::PolicyFetchResponse policy;
  em::PolicySigningKey key;
};

namespace {

// Subdirectory in the user's profile for storing user policies.
const base::FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Policy");
// File in the above directory for storing user policy data.
const base::FilePath::CharType kPolicyCacheFile[] =
    FILE_PATH_LITERAL("User Policy");

// File in the above directory for storing policy signing key data.
const base::FilePath::CharType kKeyCacheFile[] =
    FILE_PATH_LITERAL("Signing Key");

const char kMetricPolicyHasVerifiedCachedKey[] =
    "Enterprise.PolicyHasVerifiedCachedKey";

// Maximum policy and key size that will be loaded, in bytes.
const size_t kPolicySizeLimit = 1024 * 1024;
const size_t kKeySizeLimit = 16 * 1024;

// Loads policy from the backing file. Returns a PolicyLoadResult with the
// results of the fetch.
policy::PolicyLoadResult LoadPolicyFromDisk(const base::FilePath& policy_path,
                                            const base::FilePath& key_path) {
  policy::PolicyLoadResult result;
  // If the backing file does not exist, just return. We don't verify the key
  // path here, because the key is optional (the validation code will fail if
  // the key does not exist but the loaded policy is unsigned).
  if (!base::PathExists(policy_path)) {
    result.status = policy::LOAD_RESULT_NO_POLICY_FILE;
    return result;
  }
  std::string data;

  if (!base::ReadFileToStringWithMaxSize(policy_path, &data,
                                         kPolicySizeLimit) ||
      !result.policy.ParseFromString(data)) {
    LOG(WARNING) << "Failed to read or parse policy data from "
                 << policy_path.value();
    result.status = policy::LOAD_RESULT_LOAD_ERROR;
    return result;
  }

  if (!base::ReadFileToStringWithMaxSize(key_path, &data, kKeySizeLimit) ||
      !result.key.ParseFromString(data)) {
    // Log an error on missing key data, but do not trigger a load failure
    // for now since there are still old unsigned cached policy blobs in the
    // wild with no associated key (see kMetricPolicyHasVerifiedCachedKey UMA
    // stat below).
    LOG(ERROR) << "Failed to read or parse key data from " << key_path.value();
    result.key.clear_signing_key();
  }

  // Track the occurrence of valid cached keys - when this ratio gets high
  // enough, we can update the code to reject unsigned policy or unverified
  // keys.
  UMA_HISTOGRAM_BOOLEAN(kMetricPolicyHasVerifiedCachedKey,
                        result.key.has_signing_key());

  result.status = policy::LOAD_RESULT_SUCCESS;
  return result;
}

bool WriteStringToFile(const base::FilePath path, const std::string& data) {
  if (!base::CreateDirectory(path.DirName())) {
    DLOG(WARNING) << "Failed to create directory " << path.DirName().value();
    return false;
  }

  int size = data.size();
  if (base::WriteFile(path, data.c_str(), size) != size) {
    DLOG(WARNING) << "Failed to write " << path.value();
    return false;
  }

  return true;
}

// Stores policy to the backing file (must be called via a task on
// the background thread).
void StorePolicyToDiskOnBackgroundThread(
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    const em::PolicyFetchResponse& policy) {
  DVLOG(1) << "Storing policy to " << policy_path.value();
  std::string data;
  if (!policy.SerializeToString(&data)) {
    DLOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  if (!WriteStringToFile(policy_path, data))
    return;

  if (policy.has_new_public_key()) {
    // Write the new public key and its verification signature/key to a file.
    em::PolicySigningKey key_info;
    key_info.set_signing_key(policy.new_public_key());
    key_info.set_signing_key_signature(
        policy.new_public_key_verification_signature_deprecated());
    key_info.set_verification_key(GetPolicyVerificationKey());
    std::string key_data;
    if (!key_info.SerializeToString(&key_data)) {
      DLOG(WARNING) << "Failed to serialize policy signing key";
      return;
    }

    WriteStringToFile(key_path, key_data);
  }
}

}  // namespace

DesktopCloudPolicyStore::DesktopCloudPolicyStore(
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    PolicyScope policy_scope,
    PolicySource policy_source)
    : UserCloudPolicyStoreBase(background_task_runner,
                               policy_scope,
                               policy_source),
      policy_path_(policy_path),
      key_path_(key_path) {}

DesktopCloudPolicyStore::~DesktopCloudPolicyStore() {}

void DesktopCloudPolicyStore::LoadImmediately() {
  DVLOG(1) << "Initiating immediate policy load from disk";
  // Cancel any pending Load/Store/Validate operations.
  weak_factory_.InvalidateWeakPtrs();
  // Load the policy from disk...
  PolicyLoadResult result = LoadPolicyFromDisk(policy_path_, key_path_);
  // ...and install it, reporting success/failure to any observers.
  PolicyLoaded(false, result);
}

void DesktopCloudPolicyStore::Clear() {
  background_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                policy_path_, false));
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), key_path_, false));
  policy_.reset();
  policy_map_.Clear();
  policy_signature_public_key_.clear();
  persisted_policy_key_.clear();
  NotifyStoreLoaded();
}

void DesktopCloudPolicyStore::Load() {
  DVLOG(1) << "Initiating policy load from disk";
  // Cancel any pending Load/Store/Validate operations.
  weak_factory_.InvalidateWeakPtrs();

  // Start a new Load operation and have us get called back when it is
  // complete.
  base::PostTaskAndReplyWithResult(
      background_task_runner().get(), FROM_HERE,
      base::BindOnce(&LoadPolicyFromDisk, policy_path_, key_path_),
      base::BindOnce(&DesktopCloudPolicyStore::PolicyLoaded,
                     weak_factory_.GetWeakPtr(), true));
}

void DesktopCloudPolicyStore::PolicyLoaded(bool validate_in_background,
                                           PolicyLoadResult result) {
  // TODO(zmin): figure out what do with the metrics. https://crbug.com/814371
  UMA_HISTOGRAM_ENUMERATION("Enterprise.UserCloudPolicyStore.LoadStatus",
                            result.status, LOAD_RESULT_SIZE);
  switch (result.status) {
    case LOAD_RESULT_LOAD_ERROR:
      status_ = STATUS_LOAD_ERROR;
      NotifyStoreError();
      break;

    case LOAD_RESULT_NO_POLICY_FILE:
      DVLOG(1) << "No policy found on disk";
      NotifyStoreLoaded();
      break;

    case LOAD_RESULT_SUCCESS: {
      // Found policy on disk - need to validate it before it can be used.
      std::unique_ptr<em::PolicyFetchResponse> cloud_policy(
          new em::PolicyFetchResponse(result.policy));
      std::unique_ptr<em::PolicySigningKey> key(
          new em::PolicySigningKey(result.key));

      bool doing_key_rotation = false;
      if (!key->has_verification_key() ||
          key->verification_key() != GetPolicyVerificationKey()) {
        // The cached key didn't match our current key, so we're doing a key
        // rotation - make sure we request a new key from the server on our
        // next fetch.
        doing_key_rotation = true;
        DLOG(WARNING) << "Verification key rotation detected";
        // TODO(atwilson): Add code to update |verification_key| to point to
        // the correct key to validate the existing blob (can't do this until
        // we've done our first key rotation).
      }

      Validate(std::move(cloud_policy), std::move(key), validate_in_background,
               base::BindRepeating(
                   &DesktopCloudPolicyStore::InstallLoadedPolicyAfterValidation,
                   weak_factory_.GetWeakPtr(), doing_key_rotation,
                   result.key.has_signing_key() ? result.key.signing_key()
                                                : std::string()));
      break;
    }
    default:
      NOTREACHED();
  }
}

void DesktopCloudPolicyStore::ValidateKeyAndSignature(
    UserCloudPolicyValidator* validator,
    const em::PolicySigningKey* cached_key,
    const std::string& owning_domain) {
  // There are 4 cases:
  //
  // 1) Validation after loading from cache with no cached key.
  // Action: Just validate signature with an empty key - this will result in
  // a failed validation and the cached policy will be rejected.
  //
  // 2) Validation after loading from cache with a cached key
  // Action: Validate signature on policy blob but don't allow key rotation.
  //
  // 3) Validation after loading new policy from the server with no cached key
  // Action: Validate as initial key provisioning (case where we are migrating
  // from unsigned policy)
  //
  // 4) Validation after loading new policy from the server with a cached key
  // Action: Validate as normal, and allow key rotation.
  if (cached_key) {
    // Case #1/#2 - loading from cache. Validate the cached key (if no key,
    // then the validation will fail), then do normal policy data signature
    // validation using the cached key.

    // Loading from cache should not change the cached keys.
    DCHECK(persisted_policy_key_.empty() ||
           persisted_policy_key_ == cached_key->signing_key());
    DLOG_IF(WARNING, !cached_key->has_signing_key())
        << "Unsigned policy blob detected";

    validator->ValidateCachedKey(cached_key->signing_key(),
                                 cached_key->signing_key_signature(),
                                 owning_domain);
    // Loading from cache, so don't allow key rotation.
    validator->ValidateSignature(cached_key->signing_key());
  } else {
    // No passed cached_key - this is not validating the initial policy load
    // from cache, but rather an update from the server.
    if (persisted_policy_key_.empty()) {
      // Case #3 - no valid existing policy key (either this is the initial
      // policy fetch, or we're doing a key rotation), so this new policy fetch
      // should include an initial key provision.
      validator->ValidateInitialKey(owning_domain);
    } else {
      // Case #4 - verify new policy with existing key. We always allow key
      // rotation - the verification key will prevent invalid policy from being
      // injected. |persisted_policy_key_| is already known to be valid, so no
      // need to verify via ValidateCachedKey().
      validator->ValidateSignatureAllowingRotation(persisted_policy_key_,
                                                   owning_domain);
    }
  }
}

void DesktopCloudPolicyStore::InstallLoadedPolicyAfterValidation(
    bool doing_key_rotation,
    const std::string& signing_key,
    UserCloudPolicyValidator* validator) {
  // TODO(zmin): metrics
  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.UserCloudPolicyStore.LoadValidationStatus",
      validator->status(), CloudPolicyValidatorBase::VALIDATION_STATUS_SIZE);
  validation_result_ = validator->GetValidationResult();
  if (!validator->success()) {
    DVLOG(1) << "Validation failed: status=" << validator->status();
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  DVLOG(1) << "Validation succeeded - installing policy with dm_token: "
           << validator->policy_data()->request_token();
  DVLOG(1) << "Device ID: " << validator->policy_data()->device_id();

  // If we're doing a key rotation, clear the public key version so a future
  // policy fetch will force regeneration of the keys.
  if (doing_key_rotation) {
    validator->policy_data()->clear_public_key_version();
    persisted_policy_key_.clear();
  } else {
    // Policy validation succeeded, so we know the signing key is good.
    persisted_policy_key_ = signing_key;
  }

  InstallPolicy(std::move(validator->policy_data()),
                std::move(validator->payload()), persisted_policy_key_);
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

void DesktopCloudPolicyStore::Store(const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();

  std::unique_ptr<em::PolicyFetchResponse> policy_copy(
      new em::PolicyFetchResponse(policy));
  Validate(
      std::move(policy_copy), std::unique_ptr<em::PolicySigningKey>(), true,
      base::BindRepeating(&DesktopCloudPolicyStore::OnPolicyToStoreValidated,
                          weak_factory_.GetWeakPtr()));
}

void DesktopCloudPolicyStore::OnPolicyToStoreValidated(
    UserCloudPolicyValidator* validator) {
  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.UserCloudPolicyStore.StoreValidationStatus",
      validator->status(), CloudPolicyValidatorBase::VALIDATION_STATUS_SIZE);
  validation_result_ = validator->GetValidationResult();
  DVLOG(1) << "Policy validation complete: status = " << validator->status();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  // Persist the validated policy (just fire a task - don't bother getting a
  // reply because we can't do anything if it fails).
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindRepeating(&StorePolicyToDiskOnBackgroundThread, policy_path_,
                          key_path_, *validator->policy()));

  // If the key was rotated, update our local cache of the key.
  if (validator->policy()->has_new_public_key())
    persisted_policy_key_ = validator->policy()->new_public_key();

  InstallPolicy(std::move(validator->policy_data()),
                std::move(validator->payload()), persisted_policy_key_);
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

UserCloudPolicyStore::UserCloudPolicyStore(
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : DesktopCloudPolicyStore(policy_path,
                              key_path,
                              background_task_runner,
                              PolicyScope::POLICY_SCOPE_USER,
                              PolicySource::POLICY_SOURCE_CLOUD) {}

UserCloudPolicyStore::~UserCloudPolicyStore() {}

// static
std::unique_ptr<UserCloudPolicyStore> UserCloudPolicyStore::Create(
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  base::FilePath policy_path =
      profile_path.Append(kPolicyDir).Append(kPolicyCacheFile);
  base::FilePath key_path =
      profile_path.Append(kPolicyDir).Append(kKeyCacheFile);
  return base::WrapUnique(
      new UserCloudPolicyStore(policy_path, key_path, background_task_runner));
}

void UserCloudPolicyStore::SetSigninAccountId(const AccountId& account_id) {
  account_id_ = account_id;
}

void UserCloudPolicyStore::Validate(
    std::unique_ptr<em::PolicyFetchResponse> policy,
    std::unique_ptr<em::PolicySigningKey> cached_key,
    bool validate_in_background,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  // Configure the validator.
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);

  // Extract the owning domain from the signed-in user (if any is set yet).
  // If there's no owning domain, then the code just ensures that the policy
  // is self-consistent (that the keys are signed with the same domain that the
  // username field in the policy contains). UserPolicySigninServerBase will
  // verify that the username matches the signed in user once profile
  // initialization is complete (http://crbug.com/342327).
  std::string owning_domain;

  // Validate the account id if the user is signed in. The account_id_ can
  // be empty during initial policy load because this happens before the
  // Prefs subsystem is initialized.
  if (account_id_.is_valid()) {
    DVLOG(1) << "Validating account: " << account_id_;
    validator->ValidateUser(account_id_);
    owning_domain = gaia::ExtractDomainName(gaia::CanonicalizeEmail(
        gaia::SanitizeEmail(account_id_.GetUserEmail())));
  }

  ValidateKeyAndSignature(validator.get(), cached_key.get(), owning_domain);

  if (validate_in_background) {
    // Start validation in the background.
    UserCloudPolicyValidator::StartValidation(std::move(validator), callback);
  } else {
    // Run validation immediately and invoke the callback with the results.
    validator->RunValidation();
    callback.Run(validator.get());
  }
}


}  // namespace policy
