// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/collaboration/internal/metrics.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_type.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/collaboration/public/service_status.h"
#include "components/collaboration/test_support/mock_collaboration_controller_delegate.h"
#include "components/collaboration/test_support/mock_collaboration_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration {

namespace {

const data_sharing::GroupId kGroupId("/?-group_id");
const char kAccessToken[] = "/?-access_token";
constexpr char kUserEmail[] = "test@email.com";

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
    EXPECT_CALL(*data_sharing_service_, GetLogger())
        .Times(::testing::AnyNumber());
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
        tab_group_sync_service_.get(), sync_service_.get(),
        identity_test_env_.identity_manager(), std::move(delegate),
        base::BindOnce(&CollaborationControllerTest::FinishFlow,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(run_on_flow_exit)));
    task_environment_.RunUntilIdle();
  }

  void InitializeJoinController(OnceClosure run_on_flow_exit) {
    InitializeController(
        std::move(run_on_flow_exit),
        Flow(FlowType::kJoin, GroupToken(kGroupId, kAccessToken)));
  }

  void FinishFlow(OnceClosure run_on_flow_exit, const void* controller) {
    controller_.reset();
    std::move(run_on_flow_exit).Run();
  }

  void SetUpJoinRequirements() {
    GroupToken token =
        GroupToken(data_sharing::GroupId(kGroupId), kAccessToken);
    EXPECT_CALL(*data_sharing_service_,
                ReadNewGroup(token, IsNotNullCallback()))
        .WillOnce(MoveArg<1>(&group_data_callback_));
    EXPECT_CALL(*data_sharing_service_,
                GetSharedEntitiesPreview(token, IsNotNullCallback()))
        .WillOnce(MoveArg<1>(&preview_callback_));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  base::OnceCallback<void(Outcome)> prepare_ui_callback_;
  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      group_data_callback_;
  base::OnceCallback<void(const data_sharing::DataSharingService::
                              SharedDataPreviewOrFailureOutcome&)>
      preview_callback_;
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
  base::TimeDelta authentication_time = base::Milliseconds(10);
  base::TimeDelta service_initialization_time = base::Milliseconds(11);
  base::TimeDelta tab_group_fetch_time = base::Milliseconds(12);

  RunLoop run_loop;

  // Start Join flow.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
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
  EXPECT_CALL(*delegate_,
              ShowAuthenticationUi(FlowType::kJoin, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&authentication_ui_calback));

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
  task_environment_.FastForwardBy(authentication_time);
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForServicesToInitialize);

  // Simulate that the user is not already in the tab group.
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(sync_observer));
  const GroupToken& token = GroupToken(kGroupId, kAccessToken);
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(kGroupId))
      .WillRepeatedly(Return(data_sharing::MemberRole::kUnknown));
  SetUpJoinRequirements();

  // 4. WaitingForServicesToInitialize -> CheckingFlowRequirementsState ->
  // AddingUserToGroup state.
  task_environment_.FastForwardBy(service_initialization_time);
  sync_observer->OnInitialized();
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAddingUserToGroup);

  // The user should be shown invitation screen for joining a collaboration
  // group.
  GroupData group_data =
      GroupData(kGroupId, /*display_name=*/"",
                /*members=*/{}, /*former_members=*/{}, kAccessToken);
  base::OnceCallback<void(Outcome)> join_ui_callback;
  EXPECT_CALL(*delegate_, ShowJoinDialog(_, _, IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&join_ui_callback));

  std::move(group_data_callback_).Run(group_data);
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = data_sharing::SharedTabGroupPreview();
  std::move(preview_callback_).Run(preview);

  // Simulate the user accepts the join invitation. Wait for tab group to be
  // added in sync.
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetCollaborationId(syncer::CollaborationId(kGroupId.value()));
  std::vector<SavedTabGroup> all_tab_groups;
  EXPECT_CALL(*tab_group_sync_service_, GetAllGroups())
      .WillRepeatedly(Return(all_tab_groups));
  EXPECT_CALL(*data_sharing_service_,
              ReadGroupDeprecated(kGroupId, IsNotNullCallback()));

  data_sharing::DataSharingService::Observer* data_sharing_observer;
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&sync_observer));
  EXPECT_CALL(*data_sharing_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&data_sharing_observer));

  // 5. AddingUserToGroup -> WaitingForSyncAndDataSharingGroup state.
  std::move(join_ui_callback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForSyncAndDataSharingGroup);

  // Added tab group in sync but not in data sharing should not transition.
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(kGroupId))
      .WillOnce(Return(data_sharing::MemberRole::kUnknown));
  sync_observer->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForSyncAndDataSharingGroup);

  // Simulate added in both tab group and data_sharing group.
  task_environment_.FastForwardBy(tab_group_fetch_time);
  base::OnceCallback<void(Outcome)> promote_ui_callback;
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(kGroupId))
      .WillOnce(Return(data_sharing::MemberRole::kMember));
  EXPECT_CALL(*delegate_, PromoteTabGroup(kGroupId, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&promote_ui_callback));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(sync_observer));
  EXPECT_CALL(*data_sharing_service_, RemoveObserver(data_sharing_observer));

  // 6. WaitingForSyncAndDataSharingGroup -> OpeningLocalTabGroup state.
  // TODO(crbug.com/373403973): Remove data sharing observer when sync service
  // starts observing data sharing.
  sync_observer->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kOpeningLocalTabGroup);

  // Upon successfully promoting the tab group, the flow ends and exit.
  EXPECT_CALL(*delegate_, OnFlowFinished());
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_));
  std::move(promote_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow.Events",
      metrics::CollaborationServiceFlowEvent::kFlowRequirementsMet, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kOpenedNewGroup, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kAddedUserToGroup, 1);
  histogram_tester.ExpectTimeBucketCount(
      "CollaborationService.Latency.AuthenticationInitToSuccess",
      authentication_time, 1);
  histogram_tester.ExpectTimeBucketCount(
      "CollaborationService.Latency.WaitingForServicesInitialization",
      service_initialization_time, 1);
  histogram_tester.ExpectTimeBucketCount(
      "CollaborationService.Latency.TabGroupFetchedAfterPeopleGroupJoined",
      tab_group_fetch_time, 1);
}

