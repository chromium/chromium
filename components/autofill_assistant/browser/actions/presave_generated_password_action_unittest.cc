// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/presave_generated_password_action.h"

#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";
const char kGeneratedPassword[] = "m-W2b-_.7Fu9A.A";
const char kMemoryKeyForGeneratedPassword[] = "memory-key-for-generation";
}  // namespace

namespace autofill_assistant {

using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class PresaveGeneratedPasswordActionTest : public testing::Test {
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

TEST_F(PresaveGeneratedPasswordActionTest, PresaveGeneratedPassword) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  user_data_.SetAdditionalValue(kMemoryKeyForGeneratedPassword,
                                SimpleValue(std::string(kGeneratedPassword)));
  user_data_.password_form_data_ = autofill::FormData();

  PresaveGeneratedPasswordProto* presave_password_proto =
      proto_.mutable_presave_generated_password();
  presave_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  PresaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_website_login_manager_,
              PresaveGeneratedPassword(_, kGeneratedPassword, _, _))
      .Times(1);

  action.ProcessAction(callback_.Get());
}

TEST_F(PresaveGeneratedPasswordActionTest, LoginDataMissing) {
  user_data_.SetAdditionalValue(kMemoryKeyForGeneratedPassword,
                                SimpleValue(std::string(kGeneratedPassword)));
  user_data_.password_form_data_ = autofill::FormData();

  PresaveGeneratedPasswordProto* presave_password_proto =
      proto_.mutable_presave_generated_password();
  presave_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  PresaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(PresaveGeneratedPasswordActionTest, GeneratedPasswordMissing) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  user_data_.password_form_data_ = autofill::FormData();

  PresaveGeneratedPasswordProto* presave_password_proto =
      proto_.mutable_presave_generated_password();
  presave_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  PresaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(PresaveGeneratedPasswordActionTest, FormDataMissing) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  user_data_.SetAdditionalValue(kMemoryKeyForGeneratedPassword,
                                SimpleValue(std::string(kGeneratedPassword)));

  PresaveGeneratedPasswordProto* presave_password_proto =
      proto_.mutable_presave_generated_password();
  presave_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  PresaveGeneratedPasswordAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  action.ProcessAction(callback_.Get());
}
}  // namespace autofill_assistant
