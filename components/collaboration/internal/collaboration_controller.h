// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"

namespace collaboration {

// The class for managing a single collaboration group flow.
class CollaborationController {
 public:
  explicit CollaborationController(
      std::unique_ptr<CollaborationControllerDelegate> delegate);
  ~CollaborationController();

  // Disallow copy/assign.
  CollaborationController(const CollaborationController&) = delete;
  CollaborationController& operator=(const CollaborationController&) = delete;

 private:
  // States of a collaboration group flow. All new flows starts PENDNG.
  enum class StateId {
    // Initial state. The request has been received, awaiting delegate to be
    // initialized and authentication status to be verified.
    kPending,

    // UI is showing authentication screens (sign-in/sync/access token). Waiting
    // for result.
    kAuthenticating,

    // Authentication is completed. Controller will check requirements for each
    // specific flows.
    kCheckingFlowRequirements,

    // Delegate is showing invitation screen to the user.
    kAddingUserToGroup,

    // Waiting for tab group to be added in sync. Loading UI should be shown.
    kWaitingForSyncTabGroup,

    // Delegate is promoting the local tab group.
    kOpeningLocalTabGroup,

    // The flow is cancelled. The controller can safely clean itself up.
    kCancel,

    // An error occurred and need to be shown to the user.
    kError,
  };

  static constexpr std::array<std::pair<StateId, StateId>, 17>
      kValidTransitions = {{
          // kPending transitions to:
          //
          //   kAuthenticating: After all initialization steps complete
          //   successfully and authentication status is not valid.
          //   kCheckingFlowRequirements: After all initialization steps
          //   complete successfully and authentication status is valid.
          //   kError: An error occurred during initialization.
          {StateId::kPending, StateId::kAuthenticating},
          {StateId::kPending, StateId::kCheckingFlowRequirements},
          {StateId::kPending, StateId::kError},

          // kAuthenticating transitions to:
          //
          //   kCheckingFlowRequirements: After all authentication steps are
          //   completed and verified.
          //   kCancel: After the user cancels the process.
          //   kError: An error occurred during authentication.
          {StateId::kAuthenticating, StateId::kCheckingFlowRequirements},
          {StateId::kAuthenticating, StateId::kCancel},
          {StateId::kAuthenticating, StateId::kError},

          // kCheckingFlowRequirements transition to:
          //
          //   kAddingUserToGroup: When user is not in current people group.
          //   kWaitingForSyncTabGroup: When user is in current people group,
          //   but tab group not found in sync.
          //   kOpeningLocalTabGroup: When user is in current people group, and
          //   tab group found in sync.
          {StateId::kCheckingFlowRequirements, StateId::kAddingUserToGroup},
          {StateId::kCheckingFlowRequirements,
           StateId::kWaitingForSyncTabGroup},
          {StateId::kCheckingFlowRequirements, StateId::kOpeningLocalTabGroup},

          // kAddingUserToGroup transition to:
          //
          //   kWaitingForSyncTabGroup: After the user accept the join
          //   invitation and the tab group is not yet added in sync.
          //   kOpeningLocalTabGroup: After the user accept the join invitation
          //   and the tab group is in sync.
          //   kCancel: After the user cancels the join invitation
          //   kError: An error occurred during invitation screen.
          {StateId::kAddingUserToGroup, StateId::kWaitingForSyncTabGroup},
          {StateId::kAddingUserToGroup, StateId::kOpeningLocalTabGroup},
          {StateId::kAddingUserToGroup, StateId::kCancel},
          {StateId::kAddingUserToGroup, StateId::kError},

          // kWaitingForSyncTabGroup transition to:
          //
          //   kOpeningLocalTabGroup: After tab group is added in sync.
          //   kError: An error occurred while waiting for sync tab group.
          {StateId::kWaitingForSyncTabGroup, StateId::kOpeningLocalTabGroup},
          {StateId::kWaitingForSyncTabGroup, StateId::kError},

          // kOpeningLocalTabGroup transition to:
          //
          //   kError: An error occurred while opening local tab group.
          //   kCancel: After the promote is done successfully, cancel the flow
          //   to clean up.
          {StateId::kOpeningLocalTabGroup, StateId::kError},
          {StateId::kOpeningLocalTabGroup, StateId::kCancel},
      }};

  // The instance of the delegate to control UI.
  std::unique_ptr<CollaborationControllerDelegate> delegate_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_