TEST_F(CollaborationControllerTest, JoinFlowManagedDevice) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate managed device.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSigninDisabled;
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
  status.sync_status = SyncStatus::kSyncDisabledByEnterprise;
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
  status.signin_status = SigninStatus::kSigninDisabled;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kDisabledPending;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // Get the last added observer which should be the one added by the current
  // state.
  CollaborationService::Observer* observer;
  EXPECT_CALL(*collaboration_service_, AddObserver(_))
      .WillRepeatedly(SaveArg<0>(&observer));

  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForPolicyUpdate);

  // The managed account info become available.
  EXPECT_CALL(*collaboration_service_, RemoveObserver(_)).Times(2);
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

TEST_F(CollaborationControllerTest, JoinFlowManagedAccountSharingDisabled) {
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
  EXPECT_CALL(*delegate_,
              ShowAuthenticationUi(FlowType::kJoin, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&authentication_ui_calback));

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
              ShowError(ErrorInfo(ErrorInfo::Type::kSharingDisabledByPolicy),
                        IsNotNullCallback()));

  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, ManageFlowManagedAccountSharingDisabled) {
  // Start manage flow with a local shared tab group.
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  tab_groups::EitherGroupID either_id = local_id;
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetLocalGroupId(local_id);
  tab_group.SetCollaborationId(syncer::CollaborationId(kGroupId.value()));
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillRepeatedly(Return(tab_group));

  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate managed device sharing disabled.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kSyncEnabled;
  status.collaboration_status = CollaborationStatus::kDisabledForPolicy;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // The share flow is allowed to proceed to share screen.
  // 2. Pending -> Showing share screen state.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kShowingManageScreen);
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

TEST_F(CollaborationControllerTest, JoinFlowVersionOutOfDate) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate version out of date.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kSyncEnabled;
  status.collaboration_status =
      CollaborationStatus::kVersionOutOfDateShowUpdateChromeUi;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  EXPECT_CALL(
      *delegate_,
      ShowError(ErrorInfo(ErrorInfo::Type::kUpdateChromeUiForVersionOutOfDate,
                          FlowType::kJoin),
                IsNotNullCallback()));

  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, ShareFlowVersionOutOfDate) {
  // Start Share flow.
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  tab_groups::EitherGroupID either_id = local_id;
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetLocalGroupId(local_id);
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillRepeatedly(Return(tab_group));

  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate version out of date.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kSyncEnabled;
  status.collaboration_status =
      CollaborationStatus::kVersionOutOfDateShowUpdateChromeUi;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  EXPECT_CALL(
      *delegate_,
      ShowError(ErrorInfo(ErrorInfo::Type::kUpdateChromeUiForVersionOutOfDate,
                          FlowType::kShareOrManage),
                IsNotNullCallback()));
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
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
  SetUpJoinRequirements();

  controller_->SetStateForTesting(StateId::kAddingUserToGroup);
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = data_sharing::SharedTabGroupPreview();
  std::move(preview_callback_).Run(preview);
  std::move(group_data_callback_)
      .Run(base::unexpected(data_sharing::DataSharingService::
                                PeopleGroupActionFailure::kPersistentFailure));

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, ReadNewGroupAlreadyExist) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  SetUpJoinRequirements();
  controller_->SetStateForTesting(StateId::kAddingUserToGroup);

  CoreAccountInfo account = identity_test_env_.SetPrimaryAccount(
      kUserEmail, signin::ConsentLevel::kSignin);
  data_sharing::GroupMember self;
  self.gaia_id = account.gaia;
  self.role = data_sharing::MemberRole::kOwner;
  GroupData group_data =
      GroupData(kGroupId, /*display_name=*/"",
                /*members=*/{self}, /*former_members=*/{}, kAccessToken);
  std::move(group_data_callback_).Run(group_data);
  std::move(preview_callback_)
      .Run(base::unexpected(data_sharing::DataSharingService::
                                DataPreviewActionFailure::kGroupFull));

  // Fix this to expect error when SDK implementation is done.
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForSyncAndDataSharingGroup);
}

