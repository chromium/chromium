// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/collaboration/internal/metrics.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/service/sync_service.h"

namespace collaboration {

using metrics::CollaborationServiceJoinEvent;
using metrics::CollaborationServiceShareOrManageEvent;

class ControllerState;

namespace {

using ErrorInfo = CollaborationControllerDelegate::ErrorInfo;
using Outcome = CollaborationControllerDelegate::Outcome;
using ResultCallback = CollaborationControllerDelegate::ResultCallback;
using GroupDataOrFailureOutcome =
    data_sharing::DataSharingService::GroupDataOrFailureOutcome;
using StateId = CollaborationController::StateId;
using Flow = CollaborationController::Flow;

std::string GetStateIdString(StateId state) {
  switch (state) {
    case StateId::kPending:
      return "Pending";
    case StateId::kAuthenticating:
      return "Authenticating";
    case StateId::kWaitingForServicesToInitialize:
      return "WaitingForServicesToInitialize";
    case StateId::kCheckingFlowRequirements:
      return "CheckingFlowRequirements";
    case StateId::kAddingUserToGroup:
      return "AddingUserToGroup";
    case StateId::kWaitingForSyncAndDataSharingGroup:
      return "WaitingForSyncAndDataSharingGroup";
    case StateId::kOpeningLocalTabGroup:
      return "OpeningLocalTabGroup";
    case StateId::kShowingShareScreen:
      return "ShowingShareScreen";
    case StateId::kMakingTabGroupShared:
      return "MakingTabGroupShared";
    case StateId::kSharingTabGroupUrl:
      return "SharingTabGroupUrl";
    case StateId::kShowingManageScreen:
      return "ShowingManageScreen";
    case StateId::kCancel:
      return "Cancel";
    case StateId::kError:
      return "Error";
  }
}

}  // namespace

// This is base class for each state and handles the logic for the state.
// TODO(crbug.com/389953812): Consider consolidating metric recording into the
// base class. Provide a utility function to handle state specific metrics.
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
    OnProcessingFinishedWithSuccess();
  }

  // Called when an error happens during the state.
  virtual void HandleError() {
    controller->TransitionTo(StateId::kError,
                             ErrorInfo(ErrorInfo::Type::kGenericError));
  }

  virtual void HandleErrorWithType(ErrorInfo::Type type) {
    controller->TransitionTo(StateId::kError, ErrorInfo(type));
  }

  // Called when the state outcome processing is finished.
  virtual void OnProcessingFinishedWithSuccess() {}

  // Called when exiting the state.
  virtual void OnExit() {}

  const StateId id;
  const raw_ptr<CollaborationController> controller;
  base::WeakPtrFactory<ControllerState> weak_ptr_factory_{this};

 protected:
  bool IsTabGroupInSync(const data_sharing::GroupId& group_id) {
    const std::vector<tab_groups::SavedTabGroup>& all_groups =
        controller->tab_group_sync_service()->GetAllGroups();
    for (const auto& group : all_groups) {
      if (group.collaboration_id().has_value() &&
          group.collaboration_id().value() ==
              tab_groups::CollaborationId(group_id.value())) {
        return true;
      }
    }
    return false;
  }

  bool IsPeopleGroupInDataSharing(const data_sharing::GroupId& group_id) {
    return controller->collaboration_service()->GetCurrentUserRoleForGroup(
               group_id) != data_sharing::MemberRole::kUnknown;
  }
};

namespace {

class PendingState : public ControllerState {
 public:
  PendingState(StateId id,
               CollaborationController* controller,
               CollaborationController::FinishCallback exit_callback)
      : ControllerState(id, controller),
        exit_callback_(std::move(exit_callback)) {}

