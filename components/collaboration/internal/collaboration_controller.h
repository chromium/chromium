// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/signin/public/identity_manager/identity_manager.h"

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

    // Waiting on more information about a potentially managed account.
    kWaitingForPolicyUpdate,

    // UI is showing authentication screens (sign-in/sync/access token). Waiting
    // for result.
    kAuthenticating,

    // Waiting for tab group sync service and data sharing service to be ready
    // to use.
    kWaitingForServicesToInitialize,

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

    // Delegate is showing the share sheet.
    kShowingShareScreen,

    // Delegate requested creating a shared tab group.
    kMakingTabGroupShared,

    // Delegate is sharing the tab group's url.
    kSharingTabGroupUrl,

    // Delegate is showing the manage people screen.
    kShowingManageScreen,

    // Delegate is showing the leave group screen.
    kLeavingGroup,

    // Delegate is showing the delete group screen.
    kDeletingGroup,

    // A shared tab group has been deleted, cleaning up.
    kCleaningUpSharedTabGroup,

    // The flow is cancelled.
    kCancel,

    // An error occurred and need to be shown to the user.
    kError,
  };

  class Flow {
   public:
    // Join flow constructor.
    Flow(FlowType type, const data_sharing::GroupToken& token);

    // Share/manage/leave/delete flow constructor.
    Flow(FlowType type, const tab_groups::EitherGroupID& either_id);

    ~Flow();

    Flow(const Flow&);

    const FlowType type;

    const data_sharing::GroupToken& join_token() const {
      DCHECK_EQ(type, FlowType::kJoin);
      return join_token_;
    }

    const tab_groups::EitherGroupID& either_id() const {
      return either_id_;
    }

    const data_sharing::GroupToken& share_token() const {
      DCHECK_EQ(type, FlowType::kShareOrManage);
      CHECK(share_token_.IsValid());
      return share_token_;
    }

    void set_share_token(const data_sharing::GroupToken& token) {
      share_token_ = token;
    }

   private:
    // ID for join flow.
    const data_sharing::GroupToken join_token_;

    // ID for share/manage/leave/delete flow.
    const tab_groups::EitherGroupID either_id_;
    data_sharing::GroupToken share_token_;
  };

  using FinishCallback = base::OnceCallback<void()>;

  explicit CollaborationController(
      const Flow& flow,
      CollaborationService* collaboration_service,
      data_sharing::DataSharingService* data_sharing_service,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
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
  signin::IdentityManager* identity_manager() {
    return identity_manager_.get();
  }
  CollaborationService* collaboration_service() {
    return collaboration_service_.get();
  }
  Flow& flow() { return flow_; }

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

  // Cancels and exits the current flow.
  void Cancel();

  // Helper functions used in tests.
  void SetStateForTesting(StateId state);
  StateId GetStateForTesting();

 private:
  static constexpr std::array<std::pair<StateId, StateId>, 41>
      kValidTransitions = {{
          // kPending transitions to:
          //
          //   kAuthenticating: After all initialization steps complete
          //   successfully and authentication status is not valid.
          //   kWaitingForPolicyUpdate: Current account info are not ready.
          //   kCheckingFlowRequirements: After all initialization steps
          //   complete successfully and authentication status is valid.
          //   kError: An error occurred during initialization.
          {StateId::kPending, StateId::kAuthenticating},
          {StateId::kPending, StateId::kWaitingForPolicyUpdate},
          {StateId::kPending, StateId::kWaitingForServicesToInitialize},
          {StateId::kPending, StateId::kError},

          // kWaitingForPolicyUpdate transitions to:
          //
          //   kAuthenticating: Current account is not managed and sync consent
          //   is needed.
          //   kCheckingFlowRequirements: Current account is not managed.
          //   kError: Current account is managed.
          {StateId::kWaitingForPolicyUpdate, StateId::kAuthenticating},
          {StateId::kWaitingForPolicyUpdate,
           StateId::kCheckingFlowRequirements},
          {StateId::kWaitingForPolicyUpdate, StateId::kError},

          // kAuthenticating transitions to:
          //
          //   kWaitingForPolicyUpdate: Current account info are not ready.
          //   kCheckingFlowRequirements: After all authentication steps are
          //   completed and verified.
          //   kCancel: After the user cancels the process.
          //   kError: An error occurred during authentication.
          {StateId::kAuthenticating, StateId::kWaitingForPolicyUpdate},
          {StateId::kAuthenticating, StateId::kWaitingForServicesToInitialize},
          {StateId::kAuthenticating, StateId::kCancel},
          {StateId::kAuthenticating, StateId::kError},

          // kWaitingForServicesToInitialize transition to:
          //
          //   kCheckingFlowRequirements: After all services finish
          //   initializing.
          //   kError: An error occurred while waiting for service
          //   initialization.
          {StateId::kWaitingForServicesToInitialize,
           StateId::kCheckingFlowRequirements},
          {StateId::kWaitingForServicesToInitialize, StateId::kError},

          // kCheckingFlowRequirements transition to:
          //
          //   kAddingUserToGroup: When user is not in current people group.
          //   kWaitingForSyncAndDataSharingGroup: When user is in current
          //   people group,
          //   but tab group not found in sync.
          //   kOpeningLocalTabGroup: When user is in current people group, and
          //   tab group found in sync.
          //   kShowingShareScreen: In share flow, when the tab group is not
          //   shared.
          //   kShowingManageScreen: In share flow, when the tab group is a
          //   shared tab group.
          //   kError: An error occurred while checking requirements. This could
          //   be due to version mismatch.
          {StateId::kCheckingFlowRequirements, StateId::kAddingUserToGroup},
          {StateId::kCheckingFlowRequirements,
           StateId::kWaitingForSyncAndDataSharingGroup},
          {StateId::kCheckingFlowRequirements, StateId::kOpeningLocalTabGroup},
          {StateId::kCheckingFlowRequirements, StateId::kShowingShareScreen},
          {StateId::kCheckingFlowRequirements, StateId::kShowingManageScreen},
          {StateId::kCheckingFlowRequirements, StateId::kLeavingGroup},
          {StateId::kCheckingFlowRequirements, StateId::kDeletingGroup},
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
          //   kCancel: After the promote is done successfully, cancel the flow
          //   to clean up.
          //   kError: An error occurred while opening local tab group.
          {StateId::kOpeningLocalTabGroup, StateId::kCancel},
          {StateId::kOpeningLocalTabGroup, StateId::kError},

          // kShowingShareScreen transition to:
          //
          //   kSharingTabGroupUrl: After share screen request creating a shared
          //   tab group.
          //   kCancel: After the user exit the share screen without sharing.
          //   kError: An error occurred while showing the share screen.
          {StateId::kShowingShareScreen, StateId::kMakingTabGroupShared},
          {StateId::kShowingShareScreen, StateId::kCancel},
          {StateId::kShowingShareScreen, StateId::kError},

          // kMakingTabGroupShared transition to:
          //
          //   kSharingTabGroupUrl: After shared tab group is successfully
          //   created.
          //   kError: An error occurred while creating the shared tab group.
          {StateId::kMakingTabGroupShared, StateId::kSharingTabGroupUrl},
          {StateId::kMakingTabGroupShared, StateId::kError},

          // kSharingTabGroupUrl transition to:
          //
          //   kError: An error occurred while sharing the url.
          {StateId::kSharingTabGroupUrl, StateId::kError},

          // kShowingManageScreen transition to:
          //
          //   kCleaningUpSharedTabGroup: When deletion happened on a manage
          //   screen.
          //   kError: An error occurred while showing the manage people screen.
          {StateId::kShowingManageScreen, StateId::kCleaningUpSharedTabGroup},
          {StateId::kShowingManageScreen, StateId::kError},

          // kShowingManageScreen transition to:
          //
          //   kCleaningUpSharedTabGroup: After leaving group successfully.
          //   kError: An error occurred while leaving group.
          {StateId::kLeavingGroup, StateId::kCleaningUpSharedTabGroup},
          {StateId::kLeavingGroup, StateId::kError},

          // kDeletingGroup transition to:
          //
          //   kCleaningUpSharedTabGroup: When deletion has been completed.
          //   kError: An error occurred while deleting group.
          {StateId::kDeletingGroup, StateId::kCleaningUpSharedTabGroup},
          {StateId::kDeletingGroup, StateId::kError},
      }};

  bool IsValidStateTransition(StateId from, StateId to);
  std::unique_ptr<ControllerState> CreateStateObject(StateId state);

  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<ControllerState> current_state_;

  Flow flow_;
  bool is_deleting_{false};
  const raw_ptr<CollaborationService> collaboration_service_;
  const raw_ptr<data_sharing::DataSharingService> data_sharing_service_;
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<CollaborationControllerDelegate> delegate_;
  FinishCallback finish_and_delete_;
  base::WeakPtrFactory<CollaborationController> weak_ptr_factory_{this};
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_CONTROLLER_H_
