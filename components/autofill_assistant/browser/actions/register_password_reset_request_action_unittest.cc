// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_password_reset_request_action.h"

#include <string>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using password_manager::MockPasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTracker;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;

const char kOrigin[] = "https://example.com";
const char kUsername[] = "username";

class RegisterPasswordResetRequestActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetPasswordChangeSuccessTracker)
        .WillByDefault(Return(&mock_password_change_success_tracker_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
  }

 protected:
  void Run() {
    proto_.mutable_register_password_reset_request();
    RegisterPasswordResetRequestAction action(&mock_action_delegate_, proto_);
    action.ProcessAction(callback_.Get());
  }

  ActionProto proto_;
  MockActionDelegate mock_action_delegate_;
  StrictMock<MockPasswordChangeSuccessTracker>
      mock_password_change_success_tracker_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  UserData user_data_;
};

TEST_F(RegisterPasswordResetRequestActionTest, RegisterResetSuccess) {
  user_data_.selected_login_.emplace(GURL(kOrigin), kUsername);

  EXPECT_CALL(
      mock_password_change_success_tracker_,
      OnChangePasswordFlowModified(
          GURL(kOrigin), kUsername,
          PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();
}

TEST_F(RegisterPasswordResetRequestActionTest, UserDataSelectLoginNotSet) {
  // Leave user_data_.select_login_ empty.
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  Run();
}

}  // namespace
}  // namespace autofill_assistant
