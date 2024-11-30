// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

#include "base/logging.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/service/sync_service.h"

namespace collaboration {

class ControllerState;

namespace {

using ErrorInfo = CollaborationControllerDelegate::ErrorInfo;
using Outcome = CollaborationControllerDelegate::Outcome;
using ResultCallback = CollaborationControllerDelegate::ResultCallback;
using GroupDataOrFailureOutcome =
    data_sharing::DataSharingService::GroupDataOrFailureOutcome;
using StateId = CollaborationController::StateId;

std::string GetStateIdString(StateId state) {
  switch (state) {
    case StateId::kPending:
      return "Pending";
    case StateId::kAuthenticating:
      return "Authenticating";
    case StateId::kCheckingFlowRequirements:
      return "CheckingFlowRequirements";
    case StateId::kAddingUserToGroup:
      return "AddingUserToGroup";
    case StateId::kWaitingForSyncAndDataSharingGroup:
      return "WaitingForSyncAndDataSharingGroup";
    case StateId::kOpeningLocalTabGroup:
      return "OpeningLocalTabGroup";
    case StateId::kCancel:
      return "Cancel";
    case StateId::kError:
      return "Error";
  }
}
}  // namespace

// This is base class for each state and handles the logic for the state.
class ControllerState {
 public:
  ControllerState(StateId id, CollaborationController* controller)
      : id(id), controller(controller) {}
  virtual ~ControllerState() = default;

  // Called when entering the state.
  virtual void OnEnter(const ErrorInfo& error) {}

  // Called to process the outcome of an external event.
  virtual void ProcessOutcome(Outcome outcome) {
    if (outcome == Outcome::kFailure) {
      HandleError();
      return;
    } else if (outcome == Outcome::kCancel) {
      controller->Exit();
      return;
    }
    OnProcessingFinished();
  }

  // Called when an error happens during the state.
  virtual void HandleError() {
    controller->TransitionTo(StateId::kError,
                             ErrorInfo(ErrorInfo::Type::kGenericError));
  }

  // Called when the state outcome processing is finished.
  virtual void OnProcessingFinished() {}

  // Called when exiting the state.
  virtual void OnExit() {}

  const StateId id;
  const raw_ptr<CollaborationController> controller;
  base::WeakPtrFactory<ControllerState> weak_ptr_factory_{this};

 protected:
  bool IsTabGroupInSync() {
    const std::vector<tab_groups::SavedTabGroup>& all_groups =
        controller->tab_group_sync_service()->GetAllGroups();
    for (const auto& group : all_groups) {
      if (group.collaboration_id().has_value() &&
          group.collaboration_id().value() ==
              tab_groups::CollaborationId(
                  controller->token().group_id.value())) {
        return true;
      }
    }
    return false;
  }

