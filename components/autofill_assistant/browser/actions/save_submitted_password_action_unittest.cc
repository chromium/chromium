// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/save_submitted_password_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/save_password_leak_detection_delegate.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kOrigin[] = "https://example.com";
const char kUsername[] = "username";
}  // namespace

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using password_manager::MockPasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTracker;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArgPointee;
using ::testing::StrictMock;
using ::testing::WithArg;

class SaveSubmittedPasswordActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));

    ON_CALL(mock_action_delegate_, GetPasswordChangeSuccessTracker)
        .WillByDefault(Return(&mock_password_change_success_tracker_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));

    // Trigger a leak detection check by default.
    proto_.mutable_save_submitted_password()->set_leak_detection_timeout_ms(0);
  }

 protected:
  void Run() {
    SaveSubmittedPasswordAction action(&mock_action_delegate_, proto_);
    action.ProcessAction(callback_.Get());
  }

  ActionProto proto_;
  MockActionDelegate mock_action_delegate_;
  StrictMock<MockWebsiteLoginManager> mock_website_login_manager_;
  MockPasswordChangeSuccessTracker mock_password_change_success_tracker_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  UserData user_data_;
};

TEST_F(SaveSubmittedPasswordActionTest, SaveSubmittedPasswordSuccess) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(true));
  // Check for password equality.
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword);
  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowCompleted(GURL(kOrigin), kUsername,
                                    PasswordChangeSuccessTracker::EndEvent::
                                        kAutomatedFlowOwnPasswordChosen));

  // Check for leaked credentials.
  EXPECT_CALL(mock_website_login_manager_,
              CheckWhetherSubmittedCredentialIsLeaked)
      .WillOnce(RunOnceCallback<0>(
          LeakDetectionStatus(LeakDetectionStatusCode::SUCCESS), false));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       false)),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(
                  &SaveSubmittedPasswordProto::Result::used_leaked_credential,
                  false))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest, AttemptToSaveSameSubmittedPassword) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(true));

  // If the submitted password is the same as the previous one, we expect that
  // the action applies, but no save methods are called and the respective
  // flag is set in the returned proto.
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword).Times(0);

  // Check for leaked credentials and assume that the old one was not leaked.
  EXPECT_CALL(mock_website_login_manager_,
              CheckWhetherSubmittedCredentialIsLeaked)
      .WillOnce(RunOnceCallback<0>(
          LeakDetectionStatus(LeakDetectionStatusCode::SUCCESS), false));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       true)),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(
                  &SaveSubmittedPasswordProto::Result::used_leaked_credential,
                  false))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest, SaveLeakedNewSubmittedPassword) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(true));
  // Check for password equality.
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword);
  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowCompleted(GURL(kOrigin), kUsername,
                                    PasswordChangeSuccessTracker::EndEvent::
                                        kAutomatedFlowOwnPasswordChosen));

  // Check for leaked credentials.
  EXPECT_CALL(mock_website_login_manager_,
              CheckWhetherSubmittedCredentialIsLeaked)
      .WillOnce(RunOnceCallback<0>(
          LeakDetectionStatus(LeakDetectionStatusCode::SUCCESS), true));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       false)),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(
                  &SaveSubmittedPasswordProto::Result::used_leaked_credential,
                  true))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest, SaveSubmittedPasswordLeakError) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(true));
  // Check for password equality.
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword);
  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowCompleted(GURL(kOrigin), kUsername,
                                    PasswordChangeSuccessTracker::EndEvent::
                                        kAutomatedFlowOwnPasswordChosen));

  // Check for leaked credentials.
  EXPECT_CALL(mock_website_login_manager_,
              CheckWhetherSubmittedCredentialIsLeaked)
      .WillOnce(RunOnceCallback<0>(
          LeakDetectionStatus(LeakDetectionStatusCode::INCOGNITO_MODE), false));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       false)),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(
                  &SaveSubmittedPasswordProto::Result::used_leaked_credential,
                  false))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest,
       SaveSubmittedPasswordNoLeakCheckSpecified) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  // Prevent a leak warning.
  proto_.mutable_save_submitted_password()->clear_leak_detection_timeout_ms();

  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(true));
  // Check for password equality.
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword);
  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowCompleted(GURL(kOrigin), kUsername,
                                    PasswordChangeSuccessTracker::EndEvent::
                                        kAutomatedFlowOwnPasswordChosen));

  // Since no timeout was submitted in the action, no leak check is performed.
  EXPECT_CALL(mock_website_login_manager_,
              CheckWhetherSubmittedCredentialIsLeaked)
      .Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(&SaveSubmittedPasswordProto::Result::used_same_password,
                       false)),
          Property(
              &ProcessedActionProto::save_submitted_password_result,
              Property(
                  &SaveSubmittedPasswordProto::Result::used_leaked_credential,
                  false))))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest,
       ReadyToSaveSubmittedPasswordPreconditionFails) {
  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword).Times(0);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  Run();
}

TEST_F(SaveSubmittedPasswordActionTest,
       SaveSubmittedPasswordAlwaysReturnResult) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  // Prevent a leak warning.
  proto_.mutable_save_submitted_password()->clear_leak_detection_timeout_ms();

  EXPECT_CALL(mock_website_login_manager_, ReadyToSaveSubmittedPassword)
      .WillOnce(Return(true));
  // Check for password equality.
  EXPECT_CALL(mock_website_login_manager_, SubmittedPasswordIsSame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_website_login_manager_, SaveSubmittedPassword);
  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowCompleted(GURL(kOrigin), kUsername,
                                    PasswordChangeSuccessTracker::EndEvent::
                                        kAutomatedFlowOwnPasswordChosen));

  // Since no timeout was submitted in the action, no leak check is performed.
  EXPECT_CALL(mock_website_login_manager_,
              CheckWhetherSubmittedCredentialIsLeaked)
      .Times(0);

  ProcessedActionProto processed_proto;
  EXPECT_CALL(callback_, Run).WillOnce(SaveArgPointee<0>(&processed_proto));

  Run();

  // We always expect to get a result, even if no leak check was performed
  // and the password is not the same.
  EXPECT_TRUE(processed_proto.has_save_submitted_password_result());
  EXPECT_TRUE(processed_proto.save_submitted_password_result()
                  .has_used_same_password());
  EXPECT_FALSE(
      processed_proto.save_submitted_password_result().used_same_password());
}

}  // namespace autofill_assistant
