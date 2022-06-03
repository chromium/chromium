// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/edit_password_action.h"

#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";
const char kMemoryKey[] = "edited-password";
const char kPassword[] = "new-pass";
}  // namespace

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class EditPasswordActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  UserData user_data_;
};

TEST_F(EditPasswordActionTest, EditPasswordNoMemoryKey) {
  EditPasswordAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  action.ProcessAction(callback_.Get());
}

TEST_F(EditPasswordActionTest, EditPasswordNoSelectedLoginUserData) {
  EditSelectedPasswordProto* edit_password = proto_.mutable_edit_password();
  edit_password->set_memory_key(kMemoryKey);
  EditPasswordAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(EditPasswordActionTest, EditPasswordNoPasswordInUserData) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  EditSelectedPasswordProto* edit_password = proto_.mutable_edit_password();
  edit_password->set_memory_key(kMemoryKey);
  EditPasswordAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(EditPasswordActionTest, EditPasswordSuccess) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  user_data_.SetAdditionalValue(kMemoryKey,
                                SimpleValue(std::string(kPassword)));
  EditSelectedPasswordProto* edit_password = proto_.mutable_edit_password();
  edit_password->set_memory_key(kMemoryKey);
  EditPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_website_login_manager_, EditPasswordForLogin)
      .WillOnce(RunOnceCallback<2>(true));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(EditPasswordActionTest, EditPasswordNoLoginAvailable) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  user_data_.SetAdditionalValue(kMemoryKey,
                                SimpleValue(std::string(kPassword)));
  EditSelectedPasswordProto* edit_password = proto_.mutable_edit_password();
  edit_password->set_memory_key(kMemoryKey);
  EditPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_website_login_manager_, EditPasswordForLogin)
      .WillOnce(RunOnceCallback<2>(false));
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              AUTOFILL_INFO_NOT_AVAILABLE))));

  action.ProcessAction(callback_.Get());
}

}  // namespace autofill_assistant