  void OnEnter(const ErrorInfo& error) override {
    controller->delegate()->PrepareFlowUI(
        std::move(exit_callback_),
        base::BindOnce(&PendingState::ProcessOutcome,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinishedWithSuccess() override {
    if (controller->flow().type == FlowType::kJoin) {
      // Handle URL parsing errors.
      if (!controller->flow().join_token().IsValid()) {
        RecordJoinEvent(CollaborationServiceJoinEvent::kParsingFailure);
        HandleErrorWithType(ErrorInfo::Type::kInvalidUrl);
        return;
      }
    }

    // Verify authentication status.
    ServiceStatus status =
        controller->collaboration_service()->GetServiceStatus();
    if (!status.IsAuthenticationValid()) {
      controller->TransitionTo(StateId::kAuthenticating);
      return;
    }

    controller->TransitionTo(StateId::kWaitingForServicesToInitialize);
  }

 private:
  //  Will be invalid after OnEnter() is called.
  CollaborationController::FinishCallback exit_callback_;
};

class AuthenticatingState : public ControllerState,
                            public CollaborationService::Observer {
 public:
  AuthenticatingState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    if (FlowType::kJoin == controller->flow().type) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kNotSignedIn);
    } else if (FlowType::kShareOrManage == controller->flow().type) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kNotSignedIn);
    }

    controller->delegate()->ShowAuthenticationUi(
        base::BindOnce(&AuthenticatingState::ProcessOutcome,
                       local_weak_ptr_factory_.GetWeakPtr()));
  }

  void ProcessOutcome(Outcome outcome) override {
    if (Outcome::kCancel == outcome) {
      if (FlowType::kJoin == controller->flow().type) {
        RecordJoinEvent(CollaborationServiceJoinEvent::kCanceledNotSignedIn);
      } else if (FlowType::kShareOrManage == controller->flow().type) {
        RecordShareOrManageEvent(
            CollaborationServiceShareOrManageEvent::kCanceledNotSignedIn);
      }
    }

    ControllerState::ProcessOutcome(outcome);
  }

  void OnProcessingFinishedWithSuccess() override {
    ServiceStatus status =
        controller->collaboration_service()->GetServiceStatus();
    if (!status.IsAuthenticationValid()) {
      // Set up the timeout exit task.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AuthenticatingState::HandleError,
                         weak_ptr_factory_.GetWeakPtr()),
          base::Minutes(30));
      collaboration_service_observer_.Observe(
          controller->collaboration_service());
      if (FlowType::kJoin == controller->flow().type) {
        RecordJoinEvent(
            CollaborationServiceJoinEvent::kSigninVerificationFailed);
      } else if (FlowType::kShareOrManage == controller->flow().type) {
        RecordShareOrManageEvent(
            CollaborationServiceShareOrManageEvent::kSigninVerificationFailed);
      }
      return;
    }

    if (FlowType::kJoin == controller->flow().type) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kSigninVerified);
    } else if (FlowType::kShareOrManage == controller->flow().type) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kSigninVerified);
    }
    // TODO(crbug.com/380957996): Handle signin/sync changes during a flow.
    controller->delegate()->NotifySignInAndSyncStatusChange();
    controller->TransitionTo(StateId::kWaitingForServicesToInitialize);
  }

  // CollaborationService::Observer implementation.
  void OnServiceStatusChanged(const ServiceStatusUpdate& update) override {
    if (update.new_status.IsAuthenticationValid()) {
      if (FlowType::kJoin == controller->flow().type) {
        RecordJoinEvent(
            CollaborationServiceJoinEvent::kSigninVerifiedInObserver);
      } else if (FlowType::kShareOrManage == controller->flow().type) {
        RecordShareOrManageEvent(
            CollaborationServiceShareOrManageEvent::kSigninVerifiedInObserver);
      }
      controller->delegate()->NotifySignInAndSyncStatusChange();
      controller->TransitionTo(StateId::kWaitingForServicesToInitialize);
    }
  }

 private:
  base::ScopedObservation<CollaborationService, CollaborationService::Observer>
      collaboration_service_observer_{this};

  base::WeakPtrFactory<AuthenticatingState> local_weak_ptr_factory_{this};
};