TEST_F(CollaborationControllerTest, PreviewDataUrlInvalidFailure) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());
  SetUpJoinRequirements();

  controller_->SetStateForTesting(StateId::kAddingUserToGroup);
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kInvalidUrl),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));

  std::move(preview_callback_)
      .Run(base::unexpected(data_sharing::DataSharingService::
                                DataPreviewActionFailure::kPermissionDenied));
  std::move(group_data_callback_).Run(GroupData());

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest, PreviewDataGroupFullFailure) {
  // Start Join flow.
  InitializeJoinController(base::DoNothing());

  SetUpJoinRequirements();
  controller_->SetStateForTesting(StateId::kAddingUserToGroup);
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kGroupFull),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(kGroupId))
      .WillRepeatedly(Return(data_sharing::MemberRole::kUnknown));

  std::move(preview_callback_)
      .Run(base::unexpected(data_sharing::DataSharingService::
                                DataPreviewActionFailure::kGroupFull));
  std::move(group_data_callback_).Run(GroupData());

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
  EXPECT_CALL(*delegate_,
              ShowAuthenticationUi(FlowType::kJoin, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&authentication_ui_calback));

  // Pending -> Authenticating.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);

  // Authenticating -> Cancel state.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(authentication_ui_calback).Run(Outcome::kCancel);

  run_loop.Run();

  // Verify the not signed in metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow.Events",
      metrics::CollaborationServiceFlowEvent::kNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow.Events",
      metrics::CollaborationServiceFlowEvent::kCanceledNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow.Events",
      metrics::CollaborationServiceFlowEvent::kFlowRequirementsMet, 0);
}

