// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace collaboration {

class CollaborationService;
class ControllerState;

// The class for managing a single collaboration group flow.
class CollaborationController {
 public:
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

    // Waiting for tab group to be added in sync and people group to be added in
    // DataSharing. Loading UI should be shown.
    kWaitingForSyncAndDataSharingGroup,

    // Delegate is promoting the local tab group.
    kOpeningLocalTabGroup,

    // The flow is cancelled.
    kCancel,

    // An error occurred and need to be shown to the user.
    kError,
  };

  enum class Flow {
    kJoin,
    kShare,
  };

  using FinishCallback = base::OnceCallback<void()>;

  explicit CollaborationController(
      const Flow& flow,
      const data_sharing::GroupToken& token,
      CollaborationService* collaboration_service,
      data_sharing::DataSharingService* data_sharing_service,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      FinishCallback finish_and_delete);
  ~CollaborationController();

  // Disallow copy/assign.
  CollaborationController(const CollaborationController&) = delete;
  CollaborationController& operator=(const CollaborationController&) = delete;

  // Getters.
  CollaborationControllerDelegate* delegate() { return delegate_.get(); }
  data_sharing::DataSharingService* data_sharing_service() {
    return data_sharing_service_.get();
  }
  tab_groups::TabGroupSyncService* tab_group_sync_service() {
    return tab_group_sync_service_.get();
  }
  syncer::SyncService* sync_service() { return sync_service_.get(); }
  const data_sharing::GroupToken& token() { return token_; }
  CollaborationService* collaboration_service() {
    return collaboration_service_.get();
  }
  const Flow& flow() { return flow_; }

  // Called to transition to another state.
  void TransitionTo(
      StateId state,
      const CollaborationControllerDelegate::ErrorInfo& error =
          CollaborationControllerDelegate::ErrorInfo(
              CollaborationControllerDelegate::ErrorInfo::Type::kUnknown));

  // Called to refocus the current flow.
  void PromoteCurrentSession();

  // Called when the flow is finished to exit and clean itself up in the
  // service.
  void Exit();

  // Helper functions used in tests.
  void SetStateForTesting(StateId state);
  StateId GetStateForTesting();

 private:
  static constexpr std::array<std::pair<StateId, StateId>, 18>
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
          //   kWaitingForSyncAndDataSharingGroup: When user is in current
          //   people group,
          //   but tab group not found in sync.
          //   kOpeningLocalTabGroup: When user is in current people group, and
          //   tab group found in sync.
          //   kError: An error occurred while checking requirements. This could
          //   be due to version mismatch.
          {StateId::kCheckingFlowRequirements, StateId::kAddingUserToGroup},
          {StateId::kCheckingFlowRequirements,
           StateId::kWaitingForSyncAndDataSharingGroup},
          {StateId::kCheckingFlowRequirements, StateId::kOpeningLocalTabGroup},
          {StateId::kCheckingFlowRequirements, StateId::kError},

          // kAddingUserToGroup transition to:
          //
          //   kWaitingForSyncAndDataSharingGroup: After the user accept the
          //   join
          //   invitation and the tab group is not yet added in sync.
          //   kOpeningLocalTabGroup: After the user accept the join invitation
          //   and the tab group is in sync.
          //   kCancel: After the user cancels the join invitation
          //   kError: An error occurred during invitation screen.
          {StateId::kAddingUserToGroup,
           StateId::kWaitingForSyncAndDataSharingGroup},
          {StateId::kAddingUserToGroup, StateId::kOpeningLocalTabGroup},
          {StateId::kAddingUserToGroup, StateId::kCancel},
          {StateId::kAddingUserToGroup, StateId::kError},

          // kWaitingForSyncAndDataSharingGroup transition to:
          //
          //   kOpeningLocalTabGroup: After tab group is added in sync.
          //   kError: An error occurred while waiting for sync tab group.
          {StateId::kWaitingForSyncAndDataSharingGroup,
           StateId::kOpeningLocalTabGroup},
          {StateId::kWaitingForSyncAndDataSharingGroup, StateId::kError},

          // kOpeningLocalTabGroup transition to:
          //
          //   kError: An error occurred while opening local tab group.
          //   kCancel: After the promote is done successfully, cancel the flow
          //   to clean up.
          {StateId::kOpeningLocalTabGroup, StateId::kError},
          {StateId::kOpeningLocalTabGroup, StateId::kCancel},
      }};

  bool IsValidStateTransition(StateId from, StateId to);
  std::unique_ptr<ControllerState> CreateStateObject(StateId state);

  std::unique_ptr<ControllerState> current_state_;

  const Flow flow_;
  const data_sharing::GroupToken token_;
  const raw_ptr<CollaborationService> collaboration_service_;
  const raw_ptr<data_sharing::DataSharingService> data_sharing_service_;
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  const raw_ptr<syncer::SyncService> sync_service_;
  std::unique_ptr<CollaborationControllerDelegate> delegate_;
  FinishCallback finish_and_delete_;
  base::WeakPtrFactory<CollaborationController> weak_ptr_factory_{this};
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_
