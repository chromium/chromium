// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include <set>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"

namespace device_signals {

namespace {

bool IsNewEvSignalsUnaffiliatedEnabled() {
  return base::FeatureList::IsEnabled(
      enterprise_signals::features::kNewEvSignalsUnaffiliatedEnabled);
}

}  // namespace

UserPermissionServiceImpl::UserPermissionServiceImpl(
    policy::ManagementService* management_service,
    std::unique_ptr<UserDelegate> user_delegate,
    PrefService* user_prefs)
    : management_service_(management_service),
      user_delegate_(std::move(user_delegate)),
      user_prefs_(user_prefs) {
  CHECK(management_service_);
  CHECK(user_delegate_);
  CHECK(user_prefs_);
}

UserPermissionServiceImpl::~UserPermissionServiceImpl() = default;

// Returns a WeakPtr for the current service.
base::WeakPtr<UserPermissionServiceImpl>
UserPermissionServiceImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool UserPermissionServiceImpl::HasUserConsented() const {
  return user_prefs_->GetBoolean(prefs::kDeviceSignalsConsentReceived) ||
         user_prefs_->GetBoolean(prefs::kDeviceSignalsPermanentConsentReceived);
}

bool UserPermissionServiceImpl::ShouldCollectConsent() const {
  if (HasUserConsented()) {
    // Already have the user consent, so no need to collect.
    return false;
  }

  // Unmanaged profiles are not considered unaffiliated contexts.
  bool is_unaffiliated_user = IsDeviceCloudManaged() &&
                              user_delegate_->IsManagedUser() &&
                              !user_delegate_->IsAffiliated();

  bool consent_required_by_specific_policy =
      IsConsentFlowPolicyEnabled() &&
      (!IsDeviceCloudManaged() ||
       (IsNewEvSignalsUnaffiliatedEnabled() && is_unaffiliated_user));

  bool consent_required_by_dependent_policy = false;
  std::set<policy::PolicyScope> scopes =
      user_delegate_->GetPolicyScopesNeedingSignals();
  if (scopes.find(policy::POLICY_SCOPE_USER) != scopes.end()) {
    if (IsDeviceCloudManaged()) {
      // Managed device, only trigger the consent flow if the user is
      // unaffiliated.
      consent_required_by_dependent_policy = is_unaffiliated_user;
    } else {
      // Unmanaged device.
      consent_required_by_dependent_policy = true;
    }
  }

  return consent_required_by_specific_policy ||
         consent_required_by_dependent_policy;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
UserPermission UserPermissionServiceImpl::CanUserCollectSignals(
    const UserContext& user_context) const {
  // Return "unknown user" if no user ID was given.
  if (user_context.user_id.empty()) {
    return UserPermission::kMissingUser;
  }

  if (!user_delegate_->IsSameUser(user_context.user_id)) {
    return UserPermission::kUnknownUser;
  }

  if (!user_delegate_->IsManagedUser()) {
    return UserPermission::kConsumerUser;
  }

  // User consent is required on Cloud-unmanaged devices, or in unaffiliated
  // contexts. Collection of the user's consent happens via its own flow and
  // hooks, so only the resulting value needs to be evaluated here.
  if (!IsDeviceCloudManaged() || !user_delegate_->IsAffiliated()) {
    return HasUserConsented() ? UserPermission::kGranted
                              : UserPermission::kMissingConsent;
  }

  // At this point, the given user is:
  // - The same user as the browser user,
  // - Is managed by an org affiliated with the org managing the browser.
  // They are, therefore, allowed to collect signals.
  return UserPermission::kGranted;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX

UserPermission UserPermissionServiceImpl::CanCollectSignals() const {
  if (HasUserConsented()) {
    return UserPermission::kGranted;
  }

  if (!IsDeviceCloudManaged()) {
    // Consent is required on unmanaged devices.
    return UserPermission::kMissingConsent;
  }

  if (!user_delegate_->IsManagedUser() || user_delegate_->IsAffiliated()) {
    // Grant access to signals if the profile is unmanaged or affiliated.
    return UserPermission::kGranted;
  }

  // In unaffiliated contexts, signals can only be collected without consent if
  // they are solely needed by a device-level policy.
  std::set<policy::PolicyScope> scopes =
      user_delegate_->GetPolicyScopesNeedingSignals();
  bool only_needed_by_device =
      (scopes.find(policy::POLICY_SCOPE_MACHINE) != scopes.end()) &&
      scopes.size() == 1U;
  return only_needed_by_device ? UserPermission::kGranted
                               : UserPermission::kMissingConsent;
}

bool UserPermissionServiceImpl::IsConsentFlowPolicyEnabled() const {
  return user_prefs_->GetBoolean(
      prefs::kUnmanagedDeviceSignalsConsentFlowEnabled);
}

bool UserPermissionServiceImpl::IsDeviceCloudManaged() const {
  return management_service_->HasManagementAuthority(
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
}

}  // namespace device_signals