TEST_F(CollaborationControllerTest, AuthenticationCanceledAfterSignIn) {
  base::HistogramTester histogram_tester;

  RunLoop run_loop;

  // Start Join flow.
  InitializeJoinController(run_loop.QuitClosure());
  SetUpJoinRequirements();

  // Simulate getting to the Adding User To Group state.
  base::OnceCallback<void(Outcome)> join_ui_callback;
  EXPECT_CALL(*delegate_, ShowJoinDialog(_, _, IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&join_ui_callback));
  controller_->SetStateForTesting(StateId::kAddingUserToGroup);

  // Show group preview screen.
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = data_sharing::SharedTabGroupPreview();
  std::move(preview_callback_).Run(preview);
  std::move(group_data_callback_).Run(GroupData());

  // Cancel the join flow.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(join_ui_callback).Run(Outcome::kCancel);

  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kCanceled, 1);
}

TEST_F(CollaborationControllerTest, SimulateFailureToAddUserToGroup) {
  base::HistogramTester histogram_tester;

  RunLoop run_loop;

  // Start Join flow.
  InitializeJoinController(run_loop.QuitClosure());
  SetUpJoinRequirements();

  // Simulate getting to the Adding User To Group state.
  base::OnceCallback<void(Outcome)> join_ui_callback;
  EXPECT_CALL(*delegate_, ShowJoinDialog(_, _, IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&join_ui_callback));

  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(ErrorInfo(ErrorInfo::Type::kGenericError),
                                    IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&error_ui_callback));
  controller_->SetStateForTesting(StateId::kAddingUserToGroup);

  // Show group preview screen.
  data_sharing::SharedDataPreview preview;
  preview.shared_tab_group_preview = data_sharing::SharedTabGroupPreview();
  std::move(preview_callback_).Run(preview);
  std::move(group_data_callback_).Run(GroupData());

  // Simulate failure on the join flow.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(join_ui_callback).Run(Outcome::kFailure);
  std::move(error_ui_callback).Run(Outcome::kSuccess);

  run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "CollaborationService.JoinFlow",
      metrics::CollaborationServiceJoinEvent::kFailedAddingUserToGroup, 1);
}

TEST_F(CollaborationControllerTest, AuthenticationError) {
  RunLoop run_loop;
  // Start Join flow with authenticating screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  InitializeJoinController(run_loop.QuitClosure());
  EXPECT_CALL(*delegate_,
              ShowAuthenticationUi(FlowType::kJoin, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&authentication_ui_calback));
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
  EXPECT_CALL(*delegate_,
              ShowAuthenticationUi(FlowType::kJoin, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&authentication_ui_calback));
  controller_->SetStateForTesting(StateId::kAuthenticating);

  // Simulate Authentication finishing successfully on the UI.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  status.collaboration_status = CollaborationStatus::kEnabledCreateAndJoin;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillRepeatedly(Return(status));

  // Get the last added observer which should be the one added by the current
  // state.
  CollaborationService::Observer* observer;
  EXPECT_CALL(*collaboration_service_, AddObserver(_))
      .WillRepeatedly(SaveArg<0>(&observer));
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAuthenticating);

  // Simulate observing the status change.
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.old_status = status;
  update.new_status = status;
  update.new_status.sync_status = SyncStatus::kSyncEnabled;
  ASSERT_TRUE(update.new_status.IsAuthenticationValid());
  EXPECT_CALL(*delegate_, NotifySignInAndSyncStatusChange());
  EXPECT_CALL(*collaboration_service_, RemoveObserver(_)).Times(2);
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded());
  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForServicesToInitialize);
}

