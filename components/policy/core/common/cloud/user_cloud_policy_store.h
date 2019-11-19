// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/policy_signing_key.pb.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Implements a cloud policy store that stores policy on desktop. This is used
// on (non-chromeos) platforms that do not have a secure storage
// implementation.
class POLICY_EXPORT DesktopCloudPolicyStore : public UserCloudPolicyStoreBase {
 public:
  DesktopCloudPolicyStore(
      const base::FilePath& policy_file,
      const base::FilePath& key_file,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      PolicyScope policy_scope,
      PolicySource policy_source);
  ~DesktopCloudPolicyStore() override;

  // Loads policy immediately on the current thread. Virtual for mocks.
  virtual void LoadImmediately();

  // Deletes any existing policy blob and notifies observers via OnStoreLoaded()
  // that the blob has changed. Virtual for mocks.
  virtual void Clear();

  // CloudPolicyStore implementation.
  void Load() override;
  void Store(const enterprise_management::PolicyFetchResponse& policy) override;

 protected:
  // Callback invoked when a new policy has been loaded from disk. If
  // |validate_in_background| is true, then policy is validated via a background
  // thread.
  void PolicyLoaded(bool validate_in_background,
                    struct PolicyLoadResult policy_load_result);

  // Starts policy blob validation. |callback| is invoked once validation is
  // complete. If |validate_in_background| is true, then the validation work
  // occurs on a background thread (results are sent back to the calling
  // thread).
  virtual void Validate(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      std::unique_ptr<enterprise_management::PolicySigningKey> key,
      bool validate_in_background,
      const UserCloudPolicyValidator::CompletionCallback& callback) = 0;

  // Validate the |cached_key| with the |owning_domain|.
  void ValidateKeyAndSignature(
      UserCloudPolicyValidator* validator,
      const enterprise_management::PolicySigningKey* cached_key,
      const std::string& owning_domain);

  // Callback invoked to install a just-loaded policy after validation has
  // finished.
  void InstallLoadedPolicyAfterValidation(bool doing_key_rotation,
                                          const std::string& signing_key,
                                          UserCloudPolicyValidator* validator);

  // Callback invoked to store the policy after validation has finished.
  void OnPolicyToStoreValidated(UserCloudPolicyValidator* validator);

  // The current key used to verify signatures of policy. This value is
  // eventually consistent with the one persisted in the key cache file. This
  // is, generally, different from |policy_signature_public_key_| member of
  // the base class CloudPolicyStore, which always corresponds to the currently
  // effective policy.
  std::string persisted_policy_key_;

  // Path to file where we store persisted policy.
  base::FilePath policy_path_;

  // Path to file where we store the signing key for the policy blob.
  base::FilePath key_path_;

  // WeakPtrFactory used to create callbacks for validating and storing policy.
  base::WeakPtrFactory<DesktopCloudPolicyStore> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopCloudPolicyStore);
};

// Implements a cloud policy store that is stored in a simple file in the user's
// profile directory. This is used on (non-chromeos) platforms that do not have
// a secure storage implementation.
//
// The public key, which is used to verify signatures of policy, is also
// persisted in a file. During the load operation, the key is loaded from the
// file and is itself verified against the verification public key before using
// it to verify the policy signature. During the store operation, the key cache
// file is updated whenever the key rotation happens.
class POLICY_EXPORT UserCloudPolicyStore : public DesktopCloudPolicyStore {
 public:
  // Creates a policy store associated with a signed-in (or in the progress of
  // it) user.
  UserCloudPolicyStore(
      const base::FilePath& policy_file,
      const base::FilePath& key_file,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~UserCloudPolicyStore() override;

  // Factory method for creating a UserCloudPolicyStore for a profile with path
  // |profile_path|.
  static std::unique_ptr<UserCloudPolicyStore> Create(
      const base::FilePath& profile_path,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // The account id from signin for validation of the policy.
  const AccountId& signin_account_id() const { return account_id_; }

  // Sets the account id from signin for validation of the policy.
  void SetSigninAccountId(const AccountId& account_id);

 private:
  void Validate(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      std::unique_ptr<enterprise_management::PolicySigningKey> key,
      bool validate_in_background,
      const UserCloudPolicyValidator::CompletionCallback& callback) override;

  // The account id from signin for validation of the policy.
  AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStore);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_H_
