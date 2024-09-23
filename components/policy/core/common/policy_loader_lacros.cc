// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace {

// Remembers if the main user is managed or not.
// Note: This is a pessimistic default (no policies read - false) and
// once the profile is loaded, the value is set and will never change in
// production. The value changes in tests whenever policy data gets overridden.
bool g_is_main_user_managed_ = false;

enterprise_management::PolicyData* MainUserPolicyDataStorage() {
  static enterprise_management::PolicyData policy_data;
  return &policy_data;
}

bool IsManaged(const enterprise_management::PolicyData& policy_data) {
  return policy_data.state() == enterprise_management::PolicyData::ACTIVE;
}

// Returns whether a primary device account for this session is child.
bool IsChildSession() {
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  return init_params->SessionType() ==
         crosapi::mojom::SessionType::kChildSession;
}

}  // namespace

namespace policy {

PolicyLoaderLacros::PolicyLoaderLacros(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    PolicyPerProfileFilter per_profile)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/false),
      per_profile_(per_profile) {
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (per_profile_ == PolicyPerProfileFilter::kTrue &&
      init_params->DeviceAccountComponentPolicy()) {
    SetComponentPolicy(init_params->DeviceAccountComponentPolicy().value());
  }
  if (!init_params->DeviceAccountPolicy().has_value()) {
    LOG(ERROR) << "No policy data";
    return;
  }
  policy_fetch_response_ = init_params->DeviceAccountPolicy().value();
  last_fetch_timestamp_ =
      base::Time::FromTimeT(init_params->LastPolicyFetchAttemptTimestamp());
}

PolicyLoaderLacros::~PolicyLoaderLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service) {
    lacros_service->RemoveObserver(this);
  }
}

void PolicyLoaderLacros::InitOnBackgroundThread() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // We add this as observer on background thread to avoid a situation when
  // notification comes after the object is destroyed, but not removed from the
  // list yet.
  // TODO(crbug.com/40143748): Set up LacrosService in tests.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service) {
    lacros_service->AddObserver(this);
  }
}

PolicyBundle PolicyLoaderLacros::Load() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PolicyBundle bundle;

  // If per_profile loader is used, apply policy for extensions.
  if (per_profile_ == PolicyPerProfileFilter::kTrue && component_policy_)
    bundle.MergeFrom(*component_policy_);

  if (!policy_fetch_response_ || policy_fetch_response_->empty()) {
    return bundle;
  }

  auto policy = std::make_unique<enterprise_management::PolicyFetchResponse>();
  if (!policy->ParseFromArray(policy_fetch_response_.value().data(),
                              policy_fetch_response_->size())) {
    LOG(ERROR) << "Failed to parse policy data";
    return bundle;
  }
  UserCloudPolicyValidator validator(std::move(policy),
                                     /*background_task_runner=*/nullptr);
  validator.ValidatePayload();
  validator.RunValidation();

  PolicyMap policy_map;
  base::WeakPtr<CloudExternalDataManager> external_data_manager;
  DecodeProtoFields(*(validator.payload()), external_data_manager,
                    PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH,
                    PolicyScope::POLICY_SCOPE_USER, &policy_map, per_profile_);

  // We do not set enterprise defaults for child accounts, because they are
  // consumer users. The same rule is applied to policy in Ash. See
  // UserCloudPolicyManagerAsh.
  if (!IsChildSession()) {
    switch (per_profile_) {
      case PolicyPerProfileFilter::kTrue:
        SetEnterpriseUsersProfileDefaults(&policy_map);
        break;
      case PolicyPerProfileFilter::kFalse:
        SetEnterpriseUsersSystemWideDefaults(&policy_map);
        break;
      case PolicyPerProfileFilter::kAny:
        NOTREACHED_IN_MIGRATION();
    }
  }
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .MergeFrom(policy_map);

  // Remember if the main profile is managed or not.
  if (per_profile_ == PolicyPerProfileFilter::kFalse) {
    g_is_main_user_managed_ = IsManaged(*validator.policy_data());
    if (g_is_main_user_managed_) {
      *MainUserPolicyDataStorage() = *validator.policy_data();
    }
  }
  policy_data_ = std::move(validator.policy_data());

  return bundle;
}

