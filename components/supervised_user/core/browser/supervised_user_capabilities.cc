// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_capabilities.h"

#include "base/no_destructor.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

// Returns the list of capabilities observed by
// SupervisedUserCapabilitiesObserver.
const std::vector<std::string>& GetSupervisedUserCapabilityNames() {
  static base::NoDestructor<std::vector<std::string>> names(
      {kIsSubjectToParentalControlsCapabilityName});
  return *names;
}

}  // namespace

namespace supervised_user {

signin::Tribool IsPrimaryAccountSubjectToParentalControls(
    signin::IdentityManager* identity_manager) {
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  return account_info.capabilities.is_subject_to_parental_controls();
}

SupervisedUserCapabilitiesObserver::SupervisedUserCapabilitiesObserver(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_observation_.Observe(identity_manager_);
}

SupervisedUserCapabilitiesObserver::~SupervisedUserCapabilitiesObserver() {
  identity_manager_observation_.Reset();
}

void SupervisedUserCapabilitiesObserver::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK(identity_manager_);
  // Only observe updates to capabilities of the primary account.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  for (const std::string& name : GetSupervisedUserCapabilityNames()) {
    signin::Tribool new_capability_value =
        info.capabilities.GetCapabilityByName(name);
    // Do not override known capability values with kUnknown.
    if (new_capability_value == signin::Tribool::kUnknown) {
      continue;
    }
    // Update cache as capability values become available or change, then notify
    // observers.
    const auto iterator = capabilities_map_.find(name);
    if (iterator == capabilities_map_.end() ||
        iterator->second != new_capability_value) {
      DCHECK_NE(new_capability_value, signin::Tribool::kUnknown);
      capabilities_map_[name] = new_capability_value;
      CapabilityUpdateState capability_update_state =
          (new_capability_value == signin::Tribool::kTrue)
              ? CapabilityUpdateState::kSetToTrue
              : CapabilityUpdateState::kSetToFalse;
      NotifyCapabilityChange(name, capability_update_state);
    }
  }
}

void SupervisedUserCapabilitiesObserver::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Ignore non-signout events.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    return;
  }
  // Update and notify previously known capabilities.
  for (const auto& kv : capabilities_map_) {
    NotifyCapabilityChange(/*name=*/kv.first, CapabilityUpdateState::kDetached);
  }
  capabilities_map_.clear();
}

void SupervisedUserCapabilitiesObserver::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK(identity_manager_observation_.IsObservingSource(identity_manager));
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

void SupervisedUserCapabilitiesObserver::NotifyCapabilityChange(
    const std::string& name,
    CapabilityUpdateState capability_update_state) {
  if (name == kIsSubjectToParentalControlsCapabilityName) {
    OnIsSubjectToParentalControlsCapabilityChanged(capability_update_state);
  }
}

}  // namespace supervised_user
