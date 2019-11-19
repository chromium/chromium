// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_fetcher.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeSelector[] = "#some_selector";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "example_password";
}  // namespace

namespace autofill_assistant {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class SetFormFieldValueActionTest : public testing::Test {
 public:
  void SetUp() override {
    set_form_field_proto_ = proto_.mutable_set_form_value();
    set_form_field_proto_->mutable_element()->add_selectors(kFakeSelector);
    set_form_field_proto_->mutable_element()->set_visibility_requirement(
        MUST_BE_VISIBLE);
    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&client_memory_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginFetcher)
        .WillByDefault(Return(&mock_website_login_fetcher_));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _, _, _))
        .WillByDefault(RunOnceCallback<4>(OkClientStatus()));

    ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{
                WebsiteLoginFetcher::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));
    client_memory_.set_selected_login({GURL(kFakeUrl), kFakeUsername});
    fake_selector_ = Selector({kFakeSelector}).MustBeVisible();
  }

 protected:
  Selector fake_selector_;
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginFetcher mock_website_login_fetcher_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  SetFormFieldValueProto* set_form_field_proto_;
  ClientMemory client_memory_;
};

TEST_F(SetFormFieldValueActionTest, RequestedUsernameButNoLoginInClientMemory) {
  ClientMemory empty_client_memory;
  ON_CALL(mock_action_delegate_, GetClientMemory)
      .WillByDefault(Return(&empty_client_memory));
  auto* value = set_form_field_proto_->add_value();
  value->set_use_username(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, RequestedPasswordButNoLoginInClientMemory) {
  ClientMemory empty_client_memory;
  ON_CALL(mock_action_delegate_, GetClientMemory)
      .WillByDefault(Return(&empty_client_memory));
  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, RequestedPasswordButPasswordNotAvailable) {
  ON_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
      .WillByDefault(RunOnceCallback<1>(false, std::string()));
  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              AUTOFILL_INFO_NOT_AVAILABLE))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, NonAsciiKeycode) {
  auto* value = set_form_field_proto_->add_value();
  value->set_keycode(UTF8ToUnicode("𠜎")[0]);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, Username) {
  auto* value = set_form_field_proto_->add_value();
  value->set_use_username(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), kFakeUsername));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(fake_selector_, kFakeUsername, _, _, _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, Password) {
  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), kFakePassword));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(fake_selector_, kFakePassword, _, _, _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, Keycode) {
  auto* value = set_form_field_proto_->add_value();
  value->set_keycode(13);  // carriage return
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(mock_action_delegate_,
              OnSendKeyboardInput(fake_selector_, std::vector<int>{13}, _, _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, KeyboardInput) {
  auto* value = set_form_field_proto_->add_value();
  std::string keyboard_input = "SomeQuery𠜎\r";
  value->set_keyboard_input(keyboard_input);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(
      mock_action_delegate_,
      OnSendKeyboardInput(fake_selector_, UTF8ToUnicode(keyboard_input), _, _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, Text) {
  auto* value = set_form_field_proto_->add_value();
  value->set_text("SomeText𠜎");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "SomeText𠜎"));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(fake_selector_, "SomeText𠜎", _, _, _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, ClientMemoryKey) {
  auto* value = set_form_field_proto_->add_value();
  value->set_client_memory_key("key");
  client_memory_.set_additional_value("key", "SomeText𠜎");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "SomeText𠜎"));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(fake_selector_, "SomeText𠜎", _, _, _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, ClientMemoryKeyFailsIfNotInClientMemory) {
  auto* value = set_form_field_proto_->add_value();
  value->set_client_memory_key("key");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

// Test that automatic fallback to simulate keystrokes works.
TEST_F(SetFormFieldValueActionTest, Fallback) {
  auto* value = set_form_field_proto_->add_value();
  value->set_text("123");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  {
    InSequence seq;
    EXPECT_CALL(mock_action_delegate_,
                OnSetFieldValue(fake_selector_, "123",
                                /* simulate_key_presses = */ false, _, _));
    EXPECT_CALL(mock_action_delegate_,
                OnSetFieldValue(fake_selector_, "123",
                                /* simulate_key_presses = */ true, _, _));
  }

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::set_form_field_value_result,
                           Property(&SetFormFieldValueProto::Result::
                                        fallback_to_simulate_key_presses,
                                    true))))));

  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, PasswordIsClearedFromMemory) {
  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), kFakePassword));
  action.ProcessAction(callback_.Get());
  EXPECT_TRUE(action.field_inputs_.empty());
}

}  // namespace autofill_assistant