void PolicyLoaderLacros::OnPolicyUpdated(
    const std::vector<uint8_t>& policy_fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy_fetch_response_ = policy_fetch_response;
  Reload(true);
}

void PolicyLoaderLacros::OnPolicyFetchAttempt() {
  last_fetch_timestamp_ = base::Time::Now();
}

void PolicyLoaderLacros::OnComponentPolicyUpdated(
    const policy::ComponentPolicyMap& component_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The component policy is per_profile=true policy. If Lacros is using
  // secondary profile, that policy is loaded directly from DMServer. In case
  // it is using the device account, there are two PolicyLoaderLacros objects
  // present, and we need to store it only in the object with per_profile:True.
  if (per_profile_ == PolicyPerProfileFilter::kFalse) {
    return;
  }

  SetComponentPolicy(component_policy);
  Reload(true);
}

void PolicyLoaderLacros::SetComponentPolicy(
    const policy::ComponentPolicyMap& component_policy) {
  if (component_policy_) {
    component_policy_->Clear();
  } else {
    component_policy_ = std::make_unique<PolicyBundle>();
  }
  for (auto& policy_pair : component_policy) {
    PolicyMap component_policy_map;
    std::string error;
    // The component policy received from Ash is the JSON data corresponding to
    // the policy for the namespace.
    ParseComponentPolicy(policy_pair.second.GetDict().Clone(),
                         POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD_FROM_ASH,
                         &component_policy_map, &error);
    DCHECK(error.empty());

    // The data is also good; expose the policies.
    component_policy_->Get(policy_pair.first).Swap(&component_policy_map);
  }
}

enterprise_management::PolicyData* PolicyLoaderLacros::GetPolicyData() {
  if (!policy_fetch_response_ || !policy_data_)
    return nullptr;

  return policy_data_.get();
}

// static
bool PolicyLoaderLacros::IsDeviceLocalAccountUser() {
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  crosapi::mojom::SessionType session_type = init_params->SessionType();
  return session_type == crosapi::mojom::SessionType::kPublicSession ||
         session_type == crosapi::mojom::SessionType::kWebKioskSession ||
         session_type == crosapi::mojom::SessionType::kAppKioskSession;
}

// static
bool PolicyLoaderLacros::IsMainUserManaged() {
  return g_is_main_user_managed_;
}

// static
bool PolicyLoaderLacros::IsMainUserAffiliated() {
  const enterprise_management::PolicyData* policy =
      policy::PolicyLoaderLacros::main_user_policy_data();

  // To align with `DeviceLocalAccountUserBase::IsAffiliated()`, a device local
  // account user is always treated as affiliated.
  if (IsDeviceLocalAccountUser()) {
    return true;
  }

  const auto& device_ids = PolicyLoaderLacros::device_affiliation_ids();
  if (policy && !policy->user_affiliation_ids().empty() &&
      !device_ids.empty()) {
    const auto& user_ids = policy->user_affiliation_ids();
    return policy::IsAffiliated({user_ids.begin(), user_ids.end()},
                                {device_ids.begin(), device_ids.end()});
  }
  return false;
}

// static
const enterprise_management::PolicyData*
PolicyLoaderLacros::main_user_policy_data() {
  return MainUserPolicyDataStorage();
}

// static
void PolicyLoaderLacros::set_main_user_policy_data_for_testing(
    const enterprise_management::PolicyData& policy_data) {
  *MainUserPolicyDataStorage() = policy_data;
  g_is_main_user_managed_ = IsManaged(policy_data);
}

// static
const std::vector<std::string> PolicyLoaderLacros::device_affiliation_ids() {
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (!init_params->DeviceProperties()) {
    return {};
  }
  if (!init_params->DeviceProperties()->device_affiliation_ids.has_value()) {
    return {};
  }
  return init_params->DeviceProperties()->device_affiliation_ids.value();
}

// static
const std::string PolicyLoaderLacros::device_dm_token() {
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (!init_params->DeviceProperties()) {
    return std::string();
  }

  return init_params->DeviceProperties()->device_dm_token;
}

}  // namespace policy