TEST_F(CollaborationControllerTest, FullShareFlowAllStates) {
  base::HistogramTester histogram_tester;
  base::TimeDelta url_ready_time = base::Milliseconds(10);

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
  EXPECT_CALL(
      *tab_group_sync_service_,
      MakeTabGroupShared(local_id, syncer::CollaborationId(kGroupId.value()),
                         IsNotNullCallback()))
      .WillOnce(MoveArg<2>(&tab_group_sharing_callback));
  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      group_data_callback;
  EXPECT_CALL(*data_sharing_service_,
              ReadGroupDeprecated(kGroupId, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&group_data_callback));
  data_sharing::GroupToken token(kGroupId, kAccessToken);
  std::move(share_dialog_callback).Run(Outcome::kSuccess, token);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kMakingTabGroupShared);

  // Simulate successfully making the tab group shared.
  std::string url = "test_url";
  EXPECT_CALL(*data_sharing_service_, GetDataSharingUrl(_))
      .WillOnce(Return(std::make_unique<GURL>(url)));
  EXPECT_CALL(*delegate_,
              OnUrlReadyToShare(kGroupId, GURL(url), IsNotNullCallback()));
  GroupData group_data =
      GroupData(kGroupId, /*display_name=*/"",
                /*members=*/{}, /*former_members=*/{}, kAccessToken);
  std::move(group_data_callback).Run(group_data);
  task_environment_.FastForwardBy(url_ready_time);
  std::move(tab_group_sharing_callback)
      .Run(tab_groups::TabGroupSyncService::TabGroupSharingResult::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kSharingTabGroupUrl);

  // Verify the manage flow metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow.Events",
      metrics::CollaborationServiceFlowEvent::kFlowRequirementsMet, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kShareDialogShown, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::
          kCollaborationGroupCreated,
      1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kTabGroupShared, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kUrlReadyToShare, 1);
  histogram_tester.ExpectTimeBucketCount(
      "CollaborationService.Latency.LinkReadyAfterGroupCreation",
      url_ready_time, 1);
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
  tab_group.SetCollaborationId(syncer::CollaborationId(kGroupId.value()));
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillRepeatedly(Return(tab_group));

  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  base::OnceCallback<void(Outcome)> manage_dialog_callback;
  EXPECT_CALL(*delegate_, ShowManageDialog(either_id, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&manage_dialog_callback));

  controller_->SetStateForTesting(StateId::kCheckingFlowRequirements);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kShowingManageScreen);

  // Verify the manage flow metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow.Events",
      metrics::CollaborationServiceFlowEvent::kFlowRequirementsMet, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow",
      metrics::CollaborationServiceShareOrManageEvent::kManageDialogShown, 1);

  // Delete the tab group from manage screen.
  EXPECT_CALL(
      *tab_group_sync_service_,
      OnCollaborationRemoved(syncer::CollaborationId(kGroupId.value())));
  EXPECT_CALL(*data_sharing_service_, OnCollaborationGroupRemoved(kGroupId));
  EXPECT_CALL(*delegate_, OnFlowFinished());
  std::move(manage_dialog_callback).Run(Outcome::kGroupLeftOrDeleted);
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
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(FlowType::kShareOrManage,
                                               IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&authentication_ui_calback));

  // Pending -> Authenticating.
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);

  // Authenticating -> Cancel state.
  EXPECT_CALL(*delegate_, OnFlowFinished);
  std::move(authentication_ui_calback).Run(Outcome::kCancel);

  // Verify the not signed in metrics are recorded properly.
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow.Events",
      metrics::CollaborationServiceFlowEvent::kNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow.Events",
      metrics::CollaborationServiceFlowEvent::kCanceledNotSignedIn, 1);
  histogram_tester.ExpectBucketCount(
      "CollaborationService.ShareOrManageFlow.Events",
      metrics::CollaborationServiceFlowEvent::kFlowRequirementsMet, 0);
}

