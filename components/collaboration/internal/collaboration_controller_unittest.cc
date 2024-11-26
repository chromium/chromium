// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/test_support/mock_collaboration_controller_delegate.h"
#include "components/collaboration/test_support/mock_collaboration_service.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
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

  void InitializeController(
      OnceClosure run_on_flow_exit,
      const GroupToken& token = GroupToken(data_sharing::GroupId(kGroupId),
                                           kAccessToken)) {
    std::unique_ptr<MockCollaborationControllerDelegate> delegate =
        std::make_unique<MockCollaborationControllerDelegate>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, PrepareFlowUI(IsNotNullCallback()))
        .WillOnce(MoveArg<0>(&prepare_ui_callback_));
    controller_ = std::make_unique<CollaborationController>(
        CollaborationController::Flow::kJoin, token,
        collaboration_service_.get(), data_sharing_service_.get(),
        tab_group_sync_service_.get(), sync_service_.get(), std::move(delegate),
        base::BindOnce(&CollaborationControllerTest::FinishFlow,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(run_on_flow_exit)));
  }

  void FinishFlow(OnceClosure run_on_flow_exit) {
    controller_.reset();
    std::move(run_on_flow_exit).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::OnceCallback<void(Outcome)> prepare_ui_callback_;
  MockCollaborationControllerDelegate* delegate_;
  std::unique_ptr<MockCollaborationService> collaboration_service_;
  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
  std::unique_ptr<tab_groups::MockTabGroupSyncService> tab_group_sync_service_;
  std::unique_ptr<syncer::MockSyncService> sync_service_;
  std::unique_ptr<CollaborationController> controller_;
  base::WeakPtrFactory<CollaborationControllerTest> weak_ptr_factory_{this};
};

TEST_F(CollaborationControllerTest, FullFlowAllStates) {
  RunLoop run_loop;

  // Start Join flow.
  InitializeController(run_loop.QuitClosure());

  // 1. Pending state.
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate user is signed in, but not syncing.
  ServiceStatus status;
  status.signin_status = SigninStatus::kSignedIn;
  status.sync_status = SyncStatus::kNotSyncing;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillOnce(Return(status));

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
      .WillOnce(Return(status));
  ASSERT_TRUE(status.IsAuthenticationValid());

  // Simulate that the user is not already in the tab group.
  data_sharing::GroupId group_id(kGroupId);
  const GroupToken& token =
      GroupToken(data_sharing::GroupId(kGroupId), kAccessToken);
  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      group_data_callback;
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(group_id))
      .WillOnce(Return(data_sharing::MemberRole::kUnknown));
  EXPECT_CALL(*data_sharing_service_, ReadNewGroup(token, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&group_data_callback));

  // 3. Authenticating -> CheckingFlowRequirements state.
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kCheckingFlowRequirements);


  // The user should be shown invitation screen for joining a collaboration
  // group.
  GroupData group_data = GroupData(group_id, /*display_name=*/"",
                                   /*members=*/{}, kAccessToken);
  base::OnceCallback<void(Outcome)> join_ui_callback;
  EXPECT_CALL(*delegate_, ShowJoinDialog(_, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&join_ui_callback));

  // 4. CheckingFlowRequirements -> AddingUserToGroup state.
  std::move(group_data_callback).Run(group_data);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAddingUserToGroup);

  // Simulate the user accepts the join invitation. Wait for tab group to be
  // added in sync.
  SavedTabGroup tab_group(std::u16string(u"title"),
                          tab_groups::TabGroupColorId::kGrey, {});
  tab_group.SetCollaborationId(
      tab_groups::CollaborationId(std::string(kGroupId)));
  std::vector<SavedTabGroup> all_tab_groups;
  EXPECT_CALL(*tab_group_sync_service_, GetAllGroups())
      .WillOnce(Return(all_tab_groups));

  tab_groups::TabGroupSyncService::Observer* sync_observer;
  data_sharing::DataSharingService::Observer* data_sharing_observer;
  EXPECT_CALL(*sync_service_, TriggerRefresh(_));
  EXPECT_CALL(*tab_group_sync_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&sync_observer));
  EXPECT_CALL(*data_sharing_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&data_sharing_observer));

  // 5. AddingUserToGroup -> WaitingForSyncAndDataSharingGroup state.
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
  EXPECT_CALL(*delegate_, PromoteTabGroup(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&promote_ui_callback));
  EXPECT_CALL(*tab_group_sync_service_, RemoveObserver(sync_observer));
  EXPECT_CALL(*data_sharing_service_, RemoveObserver(data_sharing_observer));

  // 5. WaitingForSyncAndDataSharingGroup -> OpeningLocalTabGroup state.
  // TODO(crbug.com/373403973): Remove data sharing observer when sync service
  // starts observing data sharing.
  sync_observer->OnTabGroupAdded(tab_group, tab_groups::TriggerSource::REMOTE);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kOpeningLocalTabGroup);

  // Upon successfully promoting the tab group, the flow ends and exit.
  std::move(promote_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

TEST_F(CollaborationControllerTest, UrlHandlingError) {
  RunLoop run_loop;
  // Start Join flow.
  InitializeController(run_loop.QuitClosure(), GroupToken());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate an error parsing join URL.
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(IsNotNullCallback(),
                                    ErrorInfo(ErrorInfo::Type::kGenericError)))
      .WillOnce(MoveArg<0>(&error_ui_callback));
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);

  //  Simulate exiting the flow.
  std::move(error_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

TEST_F(CollaborationControllerTest, DelegateOutcomeError) {
  RunLoop run_loop;
  // Start Join flow.
  InitializeController(run_loop.QuitClosure());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // Simulate a failure on the UI side.
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(IsNotNullCallback(),
                                    ErrorInfo(ErrorInfo::Type::kGenericError)))
      .WillOnce(MoveArg<0>(&error_ui_callback));
  std::move(prepare_ui_callback_).Run(Outcome::kFailure);

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);

  //  Simulate exiting the flow.
  std::move(error_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

TEST_F(CollaborationControllerTest, AuthenticationError) {
  RunLoop run_loop;
  // Start Join flow with authenticating screens.
  base::OnceCallback<void(Outcome)> authentication_ui_calback;
  InitializeController(run_loop.QuitClosure());
  EXPECT_CALL(*delegate_, ShowAuthenticationUi(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&authentication_ui_calback));
  controller_->SetStateForTesting(StateId::kAuthenticating);

  // Simulate Authentication finishing successfully on the UI, but getting
  // invalid authentication status in the service.
  base::OnceCallback<void(Outcome)> error_ui_callback;
  EXPECT_CALL(*delegate_, ShowError(IsNotNullCallback(),
                                    ErrorInfo(ErrorInfo::Type::kGenericError)))
      .WillOnce(MoveArg<0>(&error_ui_callback));
  ServiceStatus status;
  status.signin_status = SigninStatus::kNotSignedIn;
  status.sync_status = SyncStatus::kSyncEnabled;
  ASSERT_FALSE(status.IsAuthenticationValid());
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillOnce(Return(status));
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);

  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kError);

  //  Simulate exiting the flow.
  std::move(error_ui_callback).Run(Outcome::kSuccess);
  run_loop.Run();
}

}  // namespace collaboration
