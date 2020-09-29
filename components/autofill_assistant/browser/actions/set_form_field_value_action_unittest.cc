// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

const char kFakeUrl[] = "https://www.example.com";
const char kFakeSelector[] = "#some_selector";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "example_password";

}  // namespace

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithArgs;

class SetFormFieldValueActionTest : public testing::Test {
 public:
  void SetUp() override {
    set_form_field_proto_ = proto_.mutable_set_form_value();
    *set_form_field_proto_->mutable_element() =
        Selector({kFakeSelector}).MustBeVisible().proto;
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, WriteUserData)
        .WillByDefault(
            RunOnceCallback<0>(&user_data_, /* field_change = */ nullptr));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _, _, _))
        .WillByDefault(RunOnceCallback<4>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, FindElement(_, _))
        .WillByDefault(WithArgs<1>([&](auto&& callback) {
          auto element_result = std::make_unique<ElementFinder::Result>();
          std::move(callback).Run(OkClientStatus(), std::move(element_result));
        }));
    ON_CALL(mock_action_delegate_, WaitForDocumentToBecomeInteractive(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, ScrollIntoView(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, ClickOrTapElement(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));

    ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
                WebsiteLoginManager::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_website_login_manager_, OnGetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));
    user_data_.selected_login_ =
        base::make_optional<WebsiteLoginManager::Login>(GURL(kFakeUrl),
                                                        kFakeUsername);
    fake_selector_ = Selector({kFakeSelector}).MustBeVisible();
  }

 protected:
  Selector fake_selector_;
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  SetFormFieldValueProto* set_form_field_proto_;
  UserData user_data_;
};

TEST_F(SetFormFieldValueActionTest, RequestedUsernameButNoLoginInClientMemory) {
  UserData empty_user_data;
  ON_CALL(mock_action_delegate_, GetUserData)
      .WillByDefault(Return(&empty_user_data));
  auto* value = set_form_field_proto_->add_value();
  value->set_use_username(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, RequestedPasswordButNoLoginInClientMemory) {
  UserData empty_user_data;
  ON_CALL(mock_action_delegate_, GetUserData)
      .WillByDefault(Return(&empty_user_data));
  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, RequestedPasswordButPasswordNotAvailable) {
  ON_CALL(mock_website_login_manager_, OnGetPasswordForLogin(_, _))
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
              OnSetFieldValue(kFakeUsername, _, _,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, fake_selector_)),
                              _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, PasswordToFill) {
  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), kFakePassword));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(kFakePassword, _, _,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, fake_selector_)),
                              _))
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
              OnSendKeyboardInput(std::vector<int>{13}, _,
                                  EqualsElement(test_util::MockFindElement(
                                      mock_action_delegate_, fake_selector_)),
                                  _))
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
  EXPECT_CALL(mock_action_delegate_,
              OnSendKeyboardInput(UTF8ToUnicode(keyboard_input), _,
                                  EqualsElement(test_util::MockFindElement(
                                      mock_action_delegate_, fake_selector_)),
                                  _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, KeyboardInputHasExpectedCallChain) {
  InSequence sequence;

  auto* value = set_form_field_proto_->add_value();
  std::string keyboard_input = "SomeQuery";
  value->set_keyboard_input(keyboard_input);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(fake_selector_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_action_delegate_, WaitForDocumentToBecomeInteractive(
                                         EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_,
              ScrollIntoView(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::CLICK, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_,
              OnSendKeyboardInput(UTF8ToUnicode(keyboard_input), _,
                                  EqualsElement(expected_element), _))
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
              OnSetFieldValue("SomeText𠜎", _, _,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, fake_selector_)),
                              _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, MultipleValuesAndSimulateKeypress) {
  auto* value = set_form_field_proto_->add_value();
  value->set_text("SomeText");
  auto* enter = set_form_field_proto_->add_value();
  enter->set_keycode(13);
  set_form_field_proto_->set_fill_strategy(SIMULATE_KEY_PRESSES);

  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(
      mock_action_delegate_,
      OnSetFieldValue("SomeText", /* simulate_key_presses = */ true, _, _, _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
  // The second entry, a deprecated keycode is transformed into a
  // field_input.keyboard_input.
  EXPECT_CALL(mock_action_delegate_,
              OnSendKeyboardInput(std::vector<int>{13}, _, _, _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, ClientMemoryKey) {
  auto* value = set_form_field_proto_->add_value();
  value->set_client_memory_key("key");
  ValueProto value_proto;
  value_proto.mutable_strings()->add_values("SomeText𠜎");
  user_data_.additional_values_["key"] = value_proto;
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "SomeText𠜎"));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue("SomeText𠜎", _, _,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, fake_selector_)),
                              _))
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
                OnSetFieldValue("123",
                                /* simulate_key_presses = */ false, _,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, fake_selector_)),
                                _));
    EXPECT_CALL(mock_action_delegate_,
                OnSetFieldValue("123",
                                /* simulate_key_presses = */ true, _,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, fake_selector_)),
                                _));
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

TEST_F(SetFormFieldValueActionTest, EmptyProfileValueFails) {
  set_form_field_proto_->add_value()->mutable_autofill_value();
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, RequestDataFromUnknownProfile) {
  auto* value = set_form_field_proto_->add_value()->mutable_autofill_value();
  value->mutable_profile()->set_identifier("none");
  value->set_value_expression("value");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, RequestUnknownDataFromProfile) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  // Middle name is expected to be empty.
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_data_.selected_addresses_["contact"] =
      std::make_unique<autofill::AutofillProfile>(contact);

  auto* value = set_form_field_proto_->add_value()->mutable_autofill_value();
  value->mutable_profile()->set_identifier("contact");
  value->set_value_expression(
      base::StrCat({"${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_MIDDLE)),
                    "}"}));
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              AUTOFILL_INFO_NOT_AVAILABLE))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, SetFieldFromProfileValue) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", "", "Doe", "", "", "", "",
                                 "", "", "", "", "");
  user_data_.selected_addresses_["contact"] =
      std::make_unique<autofill::AutofillProfile>(contact);

  auto* value = set_form_field_proto_->add_value()->mutable_autofill_value();
  value->mutable_profile()->set_identifier("contact");
  value->set_value_expression(
      base::StrCat({"${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_FIRST)),
                    "}"}));
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  ON_CALL(mock_action_delegate_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue("John", _, _,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, fake_selector_)),
                              _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

}  // namespace autofill_assistant