class WaitingForServicesToInitialize
    : public ControllerState,
      public tab_groups::TabGroupSyncService::Observer,
      public data_sharing::DataSharingService::Observer {
 public:
  WaitingForServicesToInitialize(StateId id,
                                 CollaborationController* controller)
      : ControllerState(id, controller) {}

  // ControllerState implementation.
  void OnEnter(const ErrorInfo& error) override {
    // Timeout waiting.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WaitingForServicesToInitialize::HandleError,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(5));
    // TODO(crbug.com/392791204): Wait for tab group sync to be ready.
    is_data_sharing_ready_ =
        controller->data_sharing_service()->IsGroupDataModelLoaded();
    if (!is_data_sharing_ready_) {
      data_sharing_observer_.Observe(controller->data_sharing_service());
    } else {
      if (FlowType::kJoin == controller->flow().type) {
        RecordJoinEvent(
            CollaborationServiceJoinEvent::kDataSharingReadyWhenStarted);
      } else if (FlowType::kShareOrManage == controller->flow().type) {
        RecordShareOrManageEvent(CollaborationServiceShareOrManageEvent::
                                     kDataSharingReadyWhenStarted);
      }
    }
    tab_group_sync_observer_.Observe(controller->tab_group_sync_service());
  }

  void OnProcessingFinishedWithSuccess() override {
    controller->TransitionTo(StateId::kCheckingFlowRequirements);
  }

  // TabGroupSyncService::Observer implementation.
  void OnInitialized() override {
    if (FlowType::kJoin == controller->flow().type) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kTabGroupServiceReady);
    } else if (FlowType::kShareOrManage == controller->flow().type) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kTabGroupServiceReady);
    }
    is_tab_group_sync_ready_ = true;
    MaybeProceed();
  }

  // DataSharingService::Observer implementation.
  void OnGroupDataModelLoaded() override {
    if (FlowType::kJoin == controller->flow().type) {
      RecordJoinEvent(
          CollaborationServiceJoinEvent::kDataSharingServiceReadyObserved);
    } else if (FlowType::kShareOrManage == controller->flow().type) {
      RecordShareOrManageEvent(CollaborationServiceShareOrManageEvent::
                                   kDataSharingServiceReadyObserved);
    }

    is_data_sharing_ready_ = true;
    MaybeProceed();
  }

 private:
  void MaybeProceed() {
    if (is_tab_group_sync_ready_ && is_data_sharing_ready_) {
      if (FlowType::kJoin == controller->flow().type) {
        RecordJoinEvent(
            CollaborationServiceJoinEvent::kAllServicesReadyForFlow);
      } else if (FlowType::kShareOrManage == controller->flow().type) {
        RecordShareOrManageEvent(
            CollaborationServiceShareOrManageEvent::kAllServicesReadyForFlow);
      }
      OnProcessingFinishedWithSuccess();
    }
  }

  bool is_tab_group_sync_ready_{false};
  bool is_data_sharing_ready_{false};
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_observer_{this};
  base::ScopedObservation<data_sharing::DataSharingService,
                          data_sharing::DataSharingService::Observer>
      data_sharing_observer_{this};
};

