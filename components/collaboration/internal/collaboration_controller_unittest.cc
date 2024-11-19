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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration {

namespace {

const char kGroupId[] = "/?-group_id";
const char kAccessToken[] = "/?-access_token";

using StateId = CollaborationController::StateId;
using Outcome = CollaborationControllerDelegate::Outcome;
using ErrorInfo = CollaborationControllerDelegate::ErrorInfo;
using base::RepeatingClosure;
using base::RunLoop;
using base::test::IsNotNullCallback;
using base::test::RunOnceCallback;
using data_sharing::GroupToken;
using ::testing::_;
using testing::Return;

}  // namespace

class CollaborationControllerTest : public testing::Test {
 public:
  CollaborationControllerTest() = default;
  ~CollaborationControllerTest() override = default;

  void SetUp() override {
    collaboration_service_ = std::make_unique<MockCollaborationService>();
    data_sharing_service_ =
        std::make_unique<data_sharing::MockDataSharingService>();
  }

  void TearDown() override {}

  void InitializeController(
      RepeatingClosure closure,
      const GroupToken& token = GroupToken(data_sharing::GroupId(kGroupId),
                                           kAccessToken)) {
    std::unique_ptr<MockCollaborationControllerDelegate> delegate =
        std::make_unique<MockCollaborationControllerDelegate>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, PrepareFlowUI(IsNotNullCallback()))
        .WillOnce(MoveArg<0>(&prepare_ui_callback_));
    controller_ = std::make_unique<CollaborationController>(
        CollaborationController::Flow::kJoin, token,
        collaboration_service_.get(), data_sharing_service_.get(), nullptr,
        std::move(delegate),
        base::BindOnce(&CollaborationControllerTest::FinishFlow,
                       weak_ptr_factory_.GetWeakPtr(), std::move(closure)));
  }

  void FinishFlow(RepeatingClosure closure) {
    controller_.reset();
    std::move(closure).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::OnceCallback<void(Outcome)> prepare_ui_callback_;
  MockCollaborationControllerDelegate* delegate_;
  std::unique_ptr<MockCollaborationService> collaboration_service_;
  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
  std::unique_ptr<CollaborationController> controller_;
  base::WeakPtrFactory<CollaborationControllerTest> weak_ptr_factory_{this};
};

TEST_F(CollaborationControllerTest, FullFlowAllStates) {
  // Start Join flow.
  InitializeController(base::DoNothing());
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kPending);

  // 1. Pending state, transition to Authenticating state.

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
  std::move(prepare_ui_callback_).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAuthenticating);

  // 2. Authenticating state, transition to CheckingFlowRequirements state.

  // Simulate user successfully completes authentication.
  status.sync_status = SyncStatus::kSyncEnabled;
  EXPECT_CALL(*collaboration_service_, GetServiceStatus())
      .WillOnce(Return(status));
  ASSERT_TRUE(status.IsAuthenticationValid());

  // Simulate that the user is not already in the tab group.
  data_sharing::GroupId group_id(kGroupId);
  base::OnceCallback<void(
      const data_sharing::DataSharingService::GroupDataOrFailureOutcome&)>
      group_data_callback;
  EXPECT_CALL(*collaboration_service_, GetCurrentUserRoleForGroup(group_id))
      .WillOnce(Return(data_sharing::MemberRole::kUnknown));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(group_id, IsNotNullCallback()))
      .WillOnce(MoveArg<1>(&group_data_callback));
  std::move(authentication_ui_calback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kCheckingFlowRequirements);

  // 3. CheckingFlowRequirements state, transition to AddingUserToGroup state.

  // The user should be shown invitation screen for joining a collaboration
  // group.
  base::OnceCallback<void(Outcome)> join_ui_callback;
  EXPECT_CALL(*delegate_, ShowJoinDialog(IsNotNullCallback()))
      .WillOnce(MoveArg<0>(&join_ui_callback));
  std::move(group_data_callback)
      .Run(data_sharing::GroupData(group_id, /*display_name=*/"",
                                   /*members=*/{}, kAccessToken));
  EXPECT_EQ(controller_->GetStateForTesting(), StateId::kAddingUserToGroup);

  // TODO(crbug.com/360184707): Finish AddingUserToGroup and transition to
  // WaitingForSyncTabGroup state.

  // 4. AddingUserToGroup state, transition to WaitingForSyncTabGroup state.

  // Simulate the user accepts the join invitation. Wait for tab group to be
  // added in sync.
  std::move(join_ui_callback).Run(Outcome::kSuccess);
  EXPECT_EQ(controller_->GetStateForTesting(),
            StateId::kWaitingForSyncTabGroup);
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
