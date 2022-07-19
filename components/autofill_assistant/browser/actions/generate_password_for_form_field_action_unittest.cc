// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/generate_password_for_form_field_action.h"

#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeSelector[] = "#some_selector";
const char kFakeUsername[] = "user@example.com";
const char kGeneratedPassword[] = "m-W2b-_.7Fu9A.A";
const char kMemoryKeyForGeneratedPassword[] = "memory-key-for-generation";
}  // namespace

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class GeneratePasswordForFormFieldActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, WriteUserData)
        .WillByDefault(
            RunOnceCallback<0>(&user_data_, /* field_change = */ nullptr));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));

    ON_CALL(mock_website_login_manager_, GeneratePassword)
        .WillByDefault(Return(kGeneratedPassword));

    user_data_.selected_login_ =
        absl::make_optional<WebsiteLoginManager::Login>(GURL(kFakeUrl),
                                                        kFakeUsername);
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  UserData user_data_;
};

TEST_F(GeneratePasswordForFormFieldActionTest, GeneratedPassword) {
  ON_CALL(mock_action_delegate_, RetrieveElementFormAndFieldData)
      .WillByDefault(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                        autofill::FormData(),
                                        autofill::FormFieldData()));
  GeneratePasswordForFormFieldProto* generate_password_proto =
      proto_.mutable_generate_password_for_form_field();
  *generate_password_proto->mutable_element() = Selector({kFakeSelector}).proto;
  generate_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  GeneratePasswordForFormFieldAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  action.ProcessAction(callback_.Get());
  EXPECT_EQ(kGeneratedPassword,
            user_data_.GetAdditionalValue(kMemoryKeyForGeneratedPassword)
                ->strings()
                .values(0));
  EXPECT_TRUE(user_data_.GetAdditionalValue(kMemoryKeyForGeneratedPassword)
                  ->is_client_side_only());
}

TEST_F(GeneratePasswordForFormFieldActionTest, FormDataIsNotRetrieved) {
  ON_CALL(mock_action_delegate_, RetrieveElementFormAndFieldData)
      .WillByDefault(RunOnceCallback<1>(ClientStatus(INVALID_SELECTOR),
                                        autofill::FormData(),
                                        autofill::FormFieldData()));

  GeneratePasswordForFormFieldProto* generate_password_proto =
      proto_.mutable_generate_password_for_form_field();
  *generate_password_proto->mutable_element() =
      Selector({kFakeSelector}).MustBeVisible().proto;

  GeneratePasswordForFormFieldAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_website_login_manager_, GeneratePassword).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));

  action.ProcessAction(callback_.Get());

  EXPECT_FALSE(user_data_.HasAdditionalValue(kMemoryKeyForGeneratedPassword));
}

TEST_F(GeneratePasswordForFormFieldActionTest, GeneratePasswordFails) {
  ON_CALL(mock_action_delegate_, RetrieveElementFormAndFieldData)
      .WillByDefault(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED),
                                        autofill::FormData(),
                                        autofill::FormFieldData()));
  GeneratePasswordForFormFieldProto* generate_password_proto =
      proto_.mutable_generate_password_for_form_field();
  *generate_password_proto->mutable_element() = Selector({kFakeSelector}).proto;
  generate_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  GeneratePasswordForFormFieldAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_website_login_manager_, GeneratePassword)
      .WillOnce(Return(absl::nullopt));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, NO_RENDER_FRAME))));

  action.ProcessAction(callback_.Get());

  EXPECT_FALSE(user_data_.HasAdditionalValue(kMemoryKeyForGeneratedPassword));
}

}  // namespace autofill_assistant