class CheckingFlowRequirementsState : public ControllerState {
 public:
  CheckingFlowRequirementsState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    switch (controller->flow().type) {
      case FlowType::kJoin: {
        RecordJoinEvent(CollaborationServiceJoinEvent::kFlowRequirementsMet);

        const data_sharing::GroupId group_id =
            controller->flow().join_token().group_id;
        // Check if user is already part of the group.
        if (IsPeopleGroupInDataSharing(group_id)) {
          if (IsTabGroupInSync(group_id)) {
            RecordJoinEvent(
                CollaborationServiceJoinEvent::kOpenedExistingGroup);
            controller->TransitionTo(StateId::kOpeningLocalTabGroup);
            return;
          }

          RecordJoinEvent(CollaborationServiceJoinEvent::
                              kFoundCollaborationWithoutTabGroup);
          controller->TransitionTo(StateId::kWaitingForSyncAndDataSharingGroup);
          return;
        }

        // If user is not part of the group, do a readgroup to ensure version
        // match.
        // TODO(haileywang): Do the version check in the preview data and do the
        // network requests in parallel instead of one by one.
        controller->data_sharing_service()->ReadNewGroup(
            controller->flow().join_token(),
            base::BindOnce(&CheckingFlowRequirementsState::
                               ProcessGroupDataOrFailureOutcome,
                           local_weak_ptr_factory_.GetWeakPtr()));
        break;
      }
      case FlowType::kShareOrManage:
        RecordShareOrManageEvent(
            CollaborationServiceShareOrManageEvent::kFlowRequirementsMet);

        std::optional<tab_groups::SavedTabGroup> sync_group =
            controller->tab_group_sync_service()->GetGroup(
                controller->flow().either_id());
        if (!sync_group.has_value()) {
          RecordShareOrManageEvent(
              CollaborationServiceShareOrManageEvent::kSyncedTabGroupNotFound);
          HandleError();
          return;
        }

        if (sync_group.value().is_shared_tab_group()) {
          controller->TransitionTo(StateId::kShowingManageScreen);
          return;
        }

        controller->TransitionTo(StateId::kShowingShareScreen);
        break;
    }
  }

  void OnProcessingFinishedWithSuccess() override {
    CHECK_EQ(controller->flow().type, FlowType::kJoin);
    controller->TransitionTo(StateId::kAddingUserToGroup);
  }

 private:
  // Called to process the outcome of data sharing read event.
  void ProcessGroupDataOrFailureOutcome(
      const GroupDataOrFailureOutcome& group_outcome) {
    // TODO(crbug.com/373403973): add version check.
    if (!group_outcome.has_value()) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kReadNewGroupFailed);
      HandleErrorWithType(ErrorInfo::Type::kInvalidUrl);
    }

    RecordJoinEvent(CollaborationServiceJoinEvent::kReadNewGroupSuccess);
    OnProcessingFinishedWithSuccess();
  }

  base::WeakPtrFactory<CheckingFlowRequirementsState> local_weak_ptr_factory_{
      this};
};

