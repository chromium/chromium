// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/groups/enterprise_groups_handler.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/enterprise/browser/groups/groups_features.h"
#include "components/enterprise/browser/groups/groups_prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace policy {

EnterpriseGroupsHandlerBase::EnterpriseGroupsHandlerBase(
    CloudPolicyCore* cloud_policy_core)
    : cloud_policy_core_(cloud_policy_core) {}

EnterpriseGroupsHandlerBase::~EnterpriseGroupsHandlerBase() = default;

void EnterpriseGroupsHandlerBase::Init() {
  CHECK(cloud_policy_core_);
  if (IsObserving()) {
    return;
  }
  cloud_policy_core_observation_.Observe(cloud_policy_core_);
  if (cloud_policy_core_->IsConnected()) {
    OnCoreConnected(cloud_policy_core_);
    if (cloud_policy_core_->service()->IsInitializationComplete()) {
      OnCloudPolicyServiceInitializationCompleted();
    }
  }
}

void EnterpriseGroupsHandlerBase::OnCoreConnected(CloudPolicyCore* core) {
  cloud_policy_service_observation_.Observe(core->service());
}

void EnterpriseGroupsHandlerBase::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {}

void EnterpriseGroupsHandlerBase::OnCoreDisconnecting(CloudPolicyCore* core) {
  cloud_policy_service_observation_.Reset();
}

void EnterpriseGroupsHandlerBase::OnPolicyRefreshed(bool success) {
  if (success && cloud_policy_core_->service() &&
      cloud_policy_core_->service()->IsInitializationComplete()) {
    UpdateAvailableGroups();
  }
}

void EnterpriseGroupsHandlerBase::
    OnCloudPolicyServiceInitializationCompleted() {
  UpdateAvailableGroups();
}

void EnterpriseGroupsHandlerBase::ResetObservations() {
  cloud_policy_service_observation_.Reset();
  cloud_policy_core_observation_.Reset();
}

bool EnterpriseGroupsHandlerBase::IsObserving() const {
  return cloud_policy_core_observation_.IsObserving();
}

void EnterpriseGroupsHandlerBase::UpdateAvailableGroups() {
  if (!IsFeatureEnabled()) {
    return;
  }
  if (!cloud_policy_core_->store()->first_policies_loaded()) {
    return;
  }
  base::ListValue groups;
  const enterprise_management::PolicyData* policy_data =
      cloud_policy_core_->store()->policy();
  if (policy_data) {
    for (const std::string& group_id : policy_data->available_group_ids()) {
      groups.Append(group_id);
    }
  }
  SaveGroupsToPrefs(std::move(groups));
}

void EnterpriseGroupsHandlerBase::ClearGroups() {
  if (!IsFeatureEnabled()) {
    return;
  }
  SaveGroupsToPrefs(base::ListValue());
}

bool EnterpriseGroupsHandlerBase::IsFeatureEnabled() const {
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(
             enterprise_groups::kEnterpriseGroupsExperiments);
}

EnterpriseGroupsBrowserHandler::EnterpriseGroupsBrowserHandler(
    CloudPolicyCore* cloud_policy_core,
    PrefService* local_state)
    : EnterpriseGroupsHandlerBase(cloud_policy_core),
      local_state_(local_state) {}

EnterpriseGroupsBrowserHandler::~EnterpriseGroupsBrowserHandler() = default;

void EnterpriseGroupsBrowserHandler::SaveGroupsToPrefs(base::ListValue groups) {
  local_state_->SetList(enterprise_groups::kEnterpriseGroupsBrowserPref,
                        std::move(groups));
}

EnterpriseGroupsProfileHandler::EnterpriseGroupsProfileHandler(
    CloudPolicyCore* cloud_policy_core,
    PrefService* local_state,
    const std::string& profile_key)
    : EnterpriseGroupsHandlerBase(cloud_policy_core),
      local_state_(local_state),
      profile_key_(profile_key) {}

EnterpriseGroupsProfileHandler::~EnterpriseGroupsProfileHandler() = default;

void EnterpriseGroupsProfileHandler::Shutdown() {
  ResetObservations();
}

void EnterpriseGroupsProfileHandler::ResetAndClearGroups() {
  ResetObservations();
  ClearGroups();
}

void EnterpriseGroupsProfileHandler::SaveGroupsToPrefs(base::ListValue groups) {
  ScopedDictPrefUpdate groups_prefs_update(
      local_state_, enterprise_groups::kEnterpriseGroupsProfilePref);
  if (groups.empty()) {
    groups_prefs_update->Remove(profile_key_);
  } else {
    groups_prefs_update->Set(profile_key_, std::move(groups));
  }
}

}  // namespace policy