TEST_F(CollaborationControllerTest, LeaveFlow) {
  // Start leave flow.
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  tab_groups::EitherGroupID either_id = local_id;
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetLocalGroupId(local_id);
  tab_group.SetCollaborationId(syncer::CollaborationId(kGroupId.value()));
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(either_id))
      .WillRepeatedly(Return(tab_group));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kLeaveOrDelete, either_id));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate that the user is part of the group as member.
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(kGroupId))
      .WillRepeatedly(Return(data_sharing::MemberRole::kMember));
  base::OnceCallback<void(Outcome)> leave_dialog_callback;
  EXPECT_CALL(*delegate_, ShowLeaveDialog(either_id, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&leave_dialog_callback));

  controller_->SetStateForTesting(StateId::kCheckingFlowRequirements);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kLeavingGroup);

  // Leave the collaboration group.
  base::OnceCallback<void(
      data_sharing::DataSharingService::PeopleGroupActionOutcome)>
      people_group_action_callback;
  EXPECT_CALL(*data_sharing_service_, LeaveGroup(kGroupId, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&people_group_action_callback));
  std::move(leave_dialog_callback).Run(Outcome::kSuccess);

  // Clean up the tab group.
  EXPECT_CALL(
      *tab_group_sync_service_,
      OnCollaborationRemoved(syncer::CollaborationId(kGroupId.value())));
  EXPECT_CALL(*data_sharing_service_, OnCollaborationGroupRemoved(kGroupId));
  EXPECT_CALL(*delegate_, OnFlowFinished());
  std::move(people_group_action_callback)
      .Run(
          data_sharing::DataSharingService::PeopleGroupActionOutcome::kSuccess);
}

TEST_F(CollaborationControllerTest,
       OnServiceStatusChanged_SyncDisabledByEnterprise) {
  InitializeJoinController(base::DoNothing());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  CollaborationService::Observer* observer = controller_.get();
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.old_status.collaboration_status =
      CollaborationStatus::kEnabledCreateAndJoin;
  update.new_status.collaboration_status =
      CollaborationStatus::kDisabledForPolicy;
  update.new_status.sync_status = SyncStatus::kSyncDisabledByEnterprise;

  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSyncDisabledByPolicy),
                        IsNotNullCallback()));

  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest,
       OnServiceStatusChanged_SigninDisabledByEnterprise) {
  InitializeJoinController(base::DoNothing());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  CollaborationService::Observer* observer = controller_.get();
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.old_status.collaboration_status =
      CollaborationStatus::kEnabledCreateAndJoin;
  update.new_status.collaboration_status =
      CollaborationStatus::kDisabledForPolicy;
  update.new_status.signin_status = SigninStatus::kSigninDisabled;

  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSigninDisabledByPolicy),
                        IsNotNullCallback()));

  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest,
       OnServiceStatusChanged_SharingDisabledByEnterprise) {
  InitializeJoinController(base::DoNothing());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  CollaborationService::Observer* observer = controller_.get();
  CollaborationService::Observer::ServiceStatusUpdate update;
  update.old_status.collaboration_status =
      CollaborationStatus::kEnabledCreateAndJoin;
  update.new_status.collaboration_status =
      CollaborationStatus::kDisabledForPolicy;
  // Neither signin nor sync is the reason for policy disabling.
  update.new_status.signin_status = SigninStatus::kSignedIn;
  update.new_status.sync_status = SyncStatus::kSyncEnabled;

  EXPECT_CALL(*delegate_,
              ShowError(ErrorInfo(ErrorInfo::Type::kSharingDisabledByPolicy),
                        IsNotNullCallback()));

  observer->OnServiceStatusChanged(update);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);
}