  bool IsPeopleGroupInDataSharing() {
    return controller->collaboration_service()->GetCurrentUserRoleForGroup(
               controller->token().group_id) !=
           data_sharing::MemberRole::kUnknown;
  }
};

namespace {

class PendingState : public ControllerState {
 public:
  PendingState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    controller->delegate()->PrepareFlowUI(base::BindOnce(
        &PendingState::ProcessOutcome, weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinished() override {
    // Handle URL parsing errors.
    if (!controller->token().IsValid()) {
      HandleError();
      return;
    }

    // Verify authentication status.
    ServiceStatus status =
        controller->collaboration_service()->GetServiceStatus();
    if (!status.IsAuthenticationValid()) {
      controller->TransitionTo(StateId::kAuthenticating);
      return;
    }

    controller->TransitionTo(StateId::kCheckingFlowRequirements);
  }
};

class AuthenticatingState : public ControllerState {
 public:
  AuthenticatingState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    controller->delegate()->ShowAuthenticationUi(base::BindOnce(
        &AuthenticatingState::ProcessOutcome, weak_ptr_factory_.GetWeakPtr()));
  }
  void OnProcessingFinished() override {
    ServiceStatus status =
        controller->collaboration_service()->GetServiceStatus();
    if (!status.IsAuthenticationValid()) {
      HandleError();
      return;
    }

    // TODO(crbug.com/380957996): Handle signin/sync changes during a flow.
    controller->delegate()->NotifySignInAndSyncStatusChange();
    controller->TransitionTo(StateId::kCheckingFlowRequirements);
  }
};

class CheckingFlowRequirementsState : public ControllerState {
 public:
  CheckingFlowRequirementsState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    switch (controller->flow()) {
      case CollaborationController::Flow::kJoin: {
        // Check if user is already part of the group.
        if (IsPeopleGroupInDataSharing()) {
          if (IsTabGroupInSync()) {
            controller->TransitionTo(StateId::kOpeningLocalTabGroup);
            return;
          }

          controller->TransitionTo(StateId::kWaitingForSyncAndDataSharingGroup);
          return;
        }

        // If user is not part of the group, do a readgroup to ensure version
        // match.
        controller->data_sharing_service()->ReadNewGroup(
            controller->token(),
            base::BindOnce(&CheckingFlowRequirementsState::
                               ProcessGroupDataOrFailureOutcome,
                           local_weak_ptr_factory_.GetWeakPtr()));
        break;
      }
      case CollaborationController::Flow::kShare:
        // TODO(crbug.com/373403973): Add share flow.
        break;
    }
  }

  // Called to process the outcome of data sharing read event.
  void ProcessGroupDataOrFailureOutcome(
      const GroupDataOrFailureOutcome& group_outcome) {
    // TODO(crbug.com/373403973): add version check.
    if (!group_outcome.has_value()) {
      HandleError();
    }

    OnProcessingFinished();
  }

  void OnProcessingFinished() override {
    controller->TransitionTo(StateId::kAddingUserToGroup);
  }

