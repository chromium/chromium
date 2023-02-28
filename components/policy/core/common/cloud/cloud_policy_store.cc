// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_store.h"

#include "base/check.h"
#include "base/observer_list.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

CloudPolicyStore::Observer::~Observer() = default;
void CloudPolicyStore::Observer::OnStoreDestruction(CloudPolicyStore* store) {}

CloudPolicyStore::CloudPolicyStore() = default;

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
    std::unique_ptr<enterprise_management::PolicyFetchResponse>
        policy_fetch_response,
    std::unique_ptr<enterprise_management::PolicyData> policy_data) {
  DCHECK(policy_fetch_response);
  DCHECK(policy_data);
  policy_fetch_response_ = std::move(policy_fetch_response);
  policy_ = std::move(policy_data);
}

void CloudPolicyStore::ResetPolicy() {
  policy_.reset();
  policy_fetch_response_.reset();
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
  policy_ = std::move(policy);
  if (policy_) {
    policy_fetch_response_ =
        std::make_unique<enterprise_management::PolicyFetchResponse>();
    policy_fetch_response_->set_policy_data(policy_->SerializeAsString());
  } else {
    policy_fetch_response_.reset();
  }
}

void CloudPolicyStore::set_policy_signature_public_key_for_testing(
    const std::string& key) {
  policy_signature_public_key_ = key;
}

}  // namespace policy
