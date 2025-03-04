// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/metrics.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/collaboration/test_support/mock_collaboration_controller_delegate.h"
#include "components/collaboration/test_support/mock_collaboration_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration {

namespace {

const char kGroupId[] = "/?-group_id";
const char kAccessToken[] = "/?-access_token";

using StateId = CollaborationController::StateId;
using Outcome = CollaborationControllerDelegate::Outcome;
using ErrorInfo = CollaborationControllerDelegate::ErrorInfo;
using Flow = CollaborationController::Flow;
using base::OnceClosure;
using base::RunLoop;
using base::test::IsNotNullCallback;
using base::test::RunOnceCallback;
using data_sharing::GroupData;
using data_sharing::GroupToken;
using tab_groups::SavedTabGroup;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

}  // namespace

class CollaborationControllerTest : public testing::Test {
 public:
  CollaborationControllerTest() = default;
  ~CollaborationControllerTest() override = default;

  void SetUp() override {
    collaboration_service_ = std::make_unique<MockCollaborationService>();
    data_sharing_service_ =
        std::make_unique<data_sharing::MockDataSharingService>();
    tab_group_sync_service_ =
        std::make_unique<tab_groups::MockTabGroupSyncService>();
    sync_service_ = std::make_unique<syncer::MockSyncService>();
  }

  void TearDown() override {}

  void InitializeController(OnceClosure run_on_flow_exit, const Flow& flow) {
    std::unique_ptr<MockCollaborationControllerDelegate> delegate =
        std::make_unique<MockCollaborationControllerDelegate>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_,
                PrepareFlowUI(IsNotNullCallback(), IsNotNullCallback()))
        .WillOnce(MoveArg<1>(&prepare_ui_callback_));
    controller_ = std::make_unique<CollaborationController>(
        flow, collaboration_service_.get(), data_sharing_service_.get(),
        tab_group_sync_service_.get(), sync_service_.get(), std::move(delegate),
        base::BindOnce(&CollaborationControllerTest::FinishFlow,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(run_on_flow_exit)));
  }

  void InitializeJoinController(OnceClosure run_on_flow_exit) {
    InitializeController(
        std::move(run_on_flow_exit),
        Flow(FlowType::kJoin,
             GroupToken(data_sharing::GroupId(kGroupId), kAccessToken)));
  }

  void FinishFlow(OnceClosure run_on_flow_exit) {
    controller_.reset();
    std::move(run_on_flow_exit).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::OnceCallback<void(Outcome)> prepare_ui_callback_;
  MockCollaborationControllerDelegate* delegate_;
  std::unique_ptr<MockCollaborationService> collaboration_service_;
  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
  std::unique_ptr<tab_groups::MockTabGroupSyncService> tab_group_sync_service_;
  std::unique_ptr<syncer::MockSyncService> sync_service_;
  std::unique_ptr<CollaborationController> controller_;
  base::WeakPtrFactory<CollaborationControllerTest> weak_ptr_factory_{this};
};