  base::WeakPtrFactory<CheckingFlowRequirementsState> local_weak_ptr_factory_{
      this};
};

class AddingUserToGroupState : public ControllerState {
 public:
  AddingUserToGroupState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    // TODO(crbug.com/380113830): Add preview data here.
    data_sharing::SharedDataPreview preview_data;
    controller->delegate()->ShowJoinDialog(
        preview_data, base::BindOnce(&AddingUserToGroupState::ProcessOutcome,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinished() override {
    if (IsTabGroupInSync() && IsPeopleGroupInDataSharing()) {
      controller->TransitionTo(StateId::kOpeningLocalTabGroup);
      return;
    }
    controller->TransitionTo(StateId::kWaitingForSyncAndDataSharingGroup);
  }
};

class WaitingForSyncAndDataSharingGroup
    : public ControllerState,
      public tab_groups::TabGroupSyncService::Observer,
      public data_sharing::DataSharingService::Observer {
 public:
  WaitingForSyncAndDataSharingGroup(StateId id,
                                    CollaborationController* controller)
      : ControllerState(id, controller) {
    // TODO(crbug.com/373403973): Add timeout waiting for sync and data sharing
    // service.
    tab_group_sync_observer_.Observe(controller->tab_group_sync_service());
    data_sharing_observer_.Observe(controller->data_sharing_service());
  }

  // ControllerState implementation.
  void OnProcessingFinished() override {
    controller->TransitionTo(StateId::kOpeningLocalTabGroup);
  }

  void OnEnter(const ErrorInfo& error) override {
    // Force update sync.
    controller->sync_service()->TriggerRefresh({syncer::SHARED_TAB_GROUP_DATA});
  }

  // TabGroupSyncService::Observer implementation.
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override {
    if (group.is_shared_tab_group() &&
        group.collaboration_id().value() ==
            tab_groups::CollaborationId(controller->token().group_id.value()) &&
        IsPeopleGroupInDataSharing()) {
      ProcessOutcome(Outcome::kSuccess);
    }
  }

  // DataSharingService::Observer implementation.
  void OnGroupAdded(const data_sharing::GroupData& group_data,
                    const base::Time& event_time) override {
    if (group_data.group_token.group_id.value() ==
            controller->token().group_id.value() &&
        IsTabGroupInSync()) {
      ProcessOutcome(Outcome::kSuccess);
    }
  }

 private:
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_observer_{this};
  base::ScopedObservation<data_sharing::DataSharingService,
                          data_sharing::DataSharingService::Observer>
      data_sharing_observer_{this};
};

class OpeningLocalTabGroupState : public ControllerState {
 public:
  OpeningLocalTabGroupState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    controller->delegate()->PromoteTabGroup(
        base::BindOnce(&OpeningLocalTabGroupState::ProcessOutcome,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinished() override { controller->Exit(); }
};

class ErrorState : public ControllerState {
 public:
  ErrorState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    DCHECK(error.type != ErrorInfo::Type::kUnknown);
    controller->delegate()->ShowError(
        base::BindOnce(&ErrorState::ProcessOutcome,
                       local_weak_ptr_factory_.GetWeakPtr()),
        error);
  }

  void ProcessOutcome(Outcome outcome) override { controller->Exit(); }

  base::WeakPtrFactory<ErrorState> local_weak_ptr_factory_{this};
};

}  // namespace

CollaborationController::CollaborationController(
    const Flow& flow,
    const data_sharing::GroupToken& token,
    CollaborationService* collaboration_service,
    data_sharing::DataSharingService* data_sharing_service,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    syncer::SyncService* sync_service,
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    FinishCallback finish_and_delete)
    : flow_(flow),
      token_(token),
      collaboration_service_(collaboration_service),
      data_sharing_service_(data_sharing_service),
      tab_group_sync_service_(tab_group_sync_service),
      sync_service_(sync_service),
      delegate_(std::move(delegate)),
      finish_and_delete_(std::move(finish_and_delete)) {
  current_state_ = std::make_unique<PendingState>(StateId::kPending, this);
  current_state_->OnEnter(ErrorInfo(ErrorInfo::Type::kUnknown));
}

CollaborationController::~CollaborationController() = default;

void CollaborationController::TransitionTo(StateId state,
                                           const ErrorInfo& error) {
  DVLOG(2) << "Transition from " << GetStateIdString(current_state_->id)
           << " to " << GetStateIdString(state);
  DCHECK(IsValidStateTransition(current_state_->id, state));
  current_state_->OnExit();
  current_state_ = CreateStateObject(state);
  current_state_->OnEnter(error);
}

void CollaborationController::PromoteCurrentSession() {
  delegate_->PromoteCurrentScreen();
}

void CollaborationController::Exit() {
  current_state_->OnExit();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(finish_and_delete_)));
}

void CollaborationController::SetStateForTesting(StateId state) {
  current_state_ = CreateStateObject(state);
  current_state_->OnEnter(ErrorInfo(ErrorInfo::Type::kUnknown));
}

CollaborationController::StateId CollaborationController::GetStateForTesting() {
  return current_state_->id;
}

bool CollaborationController::IsValidStateTransition(StateId from, StateId to) {
  return std::find(kValidTransitions.begin(), kValidTransitions.end(),
                   std::make_pair(from, to)) != std::end(kValidTransitions);
}

std::unique_ptr<ControllerState> CollaborationController::CreateStateObject(
    StateId state) {
  switch (state) {
    case StateId::kPending:
      return std::make_unique<PendingState>(state, this);
    case StateId::kAuthenticating:
      return std::make_unique<AuthenticatingState>(state, this);
    case StateId::kCheckingFlowRequirements:
      return std::make_unique<CheckingFlowRequirementsState>(state, this);
    case StateId::kAddingUserToGroup:
      return std::make_unique<AddingUserToGroupState>(state, this);
    case StateId::kWaitingForSyncAndDataSharingGroup:
      return std::make_unique<WaitingForSyncAndDataSharingGroup>(state, this);
    case StateId::kOpeningLocalTabGroup:
      return std::make_unique<OpeningLocalTabGroupState>(state, this);
    case StateId::kCancel:
      return std::make_unique<ControllerState>(state, this);
    case StateId::kError:
      return std::make_unique<ErrorState>(state, this);
  }
}

}  // namespace collaboration
