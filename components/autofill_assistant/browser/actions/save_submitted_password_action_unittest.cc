// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/save_submitted_password_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kOrigin[] = "https://example.com";
const char kUsername[] = "username";
}  // namespace

namespace autofill_assistant {

using password_manager::MockPasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTracker;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class SaveSubmittedPasswordActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));

    ON_CALL(mock_action_delegate_, GetPasswordChangeSuccessTracker)
        .WillByDefault(Return(&mock_password_change_success_tracker_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    SaveSubmittedPasswordAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  MockPasswordChangeSuccessTracker mock_password_change_success_tracker_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  UserData user_data_;
};

TEST_F(SaveSubmittedPasswordActionTest, SaveSubmittedPasswordSuccess) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(mock_website_login_manager_, ReadyToCommitSubmittedPassword)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword);
  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowCompleted(
          GURL(kOrigin), kUsername,
          PasswordChangeSuccessTracker::EndEvent::kAutomatedOwnPasswordFlow));
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       false))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest, AttemptToSaveSameSubmittedPassword) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(mock_website_login_manager_, ReadyToCommitSubmittedPassword)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(true));

  // If the submitted password is the same as the previous one, we expect that
  // the action applies, but no save methods are called and the respective
  // flag is set in the returned proto.
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       true))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest,
       ReadyToCommitSubmittedPasswordPreconditionFails) {
  EXPECT_CALL(mock_website_login_manager_, ReadyToCommitSubmittedPassword)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword).Times(0);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  Run();
}

}  // namespace autofill_assistant
