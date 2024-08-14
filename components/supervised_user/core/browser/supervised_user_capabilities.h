// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CAPABILITIES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CAPABILITIES_H_

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace supervised_user {

// Possible updates made to signin::Tribool capability values of
// AccountCapabilities. Note that capability values cannot be updated to
// signin::Tribool::kUnknown.
enum class CapabilityUpdateState { kSetToTrue, kSetToFalse, kDetached };

// Returns the IsSubjectToParentalControls capability value of the primary
// account if available. Signed-out users will default to
// signin::Tribool::kFalse.
signin::Tribool IsPrimaryAccountSubjectToParentalControls(
    signin::IdentityManager* identity_manager);

// Wrapper of IdentityManager::Observer, processing account capabilities
// relevant to supervised users.
class SupervisedUserCapabilitiesObserver
    : public signin::IdentityManager::Observer {
 public:
  explicit SupervisedUserCapabilitiesObserver(
      signin::IdentityManager* identity_manager);

  SupervisedUserCapabilitiesObserver(
      const SupervisedUserCapabilitiesObserver&) = delete;

  SupervisedUserCapabilitiesObserver& operator=(
      const SupervisedUserCapabilitiesObserver&) = delete;

  ~SupervisedUserCapabilitiesObserver() override;

  // Called when the IsSubjectToParentalControls capability for the primary
  // account become available or unavailable.
  virtual void OnIsSubjectToParentalControlsCapabilityChanged(
      CapabilityUpdateState capability_update_state) = 0;

 private:
  // signin::IdentityManager::Observer implementation.
  // Process signout events, during which capabilities are removed.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  // Process updates where supervised user capabilities become available.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  void NotifyCapabilityChange(const std::string& name,
                              CapabilityUpdateState capability_update_state);

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_CAPABILITIES_H_