TEST_F(CollaborationControllerTest,
       OnTabGroupRemoved_LocalId_CancelsFlowIfMatching) {
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();

  // Start Share flow associated with local_id.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));

  EXPECT_CALL(*delegate_, Cancel(_));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_));

  // Removing the tab group associated with local_id cancels the flow.
  controller_->OnTabGroupRemoved(local_id, tab_groups::TriggerSource::REMOTE);
}

TEST_F(CollaborationControllerTest,
       OnTabGroupRemoved_LocalId_DoesNotCancelFlowIfNotMatching) {
  tab_groups::LocalTabGroupID flow_local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  tab_groups::LocalTabGroupID removed_local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  ASSERT_NE(flow_local_id, removed_local_id);

  // Start Share flow associated with flow_local_id.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, flow_local_id));

  EXPECT_CALL(*delegate_, Cancel(_)).Times(0);

  // Removing a different tab group (removed_local_id) does not cancel the
  // flow.
  controller_->OnTabGroupRemoved(removed_local_id,
                                 tab_groups::TriggerSource::REMOTE);
}

TEST_F(CollaborationControllerTest,
       OnTabGroupRemoved_SyncId_CancelsFlowIfMatching) {
  base::Uuid sync_id = base::Uuid::GenerateRandomV4();

  // Start Share flow associated with sync_id.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, sync_id));

  EXPECT_CALL(*delegate_, Cancel(_));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_));

  // Removing the tab group associated with sync_id cancels the flow.
  controller_->OnTabGroupRemoved(sync_id, tab_groups::TriggerSource::REMOTE);
}

TEST_F(CollaborationControllerTest,
       OnTabGroupRemoved_SyncId_DoesNotCancelFlowIfNotMatching) {
  base::Uuid flow_sync_id = base::Uuid::GenerateRandomV4();
  base::Uuid removed_sync_id = base::Uuid::GenerateRandomV4();
  ASSERT_NE(flow_sync_id, removed_sync_id);

  // Start Share flow associated with flow_sync_id.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, flow_sync_id));

  EXPECT_CALL(*delegate_, Cancel(_)).Times(0);

  // Removing a different tab group (removed_sync_id) does not cancel the
  // flow.
  controller_->OnTabGroupRemoved(removed_sync_id,
                                 tab_groups::TriggerSource::REMOTE);
}

TEST_F(CollaborationControllerTest,
       OnTabGroupMigrated_CancelsFlowForOldSyncId) {
  base::Uuid old_sync_id = base::Uuid::GenerateRandomV4();

  // Start Share flow at pending state.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, old_sync_id));

  SavedTabGroup new_group(std::u16string(u"new_title"),
                          tab_groups::TabGroupColorId::kBlue, {});

  EXPECT_CALL(*delegate_, Cancel(_));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_));

  // Migrating the tab group associated with the old_sync_id cancels the flow.
  controller_->OnTabGroupMigrated(new_group, old_sync_id,
                                  tab_groups::TriggerSource::REMOTE);
}

TEST_F(CollaborationControllerTest,
       OnTabGroupMigrated_CancelsFlowForNewLocalId) {
  tab_groups::LocalTabGroupID local_id =
      tab_groups::test::GenerateRandomTabGroupID();
  base::Uuid old_sync_id = base::Uuid::GenerateRandomV4();

  // Start Share flow with a local_id.
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_));
  InitializeController(base::DoNothing(),
                       Flow(FlowType::kShareOrManage, local_id));

  SavedTabGroup new_group(std::u16string(u"new_title"),
                          tab_groups::TabGroupColorId::kBlue, {});
  new_group.SetLocalGroupId(local_id);

  EXPECT_CALL(*delegate_, Cancel(_));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(_));

  // Migrating a different tab group (old_sync_id) to a new group
  // that has the same local_id as the current flow should cancel the flow.
  controller_->OnTabGroupMigrated(new_group, old_sync_id,
                                  tab_groups::TriggerSource::REMOTE);
}

}  // namespace collaboration
