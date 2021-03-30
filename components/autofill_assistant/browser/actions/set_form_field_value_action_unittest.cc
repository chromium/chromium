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
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
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

class SetFormFieldValueActionTest : public content::RenderViewHostTestHarness {
 public:
  SetFormFieldValueActionTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SetFormFieldValueActionTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    set_form_field_proto_ = proto_.mutable_set_form_value();
    *set_form_field_proto_->mutable_element() = Selector({kFakeSelector}).proto;
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, WriteUserData)
        .WillByDefault(
            RunOnceCallback<0>(&user_data_, /* field_change = */ nullptr));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_website_login_manager_, OnGetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_web_controller_, GetFieldValue(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));
    test_util::MockFindAnyElement(mock_action_delegate_);
    ON_CALL(mock_web_controller_, SetValueAttribute(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_web_controller_, SelectFieldValue(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, WaitUntilDocumentIsInReadyState(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_web_controller_, ScrollIntoView(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_web_controller_, WaitUntilElementIsStable(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_action_delegate_, ClickOrTapElement(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_web_controller_, SendKeyboardInput(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus()));

    fake_selector_ = Selector({kFakeSelector});
  }

 protected:
  Selector fake_selector_;
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
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
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(kFakeUrl), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

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

TEST_F(SetFormFieldValueActionTest, UsernameToFill) {
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(kFakeUrl), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));
  ElementFinder::Result expected_element;
  expected_element.dom_object.object_data.object_id = "fake_object_id";
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute(kFakeUsername, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), kFakeUsername));

  auto* value = set_form_field_proto_->add_value();
  value->set_use_username(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, UsernameFillingFailsForMismatchingOrigin) {
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL("http://www.example.com/"), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL("http://not-real.com/"), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  auto* value = set_form_field_proto_->add_value();
  value->set_use_username(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PASSWORD_ORIGIN_MISMATCH))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, PasswordToFill) {
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(kFakeUrl), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinder::Result expected_element;
  expected_element.dom_object.object_data.object_id = "fake_object_id";
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute(kFakePassword, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), kFakePassword));

  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, PasswordFillingFailsForMismatchingOrigin) {
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL("http://www.example.com/"), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL("http://not-real.com/"), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PASSWORD_ORIGIN_MISMATCH))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, PasswordFillingSucceedsForSubdomain) {
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL("http://www.example.com/"), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL("http://login.example.com/"), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  ElementFinder::Result expected_element;
  expected_element.dom_object.object_data.object_id = "fake_object_id";
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute(kFakePassword, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), kFakePassword));

  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, PasswordIsClearedFromMemory) {
  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(kFakeUrl), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));

  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), kFakePassword));
  action.ProcessAction(callback_.Get());
  EXPECT_TRUE(action.field_inputs_.empty());
}

TEST_F(SetFormFieldValueActionTest, Keycode) {
  auto* value = set_form_field_proto_->add_value();
  value->set_keycode(13);  // carriage return
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(std::vector<int>{13}, _,
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
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode(keyboard_input), _,
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
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(mock_web_controller_,
              ScrollIntoView(true, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::CLICK, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode(keyboard_input), _,
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

  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("SomeText𠜎", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "SomeText𠜎"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, TextWithKeystrokeHasExpectedCallChain) {
  InSequence sequence;

  auto* value = set_form_field_proto_->add_value();
  std::string keyboard_input = "SomeQuery";
  value->set_text(keyboard_input);
  set_form_field_proto_->set_fill_strategy(SIMULATE_KEY_PRESSES);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(fake_selector_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(mock_web_controller_,
              ScrollIntoView(true, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::CLICK, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode(keyboard_input), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest,
       TextWithKeystrokeAndSelectHasExpectedCallChain) {
  InSequence sequence;

  auto* value = set_form_field_proto_->add_value();
  std::string keyboard_input = "SomeQuery";
  value->set_text(keyboard_input);
  set_form_field_proto_->set_fill_strategy(SIMULATE_KEY_PRESSES_SELECT_VALUE);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(fake_selector_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_web_controller_,
              SelectFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode(keyboard_input), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

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
  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode("SomeText"), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  // The second entry, a deprecated keycode is transformed into a
  // field_input.keyboard_input.
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(std::vector<int>{13}, _,
                                EqualsElement(expected_element), _))
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

  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("SomeText𠜎", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "SomeText𠜎"));

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

TEST_F(SetFormFieldValueActionTest, FallbackToSimulateKeystrokes) {
  InSequence sequence;

  auto* value = set_form_field_proto_->add_value();
  value->set_text("123");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("123", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode("123"), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::set_form_field_value_result,
                           Property(&SetFormFieldValueProto::Result::
                                        fallback_to_simulate_key_presses,
                                    true))))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, FallbackForPassword) {
  InSequence sequence;

  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL(kFakeUrl), kFakeUsername);
  EXPECT_CALL(mock_action_delegate_, FindElement(fake_selector_, _))
      .WillOnce(testing::WithArgs<1>([this](auto&& callback) {
        auto element_result = std::make_unique<ElementFinder::Result>();
        element_result->dom_object.object_data.object_id = "fake_object_id";
        content::NavigationSimulator::NavigateAndCommitFromDocument(
            GURL(kFakeUrl), web_contents()->GetMainFrame());
        element_result->container_frame_host = web_contents()->GetMainFrame();
        std::move(callback).Run(OkClientStatus(), std::move(element_result));
      }));
  ElementFinder::Result expected_element;
  expected_element.dom_object.object_data.object_id = "fake_object_id";

  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute(kFakePassword, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode(kFakePassword), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  auto* value = set_form_field_proto_->add_value();
  value->set_use_password(true);
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::set_form_field_value_result,
                           Property(&SetFormFieldValueProto::Result::
                                        fallback_to_simulate_key_presses,
                                    true))))));
  action.ProcessAction(callback_.Get());
}

TEST_F(SetFormFieldValueActionTest, FallbackForMultipleValues) {
  InSequence sequence;

  auto* value = set_form_field_proto_->add_value();
  value->set_text("SomeText");
  auto* enter = set_form_field_proto_->add_value();
  enter->set_text("SomeOtherText");
  SetFormFieldValueAction action(&mock_action_delegate_, proto_);

  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);

  // First value.
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("SomeText", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode("SomeText"), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  // Second value.
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("SomeOtherText", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode("SomeOtherText"), _,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::set_form_field_value_result,
                           Property(&SetFormFieldValueProto::Result::
                                        fallback_to_simulate_key_presses,
                                    true))))));
  action.ProcessAction(callback_.Get());
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

  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, fake_selector_);
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("John", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  action.ProcessAction(callback_.Get());
}

}  // namespace autofill_assistant