class AddingUserToGroupState : public ControllerState {
 public:
  AddingUserToGroupState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    controller->data_sharing_service()->GetSharedEntitiesPreview(
        controller->flow().join_token(),
        base::BindOnce(
            &AddingUserToGroupState::ProcessSharedDataPreviewOrFailureOutcome,
            local_weak_ptr_factory_.GetWeakPtr()));
  }

  void ProcessOutcome(Outcome outcome) override {
    if (Outcome::kCancel == outcome) {
      CHECK_EQ(controller->flow().type, FlowType::kJoin)
          << "Only the join flow can transition into the AddingUserToGroup "
             "state.";
      RecordJoinEvent(CollaborationServiceJoinEvent::kCanceled);
    }
    RecordJoinEvent(CollaborationServiceJoinEvent::kAddedUserToGroup);

    ControllerState::ProcessOutcome(outcome);
  }

  void OnProcessingFinishedWithSuccess() override {
    RecordJoinEvent(CollaborationServiceJoinEvent::kAccepted);

    const data_sharing::GroupId group_id =
        controller->flow().join_token().group_id;
    if (IsTabGroupInSync(group_id) && IsPeopleGroupInDataSharing(group_id)) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kGroupExistsWhenJoined);
      controller->TransitionTo(StateId::kOpeningLocalTabGroup);
      return;
    }

    RecordJoinEvent(CollaborationServiceJoinEvent::kOpenedNewGroup);
    controller->TransitionTo(StateId::kWaitingForSyncAndDataSharingGroup);
  }

 private:
  void ProcessSharedDataPreviewOrFailureOutcome(
      const data_sharing::DataSharingService::SharedDataPreviewOrFailureOutcome&
          preview_outcome) {
    if (!preview_outcome.has_value() &&
        preview_outcome.error() == data_sharing::DataSharingService::
                                       DataPreviewActionFailure::kGroupFull) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kPreviewGroupFullError);
      HandleError();
      return;
    }

    if (!preview_outcome.has_value() ||
        !preview_outcome.value().shared_tab_group_preview.has_value()) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kPreviewFailure);
      HandleErrorWithType(ErrorInfo::Type::kInvalidUrl);
      return;
    }

    RecordJoinEvent(CollaborationServiceJoinEvent::kPreviewSuccess);
    controller->delegate()->ShowJoinDialog(
        controller->flow().join_token(), preview_outcome.value(),
        base::BindOnce(&AddingUserToGroupState::ProcessOutcome,
                       local_weak_ptr_factory_.GetWeakPtr()));
  }

  base::WeakPtrFactory<AddingUserToGroupState> local_weak_ptr_factory_{this};
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
  void OnProcessingFinishedWithSuccess() override {
    controller->TransitionTo(StateId::kOpeningLocalTabGroup);
  }

  void OnEnter(const ErrorInfo& error) override {
    const data_sharing::GroupId group_id =
        controller->flow().join_token().group_id;
    bool tab_group_exists = IsTabGroupInSync(group_id);
    bool people_group_exists = IsPeopleGroupInDataSharing(group_id);
    CHECK(!tab_group_exists || !people_group_exists);
    // Force update sync.
    if (!tab_group_exists) {
      controller->sync_service()->TriggerRefresh(
          {syncer::SHARED_TAB_GROUP_DATA});
    }
    // Force update data sharing service.
    if (!IsPeopleGroupInDataSharing(group_id)) {
      controller->data_sharing_service()->ReadGroupDeprecated(
          group_id, base::DoNothing());
    }
  }

  // TabGroupSyncService::Observer implementation.
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override {
    const data_sharing::GroupId group_id =
        controller->flow().join_token().group_id;
    if (group.is_shared_tab_group() &&
        group.collaboration_id().value() ==
            tab_groups::CollaborationId(group_id.value()) &&
        IsPeopleGroupInDataSharing(group_id)) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kTabGroupFetched);
      ProcessOutcome(Outcome::kSuccess);
    }
  }

  // DataSharingService::Observer implementation.
  void OnGroupAdded(const data_sharing::GroupData& group_data,
                    const base::Time& event_time) override {
    const data_sharing::GroupId group_id =
        controller->flow().join_token().group_id;
    if (group_data.group_token.group_id.value() == group_id.value() &&
        IsTabGroupInSync(group_id)) {
      RecordJoinEvent(CollaborationServiceJoinEvent::kPeopleGroupFetched);
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
    // Only the join flow has a valid `group_id`.
    CHECK_EQ(controller->flow().type, FlowType::kJoin);

    RecordJoinEvent(CollaborationServiceJoinEvent::kPromoteTabGroup);
    controller->delegate()->PromoteTabGroup(
        controller->flow().join_token().group_id,
        base::BindOnce(&OpeningLocalTabGroupState::ProcessOutcome,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinishedWithSuccess() override { controller->Exit(); }
};

class ShowingShareScreen : public ControllerState {
 public:
  ShowingShareScreen(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    CHECK_EQ(controller->flow().type, FlowType::kShareOrManage);
    RecordShareOrManageEvent(
        CollaborationServiceShareOrManageEvent::kShareDialogShown);

    controller->delegate()->ShowShareDialog(
        controller->flow().either_id(),
        base::BindOnce(&ShowingShareScreen::OnCollaborationIdCreated,
                       local_weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinishedWithSuccess() override {
    controller->TransitionTo(StateId::kMakingTabGroupShared);
  }

 private:
  void OnCollaborationIdCreated(
      Outcome outcome,
      std::optional<data_sharing::GroupToken> group_token) {
    // TODO(haileywang): The following code imitate old behavior to not break
    // tests. Follow new behavior once all platform adjust to new share
    // behavior.
    if (outcome == Outcome::kFailure) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kCollaborationIdMissing);
      HandleError();
      return;
    }

    if (!group_token.has_value() || !group_token.value().IsValid()) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kCollaborationIdInvalid);
      controller->Exit();
      return;
    }

    controller->flow().set_share_token(group_token.value());
    ProcessOutcome(outcome);
  }

  base::WeakPtrFactory<ShowingShareScreen> local_weak_ptr_factory_{this};
};

class MakingTabGroupShared : public ControllerState {
 public:
  MakingTabGroupShared(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    CHECK_EQ(controller->flow().type, FlowType::kShareOrManage);

    std::optional<tab_groups::SavedTabGroup> group =
        controller->tab_group_sync_service()->GetGroup(
            controller->flow().either_id());
    if (!group.has_value()) {
      RecordShareOrManageEvent(CollaborationServiceShareOrManageEvent::
                                   kTabGroupMissingBeforeMigration);
      HandleError();
      return;
    }

    const std::optional<tab_groups::LocalTabGroupID>& local_group_id =
        group.value().local_group_id();
    CHECK(local_group_id.has_value());

    const data_sharing::GroupToken& group_token =
        controller->flow().share_token();

    RecordShareOrManageEvent(
        CollaborationServiceShareOrManageEvent::kTabGroupShared);

    controller->tab_group_sync_service()->MakeTabGroupShared(
        local_group_id.value(), group_token.group_id.value(),
        base::BindOnce(&MakingTabGroupShared::ProcessTabGroupSharingResult,
                       local_weak_ptr_factory_.GetWeakPtr()));

    controller->data_sharing_service()->ReadGroupDeprecated(
        group_token.group_id,
        base::BindOnce(&MakingTabGroupShared::ProcessGroupDataOrFailureOutcome,
                       local_weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinishedWithSuccess() override {
    controller->TransitionTo(StateId::kSharingTabGroupUrl);
  }

 private:
  void ProcessTabGroupSharingResult(
      tab_groups::TabGroupSyncService::TabGroupSharingResult result) {
    if (result !=
        tab_groups::TabGroupSyncService::TabGroupSharingResult::kSuccess) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kMigrationFailure);
      HandleError();
      return;
    }

    is_make_group_shared_complete_ = true;
    MaybeProceedFlow();
  }

  void ProcessGroupDataOrFailureOutcome(
      const GroupDataOrFailureOutcome& group_outcome) {
    if (!group_outcome.has_value()) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kReadGroupFailed);
      HandleError();
      return;
    }

    is_read_group_complete_ = true;
    MaybeProceedFlow();
  }

  void MaybeProceedFlow() {
    if (is_make_group_shared_complete_ && is_read_group_complete_) {
      OnProcessingFinishedWithSuccess();
    }
  }

  bool is_make_group_shared_complete_{false};
  bool is_read_group_complete_{false};
  base::WeakPtrFactory<MakingTabGroupShared> local_weak_ptr_factory_{this};
};

class SharingTabGroupUrl : public ControllerState {
 public:
  SharingTabGroupUrl(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    CHECK_EQ(controller->flow().type, FlowType::kShareOrManage);

    const data_sharing::GroupToken& group_token =
        controller->flow().share_token();
    data_sharing::GroupData group_data = data_sharing::GroupData();
    group_data.group_token = group_token;

    auto url =
        controller->data_sharing_service()->GetDataSharingUrl(group_data);
    if (!url) {
      RecordShareOrManageEvent(
          CollaborationServiceShareOrManageEvent::kUrlCreationFailed);
      HandleError();
      return;
    }

    RecordShareOrManageEvent(
        CollaborationServiceShareOrManageEvent::kUrlReadyToShare);
    controller->delegate()->OnUrlReadyToShare(
        group_token.group_id, *url,
        base::BindOnce(&SharingTabGroupUrl::ProcessOutcome,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinishedWithSuccess() override { controller->Exit(); }
};

class ShowingManageScreen : public ControllerState {
 public:
  ShowingManageScreen(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    CHECK_EQ(controller->flow().type, FlowType::kShareOrManage);
    RecordShareOrManageEvent(
        CollaborationServiceShareOrManageEvent::kManageDialogShown);

    controller->delegate()->ShowManageDialog(
        controller->flow().either_id(),
        base::BindOnce(&ShowingManageScreen::ProcessOutcome,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnProcessingFinishedWithSuccess() override { controller->Exit(); }
};

class ErrorState : public ControllerState {
 public:
  ErrorState(StateId id, CollaborationController* controller)
      : ControllerState(id, controller) {}

  void OnEnter(const ErrorInfo& error) override {
    controller->delegate()->ShowError(
        error, base::BindOnce(&ErrorState::ProcessOutcome,
                              local_weak_ptr_factory_.GetWeakPtr()));
  }

  void ProcessOutcome(Outcome outcome) override { controller->Exit(); }

 private:
  base::WeakPtrFactory<ErrorState> local_weak_ptr_factory_{this};
};

}  // namespace

CollaborationController::Flow::Flow(FlowType type,
                                    const data_sharing::GroupToken& token)
    : type(type), join_token_(token) {
  DCHECK(type == FlowType::kJoin);
}

CollaborationController::Flow::Flow(FlowType type,
                                    const tab_groups::EitherGroupID& either_id)
    : type(type), either_id_(either_id) {
  DCHECK(type == FlowType::kShareOrManage);
}

CollaborationController::Flow::Flow(const Flow&) = default;

CollaborationController::Flow::~Flow() = default;

CollaborationController::CollaborationController(
    const Flow& flow,
    CollaborationService* collaboration_service,
    data_sharing::DataSharingService* data_sharing_service,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    syncer::SyncService* sync_service,
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    FinishCallback finish_and_delete)
    : flow_(flow),
      collaboration_service_(collaboration_service),
      data_sharing_service_(data_sharing_service),
      tab_group_sync_service_(tab_group_sync_service),
      sync_service_(sync_service),
      delegate_(std::move(delegate)),
      finish_and_delete_(std::move(finish_and_delete)) {
  current_state_ = std::make_unique<PendingState>(
      StateId::kPending, this,
      base::BindOnce(&CollaborationController::Exit,
                     weak_ptr_factory_.GetWeakPtr()));
  current_state_->OnEnter(ErrorInfo(ErrorInfo::Type::kUnknown));
}

CollaborationController::~CollaborationController() = default;

void CollaborationController::TransitionTo(StateId state,
                                           const ErrorInfo& error) {
  VLOG(2) << "Transition from " << GetStateIdString(current_state_->id)
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
  if (is_deleting_) {
    // Exit can be triggered by multiple code paths, the delegate itself, or
    // from the service. It is safe to ignore multiple requets since we are just
    // waiting for finish_and_delete_ to run in the next post task.
    return;
  }

  current_state_->OnExit();
  delegate_->OnFlowFinished();
  is_deleting_ = true;
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
      return std::make_unique<PendingState>(state, this, base::DoNothing());
    case StateId::kAuthenticating:
      return std::make_unique<AuthenticatingState>(state, this);
    case StateId::kWaitingForServicesToInitialize:
      return std::make_unique<WaitingForServicesToInitialize>(state, this);
    case StateId::kCheckingFlowRequirements:
      return std::make_unique<CheckingFlowRequirementsState>(state, this);
    case StateId::kAddingUserToGroup:
      return std::make_unique<AddingUserToGroupState>(state, this);
    case StateId::kWaitingForSyncAndDataSharingGroup:
      return std::make_unique<WaitingForSyncAndDataSharingGroup>(state, this);
    case StateId::kOpeningLocalTabGroup:
      return std::make_unique<OpeningLocalTabGroupState>(state, this);
    case StateId::kShowingShareScreen:
      return std::make_unique<ShowingShareScreen>(state, this);
    case StateId::kMakingTabGroupShared:
      return std::make_unique<MakingTabGroupShared>(state, this);
    case StateId::kSharingTabGroupUrl:
      return std::make_unique<SharingTabGroupUrl>(state, this);
    case StateId::kShowingManageScreen:
      return std::make_unique<ShowingManageScreen>(state, this);
    case StateId::kCancel:
      return std::make_unique<ControllerState>(state, this);
    case StateId::kError:
      return std::make_unique<ErrorState>(state, this);
  }
}

}  // namespace collaboration