TEST_F(CollaborationControllerTest, FullJoinFlowAllStates) {
  base::HistogramTester histogram_tester;

  RunLoop run_loop;

  // Start Join flow.
  InitializeJoinController(run_loop.QuitClosure());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate user is signed in, but not syncing.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // The user should be shown authentication screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));

  // 2. Pending -> Authenticating state.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAuthenticating);

  // Simulate user successfully completes authentication.
  status.sync_status = SyncStatus::kSyncEnabled;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));
  ASSERT_TRUE(status.IsAuthenticationValid());
  EXPECT_CALL(*delegate_, NotifySignInAndSyncStatusChange());

  // Simulate services initialization.
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
      .WillOnce(Return(true));
  tab_groups::TabGroupSyncService::Observer* sync_observer;
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&sync_observer));

  // 3. Authenticating -> WaitingForServicesToInitialize state.
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForServicesToInitialize);

  // Simulate that the user is not already in the tab group.
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(sync_observer));
  data_sharing::GroupId group_id(kGroupId);
  const GroupToken& token = GroupToken(group_id, kAccessToken);
  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      group_data_callback;
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(group_id))
      .WillRepeatedly(Return(data_sharing::MemberRole::kUnknown));
  EXPECT_CALL(*data_sharing_service_, ReadNewGroup(token, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&group_data_callback));

  // 4. WaitingForServicesToInitialize -> CheckingFlowRequirementsState state.
  sync_observer->OnInitialized();
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kCheckingFlowRequirements);

  // The user should be shown invitation screen for joining a collaboration
  // group.
  GroupData group_data =
      GroupData(group_id, /*display_name=*/"",
                /*members=*/{}, /*former_members=*/{}, kAccessToken);
  base::OnceCallback<void(Outcome)> join_ui_callback;
  base::OnceCallback<void(const data_sharing::DataSharingService::
                              SharedDataPreviewOrFailureOutcome&)>
      preview_callback;
  EXPECT_CALL(*data_sharing_service_,
              GetSharedEntitiesPreview(token, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&preview_callback));
  EXPECT_CALL(*delegate_, ShowJoinDialog(_, _, IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&join_ui_callback));

  // 5. CheckingFlowRequirements -> AddingUserToGroup state.
  std::move(group_data_callback).Run(group_data);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAddingUserToGroup);
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = data_sharing::SharedTabGroupPreview();
  std::move(preview_callback).Run(preview);

  // Simulate the user accepts the join invitation. Wait for tab group to be
  // added in sync.
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetCollaborationId(
      tab_groups::CollaborationId(std::string(kGroupId)));
  std::vector<SavedTabGroup> all_tab_groups;
  EXPECT_CALL(*tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_tab_groups));

  data_sharing::DataSharingService::Observer* data_sharing_observer;
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&sync_observer));
  EXPECT_CALL(*data_sharing_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&data_sharing_observer));

  // 6. AddingUserToGroup -> WaitingForSyncAndDataSharingGroup state.
  std::move(join_ui_callback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForSyncAndDataSharingGroup);

  // Added tab group in sync but not in data sharing should not transition.
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(group_id))
      .WillOnce(Return(data_sharing::MemberRole::kUnknown));
  sync_observer->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForSyncAndDataSharingGroup);

  // Simulate added in both tab group and data_sharing group.
  base::OnceCallback<void(Outcome)> promote_ui_callback;
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(group_id))
      .WillOnce(Return(data_sharing::MemberRole::kMember));
  EXPECT_CALL(*delegate_, PromoteTabGroup(data_sharing::GroupId(kGroupId),
                                          IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&promote_ui_callback));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(sync_observer));
  EXPECT_CALL(*data_sharing_service_, RemoveObserver(data_sharing_observer));

  // 7. WaitingForSyncAndDataSharingGroup -> OpeningLocalTabGroup state.
  // TODO(crbug.com/373403973): Remove data sharing observer when sync service
  // starts observing data sharing.
  sync_observer->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kOpeningLocalTabGroup);

  // Upon successfully promoting the tab group, the flow ends and exit.
  EXPECT_CALL(*delegate_, OnFlowFinished());
  std::move(promote_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kFlowRequirementsMet, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kOpenedNewGroup, 1);
}

TEST_F(CollaborationControllerTest, JoinFlowManagedDevice) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate managed device.
  ServiceStatus status;
  status.signin_status = SigninStatus::kNotSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kDisabledForPolicy;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSigninDisabledByPolicy),
                        IsNotNullCallback()));

  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, JoinFlowSignedInManagedAccountAsync) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate managed account signed in.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kSyncEnabled;
  status.collaboration_status = CollaborationStatus::kDisabledPending;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));
  CollaborationService::Observer* observer;
  EXPECT_CALL(*collaboration_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForPolicyUpdate);

  // The managed account info become available.
  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSyncDisabledByPolicy),
                        IsNotNullCallback()));
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.new_status = status;
  update.new_status.collaboration_status =
      CollaborationStatus::kDisabledForPolicy;
  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, JoinFlowSignedOutManagedAccountAsync) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate managed account with info not ready.
  ServiceStatus status;
  status.signin_status = SigninStatus::kNotSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kDisabledPending;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));
  CollaborationService::Observer* observer;
  EXPECT_CALL(*collaboration_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));

  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForPolicyUpdate);

  // The managed account info become available.
  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSigninDisabledByPolicy),
                        IsNotNullCallback()));
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.new_status = status;
  update.new_status.collaboration_status =
      CollaborationStatus::kDisabledForPolicy;
  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, JoinFlowManagedAccount) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate non-managed device.
  ServiceStatus status;
  status.signin_status = SigninStatus::kNotSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // The user should be shown authentication screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));

  // 2. Pending -> Authenticating state.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAuthenticating);

  // Simulate user successfully completes authentication with a managed account.
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kSyncEnabled;
  status.collaboration_status = CollaborationStatus::kDisabledForPolicy;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));
  ASSERT_TRUE(status.IsAuthenticationValid());

  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSyncDisabledByPolicy),
                        IsNotNullCallback()));

  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, UrlHandlingError) {
  RunLoop run_loop;
  // Start Join flow.
  InitializeController(run_loop.QuitClosure(),
                       Flow(FlowType::kJoin, GroupToken()));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate an error parsing join URL.
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kInvalidUrl),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);

  //  Simulate exiting the flow.
  EXPECT_CALL(*delegate_, OnFlowFinished());
  std::move(error_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

TEST_F(CollaborationControllerTest, DelegateOutcomeError) {
  RunLoop run_loop;
  // Start Join flow.
  InitializeJoinController(run_loop.QuitClosure());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate a failure on the UI side.
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kGenericError),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));
  std::move(prepare_ui_callback_).Run(Outcome::kFailure);

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);

  //  Simulate exiting the flow.
  EXPECT_CALL(*delegate_, OnFlowFinished());
  std::move(error_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

TEST_F(CollaborationControllerTest, ReadNewGroupError) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      callback;
  EXPECT_CALL(
      *data_sharing_service_,
      ReadNewGroup(GroupToken(data_sharing::GroupId(kGroupId), kAccessToken),
                   IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&callback));

  controller_->SetStateForTesting(StateId::kCheckingFlowRequirements);

  std::move(callback).Run(
      base::unexpected(data_sharing::DataSharingService::
                           PeopleGroupActionFailure::kPersistentFailure));

  // Fix this to expect error when SDK implementation is done.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAddingUserToGroup);
}

