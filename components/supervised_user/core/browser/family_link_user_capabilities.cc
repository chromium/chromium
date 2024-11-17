// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_user_capabilities.h"

#include "base/no_destructor.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

// Returns the list of capabilities observed by
// FamilyLinkUserCapabilitiesObserver.
const std::vector<std::string>& GetFamilyLinkUserCapabilityNames() {
  static base::NoDestructor<std::vector<std::string>> names{
      {kIsSubjectToParentalControlsCapabilityName,
       kCanFetchFamilyMemberInfoCapabilityName}};
  return *names;
}

}  // namespace

namespace supervised_user {

signin::Tribool IsPrimaryAccountSubjectToParentalControls(
    signin::IdentityManager* identity_manager) {
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    // Signed-out users are not subject to parental controls.
    return signin::Tribool::kFalse;
  }
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  return account_info.capabilities.is_subject_to_parental_controls();
}

FamilyLinkUserCapabilitiesObserver::FamilyLinkUserCapabilitiesObserver(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_observation_.Observe(identity_manager_);
}

FamilyLinkUserCapabilitiesObserver::~FamilyLinkUserCapabilitiesObserver() {
  identity_manager_observation_.Reset();
}

void FamilyLinkUserCapabilitiesObserver::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK(identity_manager_);
  // Only observe updates to capabilities of the primary account.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  for (const std::string& name : GetFamilyLinkUserCapabilityNames()) {
    signin::Tribool new_capability_value =
        info.capabilities.GetCapabilityByName(name);
    // Do not override known capability values with kUnknown.
    if (new_capability_value == signin::Tribool::kUnknown) {
      continue;
    }
    CapabilityUpdateState capability_update_state =
        (new_capability_value == signin::Tribool::kTrue)
            ? CapabilityUpdateState::kSetToTrue
            : CapabilityUpdateState::kSetToFalse;
    NotifyCapabilityChange(name, capability_update_state);
  }
}

void FamilyLinkUserCapabilitiesObserver::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      AccountInfo primary_account_info =
          identity_manager_->FindExtendedAccountInfo(
              event_details.GetCurrentState().primary_account);
      OnExtendedAccountInfoUpdated(primary_account_info);
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Update and notify previously known capabilities.
      for (const std::string& name : GetFamilyLinkUserCapabilityNames()) {
        NotifyCapabilityChange(name, CapabilityUpdateState::kDetached);
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void FamilyLinkUserCapabilitiesObserver::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK(identity_manager_observation_.IsObservingSource(identity_manager));
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

void FamilyLinkUserCapabilitiesObserver::NotifyCapabilityChange(
    const std::string& name,
    CapabilityUpdateState capability_update_state) {
  if (name == kIsSubjectToParentalControlsCapabilityName) {
    OnIsSubjectToParentalControlsCapabilityChanged(capability_update_state);
  } else if (name == kCanFetchFamilyMemberInfoCapabilityName) {
    OnCanFetchFamilyMemberInfoCapabilityChanged(capability_update_state);
  }
}

}  // namespace supervised_user
