// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_STORE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_export.h"

namespace enterprise_management {
class PolicyData;
class PolicyFetchResponse;
}

namespace policy {

class CloudExternalDataManager;

// Defines the low-level interface used by the cloud policy code to:
//   1. Validate policy blobs that should be applied locally
//   2. Persist policy blobs
//   3. Decode policy blobs to PolicyMap representation
class POLICY_EXPORT CloudPolicyStore {
 public:
  // Status codes.
  enum Status {
    // Everything is in good order.
    STATUS_OK,
    // Loading policy from the underlying data store failed.
    STATUS_LOAD_ERROR,
    // Failed to store policy to the data store.
    STATUS_STORE_ERROR,
    // Failed to parse the policy read from the data store.
    STATUS_PARSE_ERROR,
    // Failed to serialize policy for storage.
    STATUS_SERIALIZE_ERROR,
    // Validation error.
    STATUS_VALIDATION_ERROR,
    // Store cannot accept policy (e.g. non-enterprise device).
    STATUS_BAD_STATE,
  };

  // Callbacks for policy store events. Most importantly, policy updates.
  class POLICY_EXPORT Observer {
   public:
    virtual ~Observer();

    // Called on changes to store->policy() and/or store->policy_map().
    virtual void OnStoreLoaded(CloudPolicyStore* store) = 0;

    // Called upon encountering errors.
    virtual void OnStoreError(CloudPolicyStore* store) = 0;

    // Called upon store destruction.
    virtual void OnStoreDestruction(CloudPolicyStore* store);
  };

  CloudPolicyStore();
  CloudPolicyStore(const CloudPolicyStore&) = delete;
  CloudPolicyStore& operator=(const CloudPolicyStore&) = delete;
  virtual ~CloudPolicyStore();

  // Indicates whether the store has been fully initialized. This is
  // accomplished by calling Load() after startup.
  bool is_initialized() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_initialized_;
  }

  base::WeakPtr<CloudExternalDataManager> external_data_manager() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return external_data_manager_;
  }

  const PolicyMap& policy_map() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return policy_map_;
  }
  bool has_policy() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(policy_.get() != nullptr,
              policy_fetch_response_.get() != nullptr);
    return policy_.get() != nullptr;
  }
  const enterprise_management::PolicyData* policy() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return policy_.get();
  }
  const enterprise_management::PolicyFetchResponse* policy_fetch_response()
      const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return policy_fetch_response_.get();
  }
  bool is_managed() const;
  Status status() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }
  bool first_policies_loaded() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return first_policies_loaded_;
  }
  CloudPolicyValidatorBase::Status validation_status() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return validation_result_.get() ? validation_result_->status
                                    : CloudPolicyValidatorBase::VALIDATION_OK;
  }
  const CloudPolicyValidatorBase::ValidationResult* validation_result() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return validation_result_.get();
  }
  const std::string& policy_signature_public_key() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return policy_signature_public_key_;
  }

  // Store a new policy blob. Pending load/store operations will be canceled.
  // The store operation may proceed asynchronously and observers are notified
  // once the operation finishes. If successful, OnStoreLoaded() will be invoked
  // on the observers and the updated policy can be read through policy().
  // Errors generate OnStoreError() notifications.
  // |invalidation_version| is the invalidation version of the policy to be
  // stored.
  void Store(const enterprise_management::PolicyFetchResponse& policy,
             int64_t invalidation_version);

  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) = 0;

  // Load the current policy blob from persistent storage. Pending load/store
  // operations will be canceled. This may trigger asynchronous operations.
  // Upon success, OnStoreLoaded() will be called on the registered observers.
  // Otherwise, OnStoreError() reports the reason for failure.
  virtual void Load() = 0;

  // Registers an observer to be notified when policy changes.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

  // Checks if `observer` was already added.
  bool HasObserver(CloudPolicyStore::Observer* observer);

  // The invalidation version of the last policy stored. This value can be read
  // by observers to determine which version of the policy is now available.
  int64_t invalidation_version() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return invalidation_version_;
  }

  // Indicate that external data referenced by policies in this store is managed
  // by |external_data_manager|. The |external_data_manager| will be notified
  // about policy changes before any other observers.
  void SetExternalDataManager(
      base::WeakPtr<CloudExternalDataManager> external_data_manager);

  // Sets whether or not the first policies for this policy store were loaded.
  void SetFirstPoliciesLoaded(bool loaded);

  // Test helper to set |policy_|.
  void set_policy_data_for_testing(
      std::unique_ptr<enterprise_management::PolicyData> policy);

  void set_policy_signature_public_key_for_testing(const std::string& key);

 protected:
  // Invokes the corresponding callback on all registered observers.
  void NotifyStoreLoaded();
  void NotifyStoreError();
  void NotifyStoreDestruction();

  // Updates whether or not the first policies were loaded.
  virtual void UpdateFirstPoliciesLoaded();

  void SetPolicy(
      std::unique_ptr<enterprise_management::PolicyFetchResponse>
          policy_fetch_response,
      std::unique_ptr<enterprise_management::PolicyData> policy_data);
  void ResetPolicy();

  // Assert non-concurrent usage in debug builds.
  SEQUENCE_CHECKER(sequence_checker_);

  // Manages external data referenced by policies.
  base::WeakPtr<CloudExternalDataManager> external_data_manager_;

  // Decoded version of the currently effective policy.
  PolicyMap policy_map_;

  // Latest status code.
  Status status_ = STATUS_OK;

  bool first_policies_loaded_ = false;

  // Latest validation result.
  std::unique_ptr<CloudPolicyValidatorBase::ValidationResult>
      validation_result_;

  // The invalidation version of the last policy stored.
  int64_t invalidation_version_ = 0;

  // The public part of signing key that is used by the currently effective
  // policy. The subclasses should keep its value up to date to correspond to
  // the currently effective policy. The member should be empty if no policy is
  // currently effective, or if signature verification was not possible for the
  // policy.
  std::string policy_signature_public_key_;

 private:
  // Whether the store has completed asynchronous initialization, which is
  // triggered by calling Load().
  bool is_initialized_ = false;

  // Currently effective policy. Should be always in sync and kept private.
  // Use `SetPolicy()` and `ResetPolicy()` to alter the fields.
  std::unique_ptr<enterprise_management::PolicyFetchResponse>
      policy_fetch_response_;
  std::unique_ptr<enterprise_management::PolicyData> policy_;

  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_STORE_H_