TEST_F(CollaborationControllerTest, PreviewDataUrlInvalidFailure) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  base::OnceCallback<void(const data_sharing::DataSharingService::
                              SharedDataPreviewOrFailureOutcome&)>
      preview_callback;
  EXPECT_CALL(*data_sharing_service_,
              GetSharedEntitiesPreview(
                  GroupToken(data_sharing::GroupId(kGroupId), kAccessToken),
                  IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&preview_callback));
  controller_->SetStateForTesting(StateId::kAddingUserToGroup);
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kInvalidUrl),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));

  std::move(preview_callback)
      .Run(base::unexpected(data_sharing::DataSharingService::
                                DataPreviewActionFailure::kPermissionDenied));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, AuthenticationCanceledBeforeSignIn) {
  base::HistogramTester histogram_tester;

  RunLoop run_loop;

  // Start Join flow at pending state.
  InitializeJoinController(run_loop.QuitClosure());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate user is not signed in or syncing.
  ServiceStatus status;
  status.signin_status = SigninStatus::kNotSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // The user should be shown authentication screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));

  // Pending -> Authenticating.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);

  // Authenticating -> Cancel state.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(authentication_ui_calback).Run(Outcome::kCancel);

  run_loop.Run();

  // Verify the not signed in metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kCanceledNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kFlowRequirementsMet, 0);
}

TEST_F(CollaborationControllerTest, AuthenticationCanceledAfterSignIn) {
  base::HistogramTester histogram_tester;

  RunLoop run_loop;

  // Start Join flow.
  InitializeJoinController(run_loop.QuitClosure());

  // Simulate getting to the Adding User To Group state.
  base::OnceCallback<void(Outcome)> join_ui_callback;
  base::OnceCallback<void(const data_sharing::DataSharingService::
                              SharedDataPreviewOrFailureOutcome&)>
      preview_callback;
  EXPECT_CALL(*data_sharing_service_,
              GetSharedEntitiesPreview(
                  GroupToken(data_sharing::GroupId(kGroupId), kAccessToken),
                  IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&preview_callback));
  EXPECT_CALL(*delegate_, ShowJoinDialog(_, _, IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&join_ui_callback));
  controller_->SetStateForTesting(StateId::kAddingUserToGroup);

  // Show group preview screen.
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = data_sharing::SharedTabGroupPreview();
  std::move(preview_callback).Run(preview);

  // Cancel the join flow.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(join_ui_callback).Run(Outcome::kCancel);

  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kCanceled, 1);
}

TEST_F(CollaborationControllerTest, AuthenticationError) {
  RunLoop run_loop;
  // Start Join flow with authenticating screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  InitializeJoinController(run_loop.QuitClosure());
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));
  controller_->SetStateForTesting(StateId::kAuthenticating);

  // Simulate Authentication finishing successfully on the UI, but getting
  // invalid authentication status in the service.
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kGenericError),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));
  EXPECT_CALL(*collaboration_service_, AddObserver(_));
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAuthenticating);

  // Reach time out.
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);

  //  Simulate exiting the flow.
  EXPECT_CALL(*delegate_, OnFlowFinished());
  std::move(error_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

TEST_F(CollaborationControllerTest, AuthenticationSuccessObserved) {
  RunLoop run_loop;
  // Start Join flow with authenticating screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  InitializeJoinController(run_loop.QuitClosure());
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));
  controller_->SetStateForTesting(StateId::kAuthenticating);

  // Simulate Authentication finishing successfully on the UI.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  CollaborationService::Observer* observer;
  EXPECT_CALL(*collaboration_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAuthenticating);

  // Simulate observing the status change.
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.old_status = status;
  update.new_status = status;
  update.new_status.sync_status = SyncStatus::kSyncEnabled;
  ASSERT_TRUE(update.new_status.IsAuthenticationValid());
  EXPECT_CALL(*delegate_, NotifySignInAndSyncStatusChange());
  EXPECT_CALL(*collaboration_service_, RemoveObserver(observer));
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded());
  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForServicesToInitialize);
}

