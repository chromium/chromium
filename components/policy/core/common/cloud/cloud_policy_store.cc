// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_store.h"

#include <memory>

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

CloudPolicyStore::Observer::~Observer() = default;
void CloudPolicyStore::Observer::OnStoreDestruction(CloudPolicyStore* store) {}

CloudPolicyStore::CloudPolicyStore(const std::string& policy_type)
    : policy_type_(policy_type) {
  CHECK(!policy_type.empty());
}

CloudPolicyStore::~CloudPolicyStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!external_data_manager_);
  NotifyStoreDestruction();
}

bool CloudPolicyStore::is_managed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return policy_.get() &&
         policy_->state() == enterprise_management::PolicyData::ACTIVE;
}

void CloudPolicyStore::Store(
    const enterprise_management::PolicyFetchResponse& policy,
    int64_t invalidation_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  invalidation_version_ = invalidation_version;
  Store(policy);
}

void CloudPolicyStore::AddObserver(CloudPolicyStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void CloudPolicyStore::RemoveObserver(CloudPolicyStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

bool CloudPolicyStore::HasObserver(CloudPolicyStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return observers_.HasObserver(observer);
}

void CloudPolicyStore::NotifyStoreLoaded() {
  is_initialized_ = true;
  UpdateFirstPoliciesLoaded();

  // The |external_data_manager_| must be notified first so that when other
  // observers are informed about the changed policies and try to fetch external
  // data referenced by these, the |external_data_manager_| has the required
  // metadata already.
  if (external_data_manager_)
    external_data_manager_->OnPolicyStoreLoaded();
  for (auto& observer : observers_)
    observer.OnStoreLoaded(this);
}

void CloudPolicyStore::UpdateFirstPoliciesLoaded() {
  first_policies_loaded_ |= has_policy();
}

void CloudPolicyStore::SetPolicy(
    std::unique_ptr<enterprise_management::PolicyData> policy_data) {
  VLOG_POLICY(1, CBCM_ENROLLMENT)
      << PolicyTypeLogPrefix(policy_type(), std::string())
      << "CloudPolicyStore::SetPolicy: " << policy_data->policy_type();
  DCHECK(policy_data);
  DCHECK_EQ(policy_data->policy_type(), policy_type());
  // CloudPolicyStore doesn't need to store `policy_value`, only its hash.
  // `policy_value` can be pretty large (up to 2MB), so free it to save memory
  // with no downsides.
  policy_value_hash_ = policy_data->has_policy_value()
                           ? base::FastHash(policy_data->policy_value())
                           : 0;
  // Manually release and destroy the string buffer to free heap memory, because
  // clear_policy_value() retains capacity.
  std::unique_ptr<std::string> discard_policy_value(
      policy_data->release_policy_value());
  policy_ = std::move(policy_data);
}

void CloudPolicyStore::ResetPolicy() {
  policy_.reset();
  policy_value_hash_ = 0;
}

void CloudPolicyStore::NotifyStoreError() {
  is_initialized_ = true;
  UpdateFirstPoliciesLoaded();

  for (auto& observer : observers_)
    observer.OnStoreError(this);
}

void CloudPolicyStore::NotifyStoreDestruction() {
  for (auto& observer : observers_)
    observer.OnStoreDestruction(this);
}

void CloudPolicyStore::SetExternalDataManager(
    base::WeakPtr<CloudExternalDataManager> external_data_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!external_data_manager_);

  external_data_manager_ = external_data_manager;
  if (is_initialized_)
    external_data_manager_->OnPolicyStoreLoaded();
}

void CloudPolicyStore::SetFirstPoliciesLoaded(bool loaded) {
  first_policies_loaded_ = loaded;
}

void CloudPolicyStore::set_policy_data_for_testing(
    std::unique_ptr<enterprise_management::PolicyData> policy) {
  // See comments in SetPolicy().
  policy_value_hash_ = policy && policy->has_policy_value()
                           ? base::FastHash(policy->policy_value())
                           : 0;
  if (policy) {
    std::unique_ptr<std::string> discard_policy_value(
        policy->release_policy_value());
  }
  policy_ = std::move(policy);
}

void CloudPolicyStore::set_policy_signature_public_key_for_testing(
    const std::string& key) {
  policy_signature_public_key_ = key;
}

}  // namespace policy