TEST_F(CollaborationControllerTest, FullShareFlowAllStates) {
  base::HistogramTester histogram_tester;

  // Start Share flow.
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();

  // Simulate that the tab group exist locally, but is not shared.
  tab_groups::EitherGroupID either_id = local_id;
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetLocalGroupId(local_id);
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillOnce(Return(tab_group));

  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);
  CollaborationControllerDelegate::ResultWithGroupTokenCallback
      share_dialog_callback;
  EXPECT_CALL(*delegate_, ShowShareDialog(either_id, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&share_dialog_callback));

  controller_->SetStateForTesting(StateId::kCheckingFlowRequirements);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kShowingShareScreen);

  //   Simulate creating the collaboration group.
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillOnce(Return(tab_group));
  tab_groups::TabGroupSyncService::TabGroupSharingCallback
      tab_group_sharing_callback;
  EXPECT_CALL(*tab_group_sync_service_,
              MakeTabGroupShared(local_id, kGroupId, IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&tab_group_sharing_callback));
  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      group_data_callback;
  data_sharing::GroupId group_id(kGroupId);
  EXPECT_CALL(*data_sharing_service_,
              ReadGroupDeprecated(group_id, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&group_data_callback));
  data_sharing::GroupToken token(group_id, kAccessToken);
  std::move(share_dialog_callback).Run(Outcome::kSuccess, token);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kMakingTabGroupShared);

  // Simulate successfully making the tab group shared.
  std::string url = "test_url";
  EXPECT_CALL(*data_sharing_service_, GetDataSharingUrl(_))
      .WillOnce(Return(std::make_unique<GURL>(url)));
  EXPECT_CALL(*delegate_,
              OnUrlReadyToShare(group_id, GURL(url), IsNotNullCallback()));
  GroupData group_data =
      GroupData(group_id, /*display_name=*/"",
                /*members=*/{}, /*former_members=*/{}, kAccessToken);
  std::move(group_data_callback).Run(group_data);
  std::move(tab_group_sharing_callback)
      .Run(tab_groups::TabGroupSyncService::TabGroupSharingResult::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kSharingTabGroupUrl);

  // Verify the manage flow metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kFlowRequirementsMet, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kShareDialogShown, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kTabGroupShared, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kUrlReadyToShare, 1);
}

TEST_F(CollaborationControllerTest, CheckingFlowRequirementsManageFlow) {
  base::HistogramTester histogram_tester;

  // Start Share flow.
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();

  // Simulate that the tab group exist locally and is a shared tab group.
  tab_groups::EitherGroupID either_id = local_id;
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetLocalGroupId(local_id);
  // Simulate that the tab group exists locally and is a shared tab group.
  tab_group.SetCollaborationId(
      tab_groups::CollaborationId(std::string(kGroupId)));
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillRepeatedly(Return(tab_group));

  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);
  EXPECT_CALL(*delegate_, ShowManageDialog(either_id, IsNotNullCallback()));

  controller_->SetStateForTesting(StateId::kCheckingFlowRequirements);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kShowingManageScreen);

  // Verify the manage flow metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kFlowRequirementsMet, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kManageDialogShown, 1);
}

TEST_F(CollaborationControllerTest, ShareFlowCanceledBeforeSignin) {
  base::HistogramTester histogram_tester;

  // Simulate that the tab group exists locally, but is not shared.
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  tab_groups::EitherGroupID either_id = local_id;
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetLocalGroupId(local_id);
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillRepeatedly(Return(tab_group));

  // Start Share flow at pending state.
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate user is not signed in or syncing.
  ServiceStatus status;
  status.signin_status = SigninStatus::kNotSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // The user should be shown authentication screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));

  // Pending -> Authenticating.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);

  // Authenticating -> Cancel state.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(authentication_ui_calback).Run(Outcome::kCancel);

  // Verify the not signed in metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kCanceledNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kFlowRequirementsMet, 0);
}

}  // namespace collaboration
